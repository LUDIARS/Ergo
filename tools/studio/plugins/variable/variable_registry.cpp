#include "variable_registry.h"

namespace ergo::studio::plugins::variable {
namespace {

std::string str_field(const JsonValue& msg, const char* key) {
    const JsonValue* v = field(msg, key);
    return v ? v->as_string() : std::string();
}

int64_t int_field(const JsonValue& msg, const char* key) {
    const JsonValue* v = field(msg, key);
    return v ? static_cast<int64_t>(v->as_number()) : 0;
}

JsonValue value_field(const JsonValue& msg, const char* key) {
    const JsonValue* v = field(msg, key);
    return v ? deep_copy(*v) : JsonValue::make_null();
}

} // namespace

bool VariableRegistry::bind(uint64_t owner, const std::string& app, const JsonValue& msg) {
    const std::string name = str_field(msg, "name");
    if (name.empty()) return false;
    JsonValue meta = value_field(msg, "meta");
    if (!meta.is_object()) meta = JsonValue::make_object();
    vars_[name] = {name, str_field(msg, "kind"), std::move(meta), value_field(msg, "value"), app, owner};
    return true;
}

bool VariableRegistry::set_value(const std::string& name, const JsonValue& value) {
    const auto it = vars_.find(name);
    if (it == vars_.end()) return false;
    it->second.value = deep_copy(value);
    return true;
}

bool VariableRegistry::unbind(const std::string& name) { return vars_.erase(name) > 0; }

bool VariableRegistry::register_actor(uint64_t owner, const std::string& app, const JsonValue& msg) {
    const int64_t handle = int_field(msg, "handle");
    if (handle <= 0) return false;
    actors_[handle] = {handle, int_field(msg, "parent"), str_field(msg, "name"), app, owner};
    return true;
}

bool VariableRegistry::unregister_actor(int64_t handle) { return actors_.erase(handle) > 0; }

VariableRegistry::Purged VariableRegistry::purge_owner(uint64_t owner) {
    Purged p;
    for (auto it = vars_.begin(); it != vars_.end();) {
        if (it->second.owner == owner) { it = vars_.erase(it); p.vars = true; }
        else ++it;
    }
    for (auto it = actors_.begin(); it != actors_.end();) {
        if (it->second.owner == owner) { it = actors_.erase(it); p.actors = true; }
        else ++it;
    }
    return p;
}

std::optional<uint64_t> VariableRegistry::owner_of(const std::string& name) const {
    const auto it = vars_.find(name);
    if (it == vars_.end()) return std::nullopt;
    return it->second.owner;
}

std::vector<BoundVar> VariableRegistry::vars() const {
    std::vector<BoundVar> out;
    for (const auto& [k, v] : vars_) out.push_back(v);
    return out;
}

std::vector<ActorNode> VariableRegistry::actors() const {
    std::vector<ActorNode> out;
    for (const auto& [k, a] : actors_) out.push_back(a);
    return out;
}

JsonValue VariableRegistry::vars_json() const {
    JsonValue arr = JsonValue::make_array();
    for (const auto& [k, v] : vars_) {
        arr.push(jobj({{"name", jstr(v.name)}, {"kind", jstr(v.kind)}, {"meta", v.meta},
                       {"value", v.value}, {"app", jstr(v.app)}}));
    }
    return arr;
}

JsonValue VariableRegistry::actors_json() const {
    JsonValue arr = JsonValue::make_array();
    for (const auto& [k, a] : actors_) {
        arr.push(jobj({{"handle", jnum(static_cast<double>(a.handle))},
                       {"parent", jnum(static_cast<double>(a.parent))},
                       {"name", jstr(a.name)}, {"app", jstr(a.app)}}));
    }
    return arr;
}

} // namespace ergo::studio::plugins::variable
