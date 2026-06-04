#pragma once
#include <utility>

namespace wit::infra {
template <typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F action) : action_(std::move(action)) {}
    ~ScopeGuard() { if (active_) { action_(); } }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    void Dismiss() { active_ = false; }
private:
    F action_;
    bool active_{true};
};

template <typename F>
[[nodiscard]] ScopeGuard<F> OnExit(F action) { return ScopeGuard<F>(std::move(action)); }
}

