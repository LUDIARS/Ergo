#pragma once

/// Inspector — singleton registry + lifecycle for the Ergo Inspector tool.
///
/// Two compilation modes:
///   * `ERGO_INSPECTOR_ENABLED` defined  → real implementation, links against ergo_inspector
///   * undefined                         → header-only no-op stubs (zero-cost; macros expand to (void)0)
///
/// The library defines ERGO_INSPECTOR_ENABLED on its PUBLIC compile interface, so any
/// target linking against `ergo_inspector` automatically sees the real API.

#include "ergo/inspector/types.h"
#include "ergo/inspector/tweakable.h"
#include "ergo/inspector/command_queue.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ergo::inspector {

#ifdef ERGO_INSPECTOR_ENABLED

class Inspector {
public:
    static Inspector& instance();

    Inspector(const Inspector&)            = delete;
    Inspector& operator=(const Inspector&) = delete;

    // ---- Registration --------------------------------------------------

    /// Register a typed lvalue. Supported T: bool, int32_t, int64_t, float, double, std::string.
    template<class T>
    Handle register_value(std::string name, T* ptr, VarMeta meta = {});

    /// Register custom getter/setter for arbitrary kinds (Color, Vec3, derived values...).
    Handle register_accessor(std::string                       name,
                             VarKind                           kind,
                             std::function<Value()>            getter,
                             std::function<void(const Value&)> setter,
                             VarMeta                           meta = {});

    void unregister(Handle h);

    // ---- Server lifecycle (implemented in server_*.cpp) ----------------

    bool start_server(uint16_t port = 17317);
    void stop_server();
    bool server_running() const;

    // ---- Per-frame processing -----------------------------------------

    /// Drain queued writes and invoke setters on the calling thread.
    /// Must be called from the host's main thread, typically once per frame.
    void apply_pending_writes();

    // ---- Server-thread queries (also safe from any thread) ------------

    /// Snapshot of the current registry. Copies the Tweakable structs.
    std::vector<Tweakable> snapshot() const;

    /// Read the current value of a registered variable. Returns false if the
    /// handle is unknown. Calls the registered getter under a shared lock.
    bool read_value(Handle h, Value& out) const;

    /// Look up a handle by name. Returns INVALID_HANDLE if not found.
    Handle find_by_name(const std::string& name) const;

    /// Enqueue a write to be applied on the next apply_pending_writes().
    void enqueue_write(Handle h, Value v);

private:
    Inspector();
    ~Inspector();

    struct Entry {
        Handle    handle = INVALID_HANDLE;
        Tweakable tw;
        // Last broadcast value; used to detect external changes for `changed` notifications.
        Value     last_seen;
    };

    mutable std::mutex                       mtx_;
    std::unordered_map<Handle, Entry>        by_handle_;
    std::unordered_map<std::string, Handle>  by_name_;
    Handle                                   next_handle_ = 1;

    CommandQueue                             writes_;

    // Server bits (opaque to keep the header free of platform headers).
    struct ServerState;
    ServerState* server_ = nullptr;
};

// ---- Macros -----------------------------------------------------------

#define ERGO_INSPECTOR_CONCAT_IMPL(a, b) a##b
#define ERGO_INSPECTOR_CONCAT(a, b) ERGO_INSPECTOR_CONCAT_IMPL(a, b)

/// Register a primitive lvalue. The handle is stored in a static so the
/// registration happens once per translation unit.
#define ERGO_INSPECT_VAR(name, lvalue, ...)                                       \
    static ::ergo::inspector::Handle ERGO_INSPECTOR_CONCAT(_ergo_insp_h_, __COUNTER__) = \
        ::ergo::inspector::Inspector::instance().register_value(name, &(lvalue), ##__VA_ARGS__)

/// Register a custom getter/setter pair. `kind` is one of ergo::inspector::VarKind.
#define ERGO_INSPECT_FN(name, kind, getter, setter, ...)                          \
    static ::ergo::inspector::Handle ERGO_INSPECTOR_CONCAT(_ergo_insp_h_, __COUNTER__) = \
        ::ergo::inspector::Inspector::instance().register_accessor(               \
            name, kind, getter, setter, ##__VA_ARGS__)

#else // !ERGO_INSPECTOR_ENABLED — header-only no-op shims

class Inspector {
public:
    static Inspector& instance() { static Inspector i; return i; }

    template<class T>
    Handle register_value(const std::string&, T*, VarMeta = {}) { return INVALID_HANDLE; }

    Handle register_accessor(const std::string&, VarKind,
                             std::function<Value()>,
                             std::function<void(const Value&)>,
                             VarMeta = {}) { return INVALID_HANDLE; }

    void unregister(Handle) {}
    bool start_server(uint16_t = 17317) { return false; }
    void stop_server() {}
    bool server_running() const { return false; }
    void apply_pending_writes() {}

    std::vector<Tweakable> snapshot() const { return {}; }
    bool read_value(Handle, Value&) const { return false; }
    Handle find_by_name(const std::string&) const { return INVALID_HANDLE; }
    void enqueue_write(Handle, Value) {}
};

#define ERGO_INSPECT_VAR(name, lvalue, ...)            ((void)0)
#define ERGO_INSPECT_FN(name, kind, getter, setter, ...) ((void)0)

#endif // ERGO_INSPECTOR_ENABLED

} // namespace ergo::inspector
