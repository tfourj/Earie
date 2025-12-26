#pragma once

#include <windows.h>
#include <objbase.h>

class ComInit
{
public:
    explicit ComInit(DWORD coInit = COINIT_MULTITHREADED) noexcept;
    ~ComInit();

    ComInit(const ComInit &) = delete;
    ComInit &operator=(const ComInit &) = delete;

    HRESULT hr() const noexcept { return m_hr; }
    bool ok() const noexcept { return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE; }

private:
    HRESULT m_hr = E_FAIL;
};


