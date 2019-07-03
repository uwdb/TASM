#ifndef LIGHTDB_MEMOIZE_H
#define LIGHTDB_MEMOIZE_H

#include "reference.h"
#include <optional>
#include <functional>

#include "timer.h"

namespace lightdb {

template<typename T>
class lazy {
public:
    explicit lazy(const std::function<T()> &initializer, std::string id) noexcept
            : initializer_(initializer), value_(nullptr), id_(id)
    { }

    lazy(const lazy&) = default;
    lazy(lazy &&) noexcept = default;
    lazy& operator=(const lazy&) = default;
    lazy& operator=(lazy&&) noexcept = default;

    constexpr T* operator->() { return &value(); }
    constexpr T& operator*() & { return value(); }
    constexpr T&& operator*() &&  { return *value(); }

    constexpr operator T&() { return value(); }

    constexpr bool initialized() const noexcept { return static_cast<bool>(value_); }

    T& value() {
        GLOBAL_TIMER.startSection(id_);
        if(value_ == nullptr)
            value_ = std::make_shared<T>(initializer_());
        GLOBAL_TIMER.endSection(id_);
        return *value_;
    }

private:
    std::function<T()> initializer_;
    std::shared_ptr<T> value_;
    std::string id_;
};

} // namespace lightdb

#endif //LIGHTDB_MEMOIZE_H
