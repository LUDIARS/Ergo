#include "ergo/bind/bind.h"
#include "ergo/bind/json_min.h"
#include "ws_client.h"

#include "actor_topology.h"
#include "pending_write_queue.h"
#include "variable_registry.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ergo::bind {

namespace {

// ---- value <-> JSON --------------------------------------------------------

jsonm::JsonValue value_to_json(const Value& v) {
    using namespace jsonm;
    switch (v.kind) {
        case VarKind::Bool:   return JsonValue::make_bool(v.b);
        case VarKind::Int32:
        case VarKind::Int64:  return JsonValue::make_number(static_cast<double>(v.i));
        case VarKind::Float:
        case VarKind::Double: return JsonValue::make_number(v.d);
        case VarKind::String: return JsonValue::make_string(v.s);
        case VarKind::Color: {
            auto a = JsonValue::make_array();
            for (int i = 0; i < 4; ++i) a.push(JsonValue::make_number(v.v[i]));
            return a;
        }
        case VarKind::Vec3: {
            auto a = JsonValue::make_array();
            for (int i = 0; i < 3; ++i) a.push(JsonValue::make_number(v.v[i]));
            return a;
        }
    }
    return JsonValue::make_null();
}

bool json_to_value(const jsonm::JsonValue& jv, VarKind kind, Value& out) {
    switch (kind) {
        case VarKind::Bool:
            if (!jv.is_bool()) return false;
            out = Value::of_bool(jv.b); return true;
        case VarKind::Int32:
            if (!jv.is_number()) return false;
            out = Value::of_int32(static_cast<int32_t>(jv.n)); return true;
        case VarKind::Int64:
            if (!jv.is_number()) return false;
            out = Value::of_int64(static_cast<int64_t>(jv.n)); return true;
        case VarKind::Float:
            if (!jv.is_number()) return false;
            out = Value::of_float(static_cast<float>(jv.n)); return true;
        case VarKind::Double:
            if (!jv.is_number()) return false;
            out = Value::of_double(jv.n); return true;
        case VarKind::String:
            if (!jv.is_string()) return false;
            out = Value::of_string(jv.s); return true;
        case VarKind::Color: {
            if (!jv.is_array() || !jv.a || jv.a->size() < 3) return false;
            float r = static_cast<float>((*jv.a)[0].as_number());
            float g = static_cast<float>((*jv.a)[1].as_number());
            float b = static_cast<float>((*jv.a)[2].as_number());
            float a = (jv.a->size() >= 4) ? static_cast<float>((*jv.a)[3].as_number()) : 1.0f;
            out = Value::of_color(r, g, b, a); return true;
        }
        case VarKind::Vec3: {
            if (!jv.is_array() || !jv.a || jv.a->size() < 3) return false;
            out = Value::of_vec3(static_cast<float>((*jv.a)[0].as_number()),
                                 static_cast<float>((*jv.a)[1].as_number()),
                                 static_cast<float>((*jv.a)[2].as_number()));
            return true;
        }
    }
    return false;
}

jsonm::JsonValue meta_to_json(const VarMeta& m) {
    using namespace jsonm;
    auto j = JsonValue::make_object();
    if (m.min != m.max) {
        j.set("min",  JsonValue::make_number(m.min));
        j.set("max",  JsonValue::make_number(m.max));
    }
    if (m.step != 0.0) j.set("step", JsonValue::make_number(m.step));
    if (m.read_only)   j.set("read_only", JsonValue::make_bool(true));
    if (!m.category.empty()) j.set("category", JsonValue::make_string(m.category));
    if (!m.unit.empty())     j.set("unit",     JsonValue::make_string(m.unit));
    if (m.actor_handle != 0) {
        // JSON Number is double; we'd lose precision above 2^53. In practice
        // handles are monotonically-assigned small integers so this is safe.
        j.set("actor", JsonValue::make_number(static_cast<double>(m.actor_handle)));
    }
    return j;
}

} // namespace

// ===========================================================================
// Engine::Impl — owns the WS transport and the message protocol. Variable,
// pending-write, and actor-topology state are delegated to dedicated single-
// responsibility helpers (see *_registry.h / *_queue.h / *_topology.h).
// ===========================================================================
struct Engine::Impl {
    using Entry      = detail::VariableRegistry::Entry;
    using ActorEntry = detail::ActorTopology::ActorEntry;

    std::string                 app_name = "anonymous";

    detail::VariableRegistry    vars;
    detail::PendingWriteQueue   pending;
    detail::ActorTopology       topo;

    ws::Client                  client;
    std::atomic<bool>           connected{false};

    void send_hello() {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op", JsonValue::make_string("hello"));
        m.set("role", JsonValue::make_string("engine"));
        m.set("app",  JsonValue::make_string(app_name));
        client.send_text(serialize(m));
    }

    void send_bind(const Entry& e) {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op",    JsonValue::make_string("bind"));
        m.set("name",  JsonValue::make_string(e.name));
        m.set("kind",  JsonValue::make_string(to_string(e.kind)));
        m.set("meta",  meta_to_json(e.meta));
        Value v = e.getter ? e.getter() : Value{};
        m.set("value", value_to_json(v));
        client.send_text(serialize(m));
    }

    void send_value(const std::string& name, const Value& v) {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op",    JsonValue::make_string("value"));
        m.set("name",  JsonValue::make_string(name));
        m.set("value", value_to_json(v));
        client.send_text(serialize(m));
    }

    void send_unbind(const std::string& name) {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op",   JsonValue::make_string("unbind"));
        m.set("name", JsonValue::make_string(name));
        client.send_text(serialize(m));
    }

    void send_actor_register(const ActorEntry& a) {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op",     JsonValue::make_string("actor_register"));
        m.set("handle", JsonValue::make_number(static_cast<double>(a.handle)));
        m.set("parent", JsonValue::make_number(static_cast<double>(a.parent)));
        m.set("name",   JsonValue::make_string(a.name));
        client.send_text(serialize(m));
    }

    void send_actor_unregister(uint64_t handle) {
        using namespace jsonm;
        auto m = JsonValue::make_object();
        m.set("op",     JsonValue::make_string("actor_unregister"));
        m.set("handle", JsonValue::make_number(static_cast<double>(handle)));
        client.send_text(serialize(m));
    }

    void on_text(const std::string& text) {
        using namespace jsonm;
        JsonValue req;
        if (!parse(text, req) || !req.is_object()) return;
        const std::string op = req.find("op") ? req.find("op")->as_string() : std::string{};
        if (op != "set") return;

        const std::string name = req.find("name") ? req.find("name")->as_string() : "";
        if (name.empty()) return;
        const JsonValue* jv = req.find("value");
        if (!jv) return;

        Handle  h;
        VarKind kind;
        if (!vars.resolve_writable(name, h, kind)) return;

        Value v;
        if (!json_to_value(*jv, kind, v)) return;

        pending.push(h, std::move(v));
    }

    void on_open() {
        send_hello();
        // Replay actor topology first — the server should know every actor
        // before bind messages start flowing so it can attach variables to
        // the right tree node.
        for (auto& a : topo.snapshot()) send_actor_register(a);
        for (auto& e : vars.snapshot()) send_bind(e);
        // Prime change detection so the next apply pass does not re-emit.
        vars.prime_all_published();
        connected.store(true, std::memory_order_release);
    }
};

// ===========================================================================
// Engine — public API
// ===========================================================================

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine() : impl_(new Impl()) {
    impl_->client.set_on_text([this](const std::string& t){ impl_->on_text(t); });
    impl_->client.set_on_open([this]{ impl_->on_open(); });
}

Engine::~Engine() {
    disconnect();
    delete impl_;
}

void Engine::set_app_name(std::string app) {
    impl_->app_name = std::move(app);
}

const std::string& Engine::app_name() const { return impl_->app_name; }

bool Engine::connect(const std::string& host, uint16_t port, const std::string& path) {
    impl_->client.start(host, port, path);
    return true;
}

void Engine::disconnect() {
    impl_->client.stop();
    impl_->connected.store(false, std::memory_order_release);
}

bool Engine::is_connected() const {
    return impl_->client.is_connected();
}

Handle Engine::bind_accessor(std::string                       name,
                             VarKind                           kind,
                             std::function<Value()>            getter,
                             std::function<void(const Value&)> setter,
                             VarMeta                           meta) {
    if (name.empty() || !getter) return INVALID_HANDLE;

    Impl::Entry stored{};
    const Handle h = impl_->vars.insert(std::move(name), kind, std::move(getter),
                                        std::move(setter), std::move(meta), stored);
    // Send bind immediately if connected; otherwise on_open will resend.
    if (impl_->client.is_connected()) {
        impl_->send_bind(stored);
        impl_->vars.mark_published(h);
    }
    return h;
}

template<class T> Handle Engine::bind(std::string name, T* ptr, VarMeta meta) {
    if (!ptr) return INVALID_HANDLE;
    auto getter = [ptr]() -> Value {
        if constexpr (std::is_same_v<T, bool>)        return Value::of_bool(*ptr);
        else if constexpr (std::is_same_v<T, int32_t>)return Value::of_int32(*ptr);
        else if constexpr (std::is_same_v<T, int64_t>)return Value::of_int64(*ptr);
        else if constexpr (std::is_same_v<T, float>)  return Value::of_float(*ptr);
        else if constexpr (std::is_same_v<T, double>) return Value::of_double(*ptr);
        else if constexpr (std::is_same_v<T, std::string>) return Value::of_string(*ptr);
        else { static_assert(sizeof(T) == 0, "unsupported bind type"); return {}; }
    };
    auto setter = [ptr](const Value& v) {
        if constexpr (std::is_same_v<T, bool>)         { *ptr = v.b; }
        else if constexpr (std::is_same_v<T, int32_t>) { *ptr = static_cast<int32_t>(v.i); }
        else if constexpr (std::is_same_v<T, int64_t>) { *ptr = v.i; }
        else if constexpr (std::is_same_v<T, float>)   { *ptr = static_cast<float>(v.d); }
        else if constexpr (std::is_same_v<T, double>)  { *ptr = v.d; }
        else if constexpr (std::is_same_v<T, std::string>) { *ptr = v.s; }
    };

    VarKind kind;
    if constexpr (std::is_same_v<T, bool>)        kind = VarKind::Bool;
    else if constexpr (std::is_same_v<T, int32_t>)kind = VarKind::Int32;
    else if constexpr (std::is_same_v<T, int64_t>)kind = VarKind::Int64;
    else if constexpr (std::is_same_v<T, float>)  kind = VarKind::Float;
    else if constexpr (std::is_same_v<T, double>) kind = VarKind::Double;
    else if constexpr (std::is_same_v<T, std::string>) kind = VarKind::String;
    else { static_assert(sizeof(T) == 0, "unsupported bind type"); kind = VarKind::Bool; }

    return bind_accessor(std::move(name), kind, std::move(getter), std::move(setter), std::move(meta));
}

template Handle Engine::bind<bool>       (std::string, bool*,        VarMeta);
template Handle Engine::bind<int32_t>    (std::string, int32_t*,     VarMeta);
template Handle Engine::bind<int64_t>    (std::string, int64_t*,     VarMeta);
template Handle Engine::bind<float>      (std::string, float*,       VarMeta);
template Handle Engine::bind<double>     (std::string, double*,      VarMeta);
template Handle Engine::bind<std::string>(std::string, std::string*, VarMeta);

void Engine::unbind(Handle h) {
    if (h == INVALID_HANDLE) return;
    std::string name;
    bool        was_published = false;
    if (!impl_->vars.erase(h, name, was_published)) return;
    if (was_published && impl_->client.is_connected()) {
        impl_->send_unbind(name);
    }
}

void Engine::actor_register(uint64_t handle, uint64_t parent,
                            const std::string& name)
{
    if (handle == 0) return;
    const Impl::ActorEntry a = impl_->topo.upsert(handle, parent, name);
    if (impl_->client.is_connected()) impl_->send_actor_register(a);
}

void Engine::actor_unregister(uint64_t handle) {
    if (handle == 0) return;
    if (impl_->topo.erase(handle) && impl_->client.is_connected()) {
        impl_->send_actor_unregister(handle);
    }
}

void Engine::apply_pending_writes() {
    // Drain incoming sets and apply them.
    for (auto& w : impl_->pending.drain()) {
        std::function<void(const Value&)> setter;
        VarMeta meta;
        VarKind kind = VarKind::Bool;
        if (!impl_->vars.fetch_for_apply(w.handle, setter, meta, kind)) continue;
        if (!setter) continue;
        if (w.value.kind != kind) continue;
        const Value clamped = clamp_to_meta(w.value, meta);
        setter(clamped);
        // Update last_seen so change detection below doesn't re-emit.
        impl_->vars.refresh_last_seen(w.handle);
    }

    // Detect external changes and push them to the server.
    if (!impl_->client.is_connected()) return;
    for (auto& [n, v] : impl_->vars.collect_changes()) {
        impl_->send_value(n, v);
    }
}

} // namespace ergo::bind
