#pragma once

/// ergo::blackboard::Property<T> — minimal R3.ReactiveProperty stand-in.
///
/// Owns a single value of type `T` and notifies subscribers when `set()` is
/// called with a value that differs from the current one (distinct-by-equality
/// when `T` supports `operator==`; always-notify otherwise).
///
/// The Blackboard registry refers to Property objects by raw pointer; the
/// Property's lifetime is owned by the calling code (typically a class
/// member field).

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace ergo::blackboard {

namespace detail {

template <typename T, typename = void>
struct has_eq : std::false_type {};

template <typename T>
struct has_eq<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>>
    : std::true_type {};

} // namespace detail

template <typename T>
class Property {
public:
    using OnChange = std::function<void(const T&)>;
    using Token    = uint64_t;

    Property() = default;
    explicit Property(T initial) : value_(std::move(initial)) {}

    Property(const Property&)            = delete;
    Property& operator=(const Property&) = delete;
    Property(Property&&)                 = delete;
    Property& operator=(Property&&)      = delete;

    const T& get() const { return value_; }

    /// Direct mutable access for callers that update the value piecewise
    /// without wanting a single-shot notify (e.g., per-component edits).
    /// No subscribers are notified — call `notify()` explicitly afterwards.
    T& mutable_ref() { return value_; }

    /// Set the value; subscribers fire only if the value actually changed
    /// (when `T` supports `operator==`). Otherwise subscribers fire on
    /// every call.
    void set(T value) {
        if constexpr (detail::has_eq<T>::value) {
            if (value_ == value) return;
        }
        value_ = std::move(value);
        notify();
    }

    /// Manually notify subscribers (use after `mutable_ref()` edits).
    void notify() {
        // Snapshot to allow subscribers to unsubscribe themselves safely.
        auto snapshot = subs_;
        for (const auto& [_token, cb] : snapshot) {
            if (cb) cb(value_);
        }
    }

    Token subscribe(OnChange cb) {
        Token t = next_token_++;
        if (t == 0) t = next_token_++;   // 0 reserved for INVALID_TOKEN
        subs_.emplace_back(t, std::move(cb));
        return t;
    }

    void unsubscribe(Token t) {
        if (t == 0) return;
        for (auto it = subs_.begin(); it != subs_.end(); ++it) {
            if (it->first == t) {
                subs_.erase(it);
                return;
            }
        }
    }

    std::size_t subscriber_count() const { return subs_.size(); }

private:
    T value_{};
    std::vector<std::pair<Token, OnChange>> subs_;
    Token next_token_ = 1;
};

} // namespace ergo::blackboard
