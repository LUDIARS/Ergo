#pragma once

/// ergo::blackboard — global named-property registry with category-scoped
/// subscription lifecycle.
///
/// Port of the Blackboard feature from VGA-Team2026/Foundation (Unity, R3)
/// to plain C++17. Hosts:
///   1. Hold their own `Property<T>` fields (lifetime owned by host).
///   2. Register them by name into the singleton `Engine`.
///   3. Subscribe to changes from anywhere in the codebase, either
///      - by name only (`subscribe<T>(name, cb)` / `find<T>(name)`): the
///        consumer never includes the producer's struct. The name must be
///        registered (with the same `T`) before the consumer subscribes, or
///      - by pointer (`subscribe<T>(name, &prop, cb)`): for code that already
///        holds the Property; `name` is then only a debug label.
///   4. Optionally tag registrations / subscriptions with a category and
///      `release(category)` them in batch (e.g. on scene unload).
///
/// `Subscription` is RAII: dropping it unsubscribes once. The same logic
/// triggers from `release(category)`; both paths are idempotent so calling
/// either order (or both) is safe. A by-name subscription is additionally
/// bound to its registration: `unregister`, a replacing `register_property`,
/// `release(<owner category>)` and `release_all` close it while the Property
/// is still alive, so a subscriber in another category (e.g. a HUD) may
/// outlive the producer's Property safely.

#include "ergo/blackboard/property.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace ergo::blackboard {

/// RAII handle returned by `Engine::subscribe`. Destruction or `reset()`
/// unsubscribes once. Idempotent with category-level cleanup.
class Subscription {
public:
    Subscription() = default;

    Subscription(std::shared_ptr<bool>          fired,
                 std::function<void()>          cleanup) noexcept
        : fired_(std::move(fired)), cleanup_(std::move(cleanup)) {}

    ~Subscription() { reset(); }

    Subscription(const Subscription&)            = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : fired_(std::move(other.fired_)),
          cleanup_(std::move(other.cleanup_)) {}

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            reset();
            fired_   = std::move(other.fired_);
            cleanup_ = std::move(other.cleanup_);
        }
        return *this;
    }

    /// Manually fire the cleanup. Idempotent.
    void reset() {
        if (fired_ && !*fired_) {
            *fired_ = true;
            if (cleanup_) cleanup_();
        }
        fired_.reset();
        cleanup_ = nullptr;
    }

    /// True if this handle still owns an active subscription.
    bool active() const { return fired_ && !*fired_; }

private:
    std::shared_ptr<bool>     fired_;
    std::function<void()>     cleanup_;
};

class Engine {
public:
    static Engine& instance();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    /// Register an existing `Property<T>` under `name`. The Property must
    /// remain alive while registered. Re-registering an existing name
    /// replaces the previous entry (warning is logged).
    template <typename T>
    void register_property(std::string name, Property<T>* property,
                           std::string category = "") {
        if (property == nullptr) {
            warn("register_property: nullptr for '" + name + "'");
            return;
        }
        register_erased(std::move(name), property, std::type_index(typeid(T)),
                        std::move(category));
    }

    /// Remove a registration by name. Idempotent.
    void unregister(const std::string& name);

    /// Typed lookup of a registered Property. Returns nullptr when `name`
    /// is not registered, or (with a warning) when it was registered with a
    /// type other than `T`. The pointer is valid while the registration
    /// lives; do not cache it across `unregister` / `release`.
    template <typename T>
    Property<T>* find(const std::string& name) const {
        return static_cast<Property<T>*>(
            find_erased(name, std::type_index(typeid(T)), /*warn_missing=*/false));
    }

    /// Subscribe by name only: the consumer never sees the producer's
    /// Property. `name` must already be registered with type `T`; an
    /// unregistered name, a different type or a null `on_change` logs a
    /// warning and returns an inactive Subscription (no deferred binding).
    ///
    /// The subscription is filed under `category` (for `release(category)`)
    /// AND bound to the registration: unregistering, replacing or releasing
    /// that registration closes it first, so the handle stays safe to drop
    /// after the producer's Property is gone.
    template <typename T>
    [[nodiscard]] Subscription subscribe(
        const std::string&            name,
        std::function<void(const T&)> on_change,
        std::string                   category = "")
    {
        if (!on_change) {
            warn("subscribe: null on_change for '" + name + "'");
            return {};
        }
        auto* property = static_cast<Property<T>*>(
            find_erased(name, std::type_index(typeid(T)), /*warn_missing=*/true));
        if (property == nullptr) return {};

        auto token = property->subscribe(std::move(on_change));
        bump_count(name);

        auto fired = std::make_shared<bool>(false);
        std::function<void()> cleanup =
            [property, token, name, this]() {
                property->unsubscribe(token);
                drop_count(name);
            };

        store_in_category(category.empty() ? std::string("Default") : category,
                          fired, cleanup);
        attach_to_entry(name, fired, cleanup);

        return Subscription(std::move(fired), std::move(cleanup));
    }

    /// Subscribe to value changes on `property`, which the caller already
    /// holds (`name` is a debug label for `subscription_count` and need not
    /// be registered). Returns an RAII handle whose destructor / `reset()`
    /// unsubscribes. Also added to `category` so `release(category)` can
    /// clean it up in bulk. Not bound to any registration: the caller keeps
    /// the Property alive until the handle is reset or released.
    template <typename T>
    [[nodiscard]] Subscription subscribe(
        const std::string&            name,
        Property<T>*                  property,
        std::function<void(const T&)> on_change,
        std::string                   category = "")
    {
        if (property == nullptr) {
            warn("subscribe: nullptr property for '" + name + "'");
            return {};
        }
        if (!on_change) {
            warn("subscribe: null on_change for '" + name + "'");
            return {};
        }

        auto token = property->subscribe(std::move(on_change));
        bump_count(name);

        // Cleanup captures the property + token + name so it stays valid even
        // if the entry is unregistered before this subscription dies.
        auto fired = std::make_shared<bool>(false);
        std::function<void()> cleanup =
            [property, token, name, this]() {
                property->unsubscribe(token);
                drop_count(name);
            };

        if (!category.empty()) {
            store_in_category(category, fired, cleanup);
        } else {
            store_in_category("Default", fired, cleanup);
        }

        return Subscription(std::move(fired), std::move(cleanup));
    }

    /// Run every cleanup recorded for `category` then drop registrations
    /// tagged with that category. Empty string is normalized to "Default".
    void release(const std::string& category);

    /// Wipe every registration / subscription. Useful between tests.
    void release_all();

    /// Enumerate every registration as (name, stored type, Property<T>*
    /// erased to `const void*`). Order is unspecified. Meant for host-side
    /// adapters (e.g. filling a UI bind context); prefer `find<T>` when the
    /// type is known. `fn` must not register / unregister / release.
    void visit(const std::function<void(const std::string&, std::type_index,
                                        const void*)>& fn) const;

    // ---- Debug ----------------------------------------------------------

    std::vector<std::string> registered_property_names() const;
    std::size_t              subscription_count(const std::string& name) const;
    std::string              debug_info() const;
    std::size_t              category_count() const;

private:
    Engine();
    ~Engine();
    struct Impl;
    Impl* impl_;

    void register_erased(std::string name, void* property,
                         std::type_index type, std::string category);

    /// Registered pointer for `name` if its stored type equals `type`.
    /// Warns on a type mismatch, and on a missing name when `warn_missing`.
    void* find_erased(const std::string& name, std::type_index type,
                      bool warn_missing) const;

    /// Bind a by-name subscription's cleanup to the registration of `name`
    /// so unregister / replace / release run it before erasing the entry.
    void attach_to_entry(const std::string&    name,
                         std::shared_ptr<bool> fired,
                         std::function<void()> cleanup);

    void bump_count(const std::string& name);
    void drop_count(const std::string& name);

    void store_in_category(const std::string&            category,
                           std::shared_ptr<bool>         fired,
                           std::function<void()>         cleanup);

    void warn(const std::string& msg) const;
};

} // namespace ergo::blackboard

// ---------------------------------------------------------------------------
// Macros — convenience wrappers around the Engine API.
// ---------------------------------------------------------------------------

/// Register a Property lvalue into the global Blackboard. Optional 3rd
/// argument is the category string. Type is deduced from the lvalue.
#define BLACKBOARD_REGISTER(name, prop, ...)                                \
    ::ergo::blackboard::Engine::instance().register_property(               \
        name, &(prop), ##__VA_ARGS__)
