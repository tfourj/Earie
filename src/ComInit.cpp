#include "ComInit.h"

#include <objbase.h>

ComInit::ComInit(DWORD coInit) noexcept
{
    m_hr = CoInitializeEx(nullptr, coInit);
}

ComInit::~ComInit()
{
    if (SUCCEEDED(m_hr)) {
        CoUninitialize();
    }
}


