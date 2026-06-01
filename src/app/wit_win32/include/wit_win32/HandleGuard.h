#pragma once
#include <Windows.h>
#include <utility>

namespace wit::win32 {
template <typename T, auto Deleter>
class HandleGuard {
public:
    HandleGuard() = default;
    explicit HandleGuard(T handle) : handle_(handle) {}
    ~HandleGuard() { Reset(); }
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    T Get() const { return handle_; }
    T Release() { return std::exchange(handle_, {}); }
    void Reset(T handle = {}) { if (handle_) { Deleter(handle_); } handle_ = handle; }
private:
    T handle_{};
};
}

