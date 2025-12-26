#include "ComInit.h"

#include <objbase.h>

ComInit::ComInit(DWORD coInit) noexcept
{
    m_hr = CoInitializeEx(nullptr, coInit);
}

ComInit::~ComInit()
{
    // Only uninitialize when we actually initialized COM in this call.
    // If CoInitializeEx returned RPC_E_CHANGED_MODE, COM was already initialized with
    // a different concurrency model, and we must NOT call CoUninitialize().
    if (m_hr == S_OK || m_hr == S_FALSE) {
        CoUninitialize();
    }
}


