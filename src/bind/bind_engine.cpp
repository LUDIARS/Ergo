#include "ergo/bind/bind.h"
#include "ergo/bind/json_min.h"
#include "ws_client.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>
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
    return j;
}

} // namespace

// ===========================================================================
// Engine::Impl
// ===========================================================================
struct Engine::Impl {
    struct Entry {
        Handle                              handle = INVALID_HANDLE;
        std::string                         name;
        VarKind                             kind = VarKind::Bool;
        VarMeta                             meta;
        std::function<Value()>              getter;
        std::function<void(const Value&)>   setter;
        Value                               last_seen;
        bool                                published = false;
    };

    struct PendingWrite {
        Handle handle = INVALID_HANDLE;
        Value  value;
    };

    std::string                              app_name = "anonymous";

    mutable std::mutex                       mtx;
    std::unordered_map<Handle, Entry>        by_handle;
    std::unordered_map<std::string, Handle>  by_name;
    Handle                                   next_handle = 1;

    std::mutex                               wq_mtx;
    std::vector<PendingWrite>                wq;

    ws::Client                               client;
    std::atomic<bool>                        connected{false};

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

        VarKind kind;
        Handle h;
        {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = by_name.find(name);
            if (it == by_name.end()) return;
            h = it->second;
            auto eit = by_handle.find(h);
            if (eit == by_handle.end()) return;
            if (eit->second.meta.read_only) return;
            kind = eit->second.kind;
        }

        Value v;
        if (!json_to_value(*jv, kind, v)) return;

        std::lock_guard<std::mutex> lk(wq_mtx);
        wq.push_back({h, std::move(v)});
    }

    void on_open() {
        send_hello();
        std::vector<Entry> entries;
        {
            std::lock_guard<std::mutex> lk(mtx);
            entries.reserve(by_handle.size());
            for (auto& [h, e] : by_handle) entries.push_back(e);
        }
        for (auto& e : entries) send_bind(e);
        // Mark all entries as "published" to prime change detection.
        std::lock_guard<std::mutex> lk(mtx);
        for (auto& [h, e] : by_handle) {
            if (e.getter) e.last_seen = e.getter();
            e.published = true;
        }
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

    Impl::Entry e{};
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->by_name.find(name);
        if (it != impl_->by_name.end()) {
            std::fprintf(stderr,
                "[ergo::bind] WARN: re-binding existing variable \"%s\" — old handle dropped\n",
                name.c_str());
            impl_->by_handle.erase(it->second);
            impl_->by_name.erase(it);
        }
        e.handle  = impl_->next_handle++;
        if (impl_->next_handle == INVALID_HANDLE) impl_->next_handle = 1;
        e.name    = name;
        e.kind    = kind;
        e.meta    = std::move(meta);
        e.getter  = std::move(getter);
        e.setter  = std::move(setter);
        if (e.getter) e.last_seen = e.getter();
        e.published = false;
        impl_->by_name[name] = e.handle;
        impl_->by_handle.emplace(e.handle, e);
    }
    // Send bind immediately if connected; otherwise on_open will resend.
    if (impl_->client.is_connected()) {
        impl_->send_bind(e);
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->by_handle.find(e.handle);
        if (it != impl_->by_handle.end()) it->second.published = true;
    }
    return e.handle;
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
    bool was_published = false;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->by_handle.find(h);
        if (it == impl_->by_handle.end()) return;
        name = it->second.name;
        was_published = it->second.published;
        impl_->by_name.erase(it->second.name);
        impl_->by_handle.erase(it);
    }
    if (was_published && impl_->client.is_connected()) {
        impl_->send_unbind(name);
    }
}

void Engine::apply_pending_writes() {
    // Drain incoming sets first
    std::vector<Impl::PendingWrite> local;
    { std::lock_guard<std::mutex> lk(impl_->wq_mtx); local.swap(impl_->wq); }
    for (auto& w : local) {
        std::function<void(const Value&)> setter;
        VarMeta meta;
        VarKind kind = VarKind::Bool;
        {
            std::lock_guard<std::mutex> lk(impl_->mtx);
            auto it = impl_->by_handle.find(w.handle);
            if (it == impl_->by_handle.end()) continue;
            if (it->second.meta.read_only) continue;
            setter = it->second.setter;
            meta   = it->second.meta;
            kind   = it->second.kind;
        }
        if (!setter) continue;
        if (w.value.kind != kind) continue;
        const Value clamped = clamp_to_meta(w.value, meta);
        setter(clamped);
        // Update last_seen so the change-detection loop below doesn't re-emit.
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->by_handle.find(w.handle);
        if (it != impl_->by_handle.end() && it->second.getter) {
            it->second.last_seen = it->second.getter();
        }
    }

    // Detect external changes and push them to the server.
    if (!impl_->client.is_connected()) return;
    std::vector<std::pair<std::string, Value>> outs;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        for (auto& [h, e] : impl_->by_handle) {
            if (!e.getter || !e.published) continue;
            Value cur = e.getter();
            if (!cur.equals(e.last_seen)) {
                e.last_seen = cur;
                outs.emplace_back(e.name, cur);
            }
        }
    }
    for (auto& [n, v] : outs) impl_->send_value(n, v);
}

} // namespace ergo::bind
