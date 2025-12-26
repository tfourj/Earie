#pragma once

#include <windows.h>
#include <unknwn.h>

template <typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ComPtr(std::nullptr_t) {}

    ~ComPtr() { reset(); }

    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;

    ComPtr(ComPtr &&other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    ComPtr &operator=(ComPtr &&other) noexcept
    {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T *get() const { return m_ptr; }
    T **put()
    {
        reset();
        return &m_ptr;
    }

    T *operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    void reset()
    {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

    T *detach()
    {
        T *tmp = m_ptr;
        m_ptr = nullptr;
        return tmp;
    }

    void attach(T *p)
    {
        reset();
        m_ptr = p;
    }

    void addRef()
    {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

private:
    T *m_ptr = nullptr;
};


