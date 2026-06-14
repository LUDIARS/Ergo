#pragma once

/// ergo::bind — bind any host variable for live editing from the unified
/// ergo tool (tools/ergo/, `variable` plugin).
///
/// Hosts call `bind()` (or the `BIND_VAR` macro) on debug code paths. The
/// engine connects outbound to the tool's variable plugin (default
/// `ws://127.0.0.1:5170/variable/ws`) and:
///   * sends a `bind` message for each registered variable
///   * watches the value each frame; on change, sends a `value` message
///   * accepts `set` messages from the server, queues them, and applies
///     them on `apply_pending_writes()`
///
/// Compile-time strippable:
///   ERGO_BIND_ENABLED defined  → real implementation
///   undefined                  → header-only no-op shims; macros expand to (void)0

#include "ergo/bind/types.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ergo::bind {

// Handle / INVALID_HANDLE now live in types.h (included above) so that
// lightweight consumers can use them without the Engine declaration.

#ifdef ERGO_BIND_ENABLED

class Engine {
public:
    static Engine& instance();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    /// Identify this app on the tool side. Default: "anonymous". Must be
    /// called before connect() to take effect on the server side.
    void set_app_name(std::string app);
    const std::string& app_name() const;

    /// Connect outbound to the unified ergo tool's `variable` plugin.
    /// Returns true if a background worker was started. Reconnects
    /// automatically on failure.
    bool connect(const std::string& host = "127.0.0.1",
                 uint16_t port = 5170,
                 const std::string& path = "/variable/ws");

    /// Stop the worker thread. Idempotent.
    void disconnect();

    bool is_connected() const;

    // ---- bind / unbind ---------------------------------------------------

    /// Register a typed lvalue. Supported T: bool, int32_t, int64_t, float,
    /// double, std::string. Returns INVALID_HANDLE on bad inputs.
    template<class T>
    Handle bind(std::string name, T* ptr, VarMeta meta = {});

    /// Register custom getter/setter (use for Color, Vec3, derived values).
    Handle bind_accessor(std::string                       name,
                         VarKind                           kind,
                         std::function<Value()>            getter,
                         std::function<void(const Value&)> setter,
                         VarMeta                           meta = {});

    void unbind(Handle h);

    // ---- actor hierarchy -------------------------------------------------
    //
    // Used by ergo_actor to announce scene-graph nodes. The engine
    // caches these so they can be replayed on reconnect alongside the
    // bind table. No-ops are safe — tests can register actors even
    // without a live connection.

    /// Notify the server that an actor exists in the scene graph.
    /// `parent == 0` means a root actor.
    void actor_register(uint64_t handle, uint64_t parent, const std::string& name);

    /// Remove a previously-registered actor.
    void actor_unregister(uint64_t handle);

    // ---- per-frame --------------------------------------------------------

    /// Drain queued writes and invoke setters. Also publishes value-change
    /// notifications to the server (if connected). Call from the main thread
    /// once per frame.
    void apply_pending_writes();

private:
    Engine();
    ~Engine();
    struct Impl;
    Impl* impl_ = nullptr;
};

// ---- Macros ---------------------------------------------------------------

#define ERGO_BIND_CONCAT_IMPL(a, b) a##b
#define ERGO_BIND_CONCAT(a, b) ERGO_BIND_CONCAT_IMPL(a, b)

/// Bind a primitive lvalue. Static handle => one-shot per translation unit.
#define BIND_VAR(name, lvalue, ...)                                              \
    static ::ergo::bind::Handle ERGO_BIND_CONCAT(_ergo_bind_h_, __COUNTER__) =   \
        ::ergo::bind::Engine::instance().bind(name, &(lvalue), ##__VA_ARGS__)

/// Bind custom getter/setter.
#define BIND_FN(name, kind, getter, setter, ...)                                 \
    static ::ergo::bind::Handle ERGO_BIND_CONCAT(_ergo_bind_h_, __COUNTER__) =   \
        ::ergo::bind::Engine::instance().bind_accessor(name, kind, getter, setter, ##__VA_ARGS__)

#else // !ERGO_BIND_ENABLED — header-only no-ops

class Engine {
public:
    static Engine& instance() { static Engine e; return e; }
    void set_app_name(std::string) {}
    const std::string& app_name() const { static const std::string s; return s; }
    bool connect(const std::string& = "127.0.0.1", uint16_t = 5170,
                 const std::string& = "/variable/ws") { return false; }
    void disconnect() {}
    bool is_connected() const { return false; }
    template<class T> Handle bind(std::string, T*, VarMeta = {}) { return INVALID_HANDLE; }
    Handle bind_accessor(std::string, VarKind,
                         std::function<Value()>,
                         std::function<void(const Value&)>,
                         VarMeta = {}) { return INVALID_HANDLE; }
    void unbind(Handle) {}
    void actor_register(uint64_t, uint64_t, const std::string&) {}
    void actor_unregister(uint64_t) {}
    void apply_pending_writes() {}
};

#define BIND_VAR(name, lvalue, ...) ((void)0)
#define BIND_FN(name, kind, getter, setter, ...) ((void)0)

#endif

} // namespace ergo::bind
