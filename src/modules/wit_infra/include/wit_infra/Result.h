#pragma once
#include <variant>

namespace wit::infra {
template <typename T, typename E>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(E error) : storage_(std::move(error)) {}
    bool HasValue() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return HasValue(); }
    T& Value() { return std::get<T>(storage_); }
    const T& Value() const { return std::get<T>(storage_); }
    E& Error() { return std::get<E>(storage_); }
    const E& Error() const { return std::get<E>(storage_); }
private:
    std::variant<T, E> storage_;
};
}

