#include "AudioWorker.h"

#include "win/ComPtr.h"
#include "win/Hr.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QMutex>

#include <unordered_map>

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <propvarutil.h>
#include <functiondiscoverykeys_devpkey.h>

// MinGW headers sometimes only forward-declare IAudioMeterInformation.
// Define the minimal interface here so we can call GetPeakValue for per-session meters.
struct IAudioMeterInformation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *peak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *channelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT channelCount, float *peakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *hardwareSupportMask) = 0;
};

static const IID IID_IAudioMeterInformation = {0xc02216f6, 0x8c67, 0x4b5b, {0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64}};

static QString deviceFriendlyName(IMMDevice *device)
{
    if (!device)
        return {};

    ComPtr<IPropertyStore> props;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, props.put());
    if (FAILED(hr))
        return {};

    // MinGW headers can declare PKEY_Device_FriendlyName as extern without providing a definition.
    // Use the literal PROPERTYKEY instead (FMTID {A45C254E-DF1C-4EFD-8020-67D146A850E0}, PID 14).
    static const PROPERTYKEY kPkeyDeviceFriendlyName = {
        {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
        14
    };

    PROPVARIANT v;
    PropVariantInit(&v);
    hr = props->GetValue(kPkeyDeviceFriendlyName, &v);
    if (FAILED(hr)) {
        PropVariantClear(&v);
        return {};
    }
    QString out = (v.vt == VT_LPWSTR && v.pwszVal) ? QString::fromWCharArray(v.pwszVal) : QString();
    PropVariantClear(&v);
    return out;
}

static QString deviceId(IMMDevice *device)
{
    if (!device)
        return {};
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || !id)
        return {};
    QString out = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return out;
}

static QString exePathForPid(DWORD pid)
{
    if (pid == 0)
        return {};
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return {};
    wchar_t buf[MAX_PATH * 4];
    DWORD len = static_cast<DWORD>(std::size(buf));
    QString out;
    if (QueryFullProcessImageNameW(h, 0, buf, &len) && len > 0) {
        out = QString::fromWCharArray(buf, static_cast<int>(len));
    }
    CloseHandle(h);
    return out;
}

static bool isLikelySystemSession(const QString &exePath)
{
    const QString lower = exePath.toLower();
    if (lower.isEmpty())
        return true;
    if (lower.endsWith(QStringLiteral("\\audiodg.exe")))
        return true;
    if (lower.endsWith(QStringLiteral("\\svchost.exe")))
        return true;
    return false;
}

struct AudioWorker::Impl
{
    HRESULT comHr = E_FAIL;
    ComPtr<IMMDeviceEnumerator> enumerator;

    // Device id -> endpoint/session manager
    struct SessionKey {
        QString deviceId;
        quint32 pid = 0;
        QString exePath;
        bool operator==(const SessionKey &o) const { return deviceId == o.deviceId && pid == o.pid && exePath == o.exePath; }
    };
    struct SessionKeyHash {
        size_t operator()(const SessionKey &k) const noexcept
        {
            return qHash(k.deviceId) ^ (qHash(k.exePath) << 1) ^ (static_cast<size_t>(k.pid) << 16);
        }
    };

    struct SessionCom {
        ComPtr<IAudioSessionControl> ctrl;
        ComPtr<IAudioSessionControl2> ctrl2;
        ComPtr<ISimpleAudioVolume> simple;
        ComPtr<IAudioMeterInformation> meter;
        IAudioSessionEvents *events = nullptr; // owned via COM refcount
        qint64 lastActiveMs = 0;
        AudioSessionState state = AudioSessionStateInactive;
    };

    struct DeviceCom {
        ComPtr<IMMDevice> device;
        ComPtr<IAudioEndpointVolume> endpoint;
        ComPtr<IAudioSessionManager2> sessionMgr;
    };

    struct QStringHash {
        size_t operator()(const QString &s) const noexcept { return static_cast<size_t>(qHash(s)); }
    };

    std::unordered_map<QString, DeviceCom, QStringHash> devices; // deviceId -> com
    std::unordered_map<SessionKey, SessionCom, SessionKeyHash> sessions;
    QHash<QString, qint64> lastActiveByKeyStr; // stable grace tracking across rebuilds

    // Callbacks
    class NotificationClient final : public IMMNotificationClient
    {
    public:
        explicit NotificationClient(AudioWorker *w)
            : m_worker(w)
        {
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG v = --m_ref;
            if (v == 0)
                delete this;
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
        {
            if (!ppv)
                return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
                *ppv = static_cast<IMMNotificationClient *>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { ping(); return S_OK; }

    private:
        void ping()
        {
            if (m_worker)
                QMetaObject::invokeMethod(m_worker, [this]() { m_worker->scheduleSnapshot(); }, Qt::QueuedConnection);
        }

        std::atomic<ULONG> m_ref{1};
        AudioWorker *m_worker = nullptr;
    };

    class EndpointCallback final : public IAudioEndpointVolumeCallback
    {
    public:
        explicit EndpointCallback(AudioWorker *w)
            : m_worker(w)
        {
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG v = --m_ref;
            if (v == 0)
                delete this;
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
        {
            if (!ppv)
                return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioEndpointVolumeCallback)) {
                *ppv = static_cast<IAudioEndpointVolumeCallback *>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override
        {
            if (m_worker)
                QMetaObject::invokeMethod(m_worker, [this]() { m_worker->scheduleSnapshot(); }, Qt::QueuedConnection);
            return S_OK;
        }

    private:
        std::atomic<ULONG> m_ref{1};
        AudioWorker *m_worker = nullptr;
    };

    class SessionEvents final : public IAudioSessionEvents
    {
    public:
        SessionEvents(AudioWorker *w, const SessionKey &k)
            : m_worker(w)
            , m_key(k)
        {
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG v = --m_ref;
            if (v == 0)
                delete this;
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
        {
            if (!ppv)
                return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioSessionEvents)) {
                *ppv = static_cast<IAudioSessionEvents *>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR, LPCGUID) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR, LPCGUID) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float, BOOL, LPCGUID) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID, LPCGUID) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState) override { ping(); return S_OK; }
        HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason) override { ping(); return S_OK; }

    private:
        void ping()
        {
            if (m_worker)
                QMetaObject::invokeMethod(m_worker, [this]() { m_worker->scheduleSnapshot(); }, Qt::QueuedConnection);
        }

        std::atomic<ULONG> m_ref{1};
        AudioWorker *m_worker = nullptr;
        SessionKey m_key;
    };

    class SessionNotification final : public IAudioSessionNotification
    {
    public:
        explicit SessionNotification(AudioWorker *w)
            : m_worker(w)
        {
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG v = --m_ref;
            if (v == 0)
                delete this;
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
        {
            if (!ppv)
                return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioSessionNotification)) {
                *ppv = static_cast<IAudioSessionNotification *>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl *) override
        {
            if (m_worker)
                QMetaObject::invokeMethod(m_worker, [this]() { m_worker->scheduleSnapshot(); }, Qt::QueuedConnection);
            return S_OK;
        }

    private:
        std::atomic<ULONG> m_ref{1};
        AudioWorker *m_worker = nullptr;
    };

    ComPtr<IMMNotificationClient> notifyClient;
    ComPtr<IAudioEndpointVolumeCallback> endpointCb;
    ComPtr<IAudioSessionNotification> sessionCb;

    QTimer peakPollTimer;

    HRESULT init(AudioWorker *worker)
    {
        // IMPORTANT: CoInitializeEx must run on the thread that will use COM (this worker thread),
        // not on the GUI thread.
        comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(comHr) && comHr != RPC_E_CHANGED_MODE)
            return comHr;

        HR_RET(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(enumerator.put())));

        notifyClient.attach(new NotificationClient(worker));
        HR_RET(enumerator->RegisterEndpointNotificationCallback(notifyClient.get()));

        endpointCb.attach(new EndpointCallback(worker));
        sessionCb.attach(new SessionNotification(worker));

        return S_OK;
    }

    void shutdown()
    {
        // Best-effort unregistration.
        for (auto &kv : devices) {
            auto &dc = kv.second;
            if (dc.endpoint && endpointCb) {
                dc.endpoint->UnregisterControlChangeNotify(endpointCb.get());
            }
            if (dc.sessionMgr && sessionCb) {
                dc.sessionMgr->UnregisterSessionNotification(sessionCb.get());
            }
        }

        for (auto &kv : sessions) {
            auto &sc = kv.second;
            if (sc.ctrl && sc.events) {
                sc.ctrl->UnregisterAudioSessionNotification(sc.events);
            }
            if (sc.events) {
                sc.events->Release();
                sc.events = nullptr;
            }
        }

        if (enumerator && notifyClient) {
            enumerator->UnregisterEndpointNotificationCallback(notifyClient.get());
        }
        devices.clear();
        sessions.clear();
        enumerator.reset();
        notifyClient.reset();
        endpointCb.reset();
        sessionCb.reset();

        if (comHr == S_OK || comHr == S_FALSE) {
            CoUninitialize();
        }
        comHr = E_FAIL;
    }
};

AudioWorker::AudioWorker(QObject *parent)
    : QObject(parent)
{
    m = new Impl();

    // Ensure timers move with this object when we moveToThread().
    m_snapshotTimer.setParent(this);
    m->peakPollTimer.setParent(this);
    m_meterTimer.setParent(this);

    m_snapshotTimer.setSingleShot(true);
    m_snapshotTimer.setInterval(16);
    connect(&m_snapshotTimer, &QTimer::timeout, this, &AudioWorker::emitSnapshotNow);

    // Slow periodic refresh as a safety net (structure changes are still event-driven).
    m->peakPollTimer.setInterval(250);
    connect(&m->peakPollTimer, &QTimer::timeout, this, [this]() { scheduleSnapshot(); });

    // Per-session peak meters for EarTrumpet-like activity line.
    m_meterTimer.setInterval(50);
    connect(&m_meterTimer, &QTimer::timeout, this, &AudioWorker::emitPeaksNow);
}

AudioWorker::~AudioWorker()
{
    stop();
    delete m;
    m = nullptr;
}

void AudioWorker::start()
{
    if (!m)
        return;

    const HRESULT hr = m->init(this);
    if (FAILED(hr)) {
        emit error(QStringLiteral("CoreAudio init failed: %1").arg(hrToString(hr)));
        return;
    }

    m->peakPollTimer.start();
    m_meterTimer.start();
    scheduleSnapshot();
}

void AudioWorker::stop()
{
    if (!m)
        return;
    m->peakPollTimer.stop();
    m_snapshotTimer.stop();
    m_meterTimer.stop();
    m->shutdown();
}

void AudioWorker::setShowSystemSessions(bool show)
{
    m_showSystemSessions = show;
    scheduleSnapshot();
}

void AudioWorker::setDeviceVolume(const QString &deviceId, double volume01)
{
    if (!m)
        return;
    auto it = m->devices.find(deviceId);
    if (it == m->devices.end() || !it->second.endpoint)
        return;
    it->second.endpoint->SetMasterVolumeLevelScalar(static_cast<float>(qBound(0.0, volume01, 1.0)), nullptr);
}

void AudioWorker::setDeviceMuted(const QString &deviceId, bool muted)
{
    if (!m)
        return;
    auto it = m->devices.find(deviceId);
    if (it == m->devices.end() || !it->second.endpoint)
        return;
    it->second.endpoint->SetMute(muted ? TRUE : FALSE, nullptr);
}

void AudioWorker::setSessionVolume(const QString &deviceId, quint32 pid, const QString &exePath, double volume01)
{
    if (!m)
        return;
    Impl::SessionKey key{deviceId, pid, exePath};
    auto it = m->sessions.find(key);
    if (it == m->sessions.end() || !it->second.simple)
        return;
    it->second.simple->SetMasterVolume(static_cast<float>(qBound(0.0, volume01, 1.0)), nullptr);
}

void AudioWorker::setSessionMuted(const QString &deviceId, quint32 pid, const QString &exePath, bool muted)
{
    if (!m)
        return;
    Impl::SessionKey key{deviceId, pid, exePath};
    auto it = m->sessions.find(key);
    if (it == m->sessions.end() || !it->second.simple)
        return;
    it->second.simple->SetMute(muted ? TRUE : FALSE, nullptr);
}

void AudioWorker::scheduleSnapshot()
{
    if (!m_snapshotTimer.isActive())
        m_snapshotTimer.start();
}

void AudioWorker::emitSnapshotNow()
{
    if (!m || !m->enumerator)
        return;

    // Refresh device list.
    ComPtr<IMMDeviceCollection> coll;
    HRESULT hr = m->enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, coll.put());
    if (FAILED(hr)) {
        emit error(QStringLiteral("EnumAudioEndpoints failed: %1").arg(hrToString(hr)));
        return;
    }

    UINT count = 0;
    coll->GetCount(&count);

    // Determine default device id.
    QString defaultId;
    {
        ComPtr<IMMDevice> def;
        if (SUCCEEDED(m->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, def.put()))) {
            defaultId = deviceId(def.get());
        }
    }

    // Build snapshot.
    QVector<DeviceState> devices;
    devices.reserve(static_cast<int>(count));

    // Rebuild COM caches each snapshot, but unregister previous callbacks to avoid dangling notifications.
    {
        for (auto &kv : m->devices) {
            auto &dc = kv.second;
            if (dc.endpoint && m->endpointCb) {
                dc.endpoint->UnregisterControlChangeNotify(m->endpointCb.get());
            }
            if (dc.sessionMgr && m->sessionCb) {
                dc.sessionMgr->UnregisterSessionNotification(m->sessionCb.get());
            }
        }
        for (auto &kv : m->sessions) {
            auto &sc = kv.second;
            if (sc.ctrl && sc.events) {
                sc.ctrl->UnregisterAudioSessionNotification(sc.events);
            }
            if (sc.events) {
                sc.events->Release();
                sc.events = nullptr;
            }
        }
        m->devices.clear();
        m->sessions.clear();
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (FAILED(coll->Item(i, dev.put())) || !dev)
            continue;

        const QString id = deviceId(dev.get());
        if (id.isEmpty())
            continue;

        DeviceState ds;
        ds.id = id;
        ds.name = deviceFriendlyName(dev.get());
        ds.isDefault = (id == defaultId);

        Impl::DeviceCom dc;
        dc.device.attach(dev.detach());

        // Endpoint volume.
        {
            ComPtr<IAudioEndpointVolume> ep;
            hr = dc.device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(ep.put()));
            if (SUCCEEDED(hr) && ep) {
                float vol = 1.0f;
                BOOL mute = FALSE;
                ep->GetMasterVolumeLevelScalar(&vol);
                ep->GetMute(&mute);
                ds.volume = vol;
                ds.muted = (mute == TRUE);
                ep->RegisterControlChangeNotify(m->endpointCb.get());
                dc.endpoint.attach(ep.detach());
            }
        }

        // Session manager + sessions.
        {
            ComPtr<IAudioSessionManager2> mgr;
            hr = dc.device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(mgr.put()));
            if (SUCCEEDED(hr) && mgr) {
                mgr->RegisterSessionNotification(m->sessionCb.get());
                dc.sessionMgr.attach(mgr.detach());

                ComPtr<IAudioSessionEnumerator> se;
                if (SUCCEEDED(dc.sessionMgr->GetSessionEnumerator(se.put())) && se) {
                    int scount = 0;
                    se->GetCount(&scount);
                    for (int si = 0; si < scount; ++si) {
                        ComPtr<IAudioSessionControl> ctrl;
                        if (FAILED(se->GetSession(si, ctrl.put())) || !ctrl)
                            continue;

                        ComPtr<IAudioSessionControl2> ctrl2;
                        if (FAILED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void **>(ctrl2.put()))) || !ctrl2)
                            continue;

                        DWORD pid = 0;
                        ctrl2->GetProcessId(&pid);
                        if (pid == 0)
                            continue;

                        const QString exe = exePathForPid(pid);
                        if (!m_showSystemSessions && isLikelySystemSession(exe))
                            continue;

                        // Volume/mute.
                        ComPtr<ISimpleAudioVolume> simple;
                        if (FAILED(ctrl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void **>(simple.put()))) || !simple)
                            continue;

                        float sVol = 1.0f;
                        BOOL sMute = FALSE;
                        simple->GetMasterVolume(&sVol);
                        simple->GetMute(&sMute);

                        // Display name.
                        LPWSTR dname = nullptr;
                        QString display;
                        if (SUCCEEDED(ctrl->GetDisplayName(&dname)) && dname) {
                            display = QString::fromWCharArray(dname).trimmed();
                            CoTaskMemFree(dname);
                        }
                        if (display.isEmpty())
                            display = QFileInfo(exe).baseName();

                        // State.
                        AudioSessionState st = AudioSessionStateInactive;
                        ctrl->GetState(&st);

                        const QString keyStr = id + QLatin1Char('|') + QString::number(pid) + QLatin1Char('|') + exe;
                        qint64 lastActive = m->lastActiveByKeyStr.value(keyStr, 0);
                        if (st == AudioSessionStateActive) {
                            lastActive = nowMs;
                            m->lastActiveByKeyStr.insert(keyStr, lastActive);
                        }

                        SessionState ss;
                        ss.deviceId = id;
                        ss.pid = static_cast<quint32>(pid);
                        ss.exePath = exe;
                        ss.displayName = display;
                        ss.iconKey = exe;
                        ss.volume = sVol;
                        ss.muted = (sMute == TRUE);
                        ss.active = (st == AudioSessionStateActive);
                        ss.lastActiveMs = lastActive;

                        // Keep COM refs for control.
                        Impl::SessionKey key{id, static_cast<quint32>(pid), exe};
                        Impl::SessionCom sc;
                        sc.ctrl.attach(ctrl.detach());
                        sc.ctrl2.attach(ctrl2.detach());
                        sc.simple.attach(simple.detach());

                        // Peak meter (may be unavailable for some sessions).
                        if (sc.ctrl) {
                            ComPtr<IAudioMeterInformation> meter;
                            if (SUCCEEDED(sc.ctrl->QueryInterface(IID_IAudioMeterInformation, reinterpret_cast<void **>(meter.put()))) && meter) {
                                sc.meter.attach(meter.detach());
                            }
                        }

                        sc.lastActiveMs = lastActive;
                        sc.state = st;
                        auto *events = new Impl::SessionEvents(this, key);
                        if (sc.ctrl) {
                            // RegisterAudioSessionNotification does NOT guarantee AddRef on events across all implementations,
                            // so we keep an explicit ref we own.
                            events->AddRef();
                            sc.ctrl->RegisterAudioSessionNotification(events);
                            sc.events = events;
                        }
                        events->Release(); // balance initial ref

                        m->sessions.insert({key, std::move(sc)});

                        ds.sessions.push_back(ss);
                    }
                }
            }
        }

        m->devices.emplace(id, std::move(dc));
        devices.push_back(ds);
    }

    emit snapshotReady(devices);
}

void AudioWorker::emitPeaksNow()
{
    if (!m)
        return;

    QVector<SessionPeak> peaks;
    peaks.reserve(static_cast<int>(m->sessions.size()));

    for (const auto &kv : m->sessions) {
        const auto &key = kv.first;
        const auto &sc = kv.second;

        float p = 0.0f;
        if (sc.meter) {
            (void)sc.meter->GetPeakValue(&p);
        }

        SessionPeak sp;
        sp.deviceId = key.deviceId;
        sp.pid = key.pid;
        sp.exePath = key.exePath;
        sp.peak = qBound(0.0, static_cast<double>(p), 1.0);
        peaks.push_back(sp);
    }

    emit peaksReady(peaks);
}


