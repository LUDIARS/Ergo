#include "ergo/inspector/inspector.h"

#include <cstdio>
#include <utility>

namespace ergo::inspector {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Inspector& Inspector::instance() {
    static Inspector s;
    return s;
}

Inspector::Inspector() = default;
Inspector::~Inspector() {
    // server is owned by start_server / stop_server; if a host forgot to stop,
    // we leave it alone — destruction order across TU statics is fragile and
    // the OS will reclaim the socket anyway.
}

// ---------------------------------------------------------------------------
// register_accessor — central registration path
// ---------------------------------------------------------------------------

Handle Inspector::register_accessor(std::string                       name,
                                    VarKind                           kind,
                                    std::function<Value()>            getter,
                                    std::function<void(const Value&)> setter,
                                    VarMeta                           meta) {
    if (name.empty() || !getter) return INVALID_HANDLE;

    std::lock_guard<std::mutex> lk(mtx_);

    // Duplicate-name policy: warn and overwrite (drop the old handle).
    auto it = by_name_.find(name);
    if (it != by_name_.end()) {
        std::fprintf(stderr,
                     "[ergo::inspector] WARN: re-registering existing variable \"%s\"; old handle %u dropped\n",
                     name.c_str(), it->second);
        by_handle_.erase(it->second);
        by_name_.erase(it);
    }

    Handle h = next_handle_++;
    if (next_handle_ == INVALID_HANDLE) next_handle_ = 1;

    Entry e{};
    e.handle    = h;
    e.tw.name   = name;
    e.tw.kind   = kind;
    e.tw.meta   = std::move(meta);
    e.tw.getter = std::move(getter);
    e.tw.setter = std::move(setter);
    if (e.tw.getter) e.last_seen = e.tw.getter();

    by_name_[name] = h;
    by_handle_.emplace(h, std::move(e));
    return h;
}

void Inspector::unregister(Handle h) {
    if (h == INVALID_HANDLE) return;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = by_handle_.find(h);
    if (it == by_handle_.end()) return;
    by_name_.erase(it->second.tw.name);
    by_handle_.erase(it);
}

// ---------------------------------------------------------------------------
// register_value — typed lvalue convenience overloads
// ---------------------------------------------------------------------------

namespace {
template<class T> VarKind kind_of();

template<> VarKind kind_of<bool>()        { return VarKind::Bool; }
template<> VarKind kind_of<int32_t>()     { return VarKind::Int32; }
template<> VarKind kind_of<int64_t>()     { return VarKind::Int64; }
template<> VarKind kind_of<float>()       { return VarKind::Float; }
template<> VarKind kind_of<double>()      { return VarKind::Double; }
template<> VarKind kind_of<std::string>() { return VarKind::String; }
} // namespace

template<class T>
Handle Inspector::register_value(std::string name, T* ptr, VarMeta meta) {
    if (!ptr) return INVALID_HANDLE;
    auto getter = [ptr]() -> Value {
        if constexpr (std::is_same_v<T, bool>)        return Value::of_bool(*ptr);
        else if constexpr (std::is_same_v<T, int32_t>)return Value::of_int32(*ptr);
        else if constexpr (std::is_same_v<T, int64_t>)return Value::of_int64(*ptr);
        else if constexpr (std::is_same_v<T, float>)  return Value::of_float(*ptr);
        else if constexpr (std::is_same_v<T, double>) return Value::of_double(*ptr);
        else if constexpr (std::is_same_v<T, std::string>) return Value::of_string(*ptr);
        else { static_assert(sizeof(T) == 0, "unsupported register_value type"); return {}; }
    };
    auto setter = [ptr](const Value& v) {
        if constexpr (std::is_same_v<T, bool>)         { *ptr = v.b; }
        else if constexpr (std::is_same_v<T, int32_t>) { *ptr = static_cast<int32_t>(v.i); }
        else if constexpr (std::is_same_v<T, int64_t>) { *ptr = v.i; }
        else if constexpr (std::is_same_v<T, float>)   { *ptr = static_cast<float>(v.d); }
        else if constexpr (std::is_same_v<T, double>)  { *ptr = v.d; }
        else if constexpr (std::is_same_v<T, std::string>) { *ptr = v.s; }
    };
    return register_accessor(std::move(name), kind_of<T>(),
                             std::move(getter), std::move(setter), std::move(meta));
}

// Explicit instantiations for the supported primitive kinds.
template Handle Inspector::register_value<bool>       (std::string, bool*,        VarMeta);
template Handle Inspector::register_value<int32_t>    (std::string, int32_t*,     VarMeta);
template Handle Inspector::register_value<int64_t>    (std::string, int64_t*,     VarMeta);
template Handle Inspector::register_value<float>      (std::string, float*,       VarMeta);
template Handle Inspector::register_value<double>     (std::string, double*,      VarMeta);
template Handle Inspector::register_value<std::string>(std::string, std::string*, VarMeta);

// ---------------------------------------------------------------------------
// Per-frame draining
// ---------------------------------------------------------------------------

void Inspector::apply_pending_writes() {
    auto pending = writes_.drain();
    for (auto& cmd : pending) {
        // Snapshot the entry under lock, run the setter outside it.
        std::function<void(const Value&)> setter;
        VarMeta meta;
        VarKind kind = VarKind::Bool;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = by_handle_.find(cmd.handle);
            if (it == by_handle_.end()) continue;
            if (it->second.tw.meta.read_only) continue;
            setter = it->second.tw.setter;
            meta   = it->second.tw.meta;
            kind   = it->second.tw.kind;
        }
        if (!setter) continue;
        if (cmd.value.kind != kind) continue; // type mismatch — drop
        const Value clamped = clamp_to_meta(cmd.value, meta);
        setter(clamped);

        // Refresh last_seen so we don't immediately broadcast our own write.
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(cmd.handle);
        if (it != by_handle_.end() && it->second.tw.getter) {
            it->second.last_seen = it->second.tw.getter();
        }
    }
}

// ---------------------------------------------------------------------------
// Read paths
// ---------------------------------------------------------------------------

std::vector<Tweakable> Inspector::snapshot() const {
    std::vector<Tweakable> out;
    std::lock_guard<std::mutex> lk(mtx_);
    out.reserve(by_handle_.size());
    for (auto& [h, e] : by_handle_) out.push_back(e.tw);
    return out;
}

bool Inspector::read_value(Handle h, Value& out) const {
    std::function<Value()> getter;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(h);
        if (it == by_handle_.end() || !it->second.tw.getter) return false;
        getter = it->second.tw.getter;
    }
    out = getter();
    return true;
}

Handle Inspector::find_by_name(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = by_name_.find(name);
    return it == by_name_.end() ? INVALID_HANDLE : it->second;
}

void Inspector::enqueue_write(Handle h, Value v) {
    if (h == INVALID_HANDLE) return;
    writes_.push(WriteCommand{h, std::move(v)});
}

} // namespace ergo::inspector
