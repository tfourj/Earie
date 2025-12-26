#include "UpdateCoalescer.h"

UpdateCoalescer::UpdateCoalescer(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &UpdateCoalescer::flush);
}

void UpdateCoalescer::post(std::function<void()> fn)
{
    m_pending.emplace_back(std::move(fn));
    if (!m_timer.isActive())
        m_timer.start();
}

void UpdateCoalescer::flush()
{
    auto work = std::move(m_pending);
    m_pending.clear();
    for (auto &fn : work) {
        if (fn)
            fn();
    }
}


