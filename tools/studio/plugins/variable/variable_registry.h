#pragma once
/// Registry of variables and actors bound by ergo_bind engine clients
/// (wire protocol: tools/ergo variable/protocol.ts, SCHEMA_VERSION 2).
/// Pure state: no sockets, no locking. VariablePlugin owns the lock.
#include "../common/json_util.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::variable {

struct BoundVar {
    std::string name;
    std::string kind;     ///< bool / int32 / int64 / float / double / string / color / vec3
    JsonValue   meta;     ///< {min, max, step, read_only, category, unit, actor}
    JsonValue   value;
    std::string app;
    uint64_t    owner = 0;   ///< connection that bound it
};

struct ActorNode {
    int64_t     handle = 0;   ///< > 0
    int64_t     parent = 0;   ///< 0 = root
    std::string name;
    std::string app;
    uint64_t    owner = 0;
};

class VariableRegistry {
public:
    /// `msg` is an engine "bind" frame. False when it lacks a name.
    bool bind(uint64_t owner, const std::string& app, const JsonValue& msg);
    /// True when the variable exists (its value is replaced).
    bool set_value(const std::string& name, const JsonValue& value);
    bool unbind(const std::string& name);
    bool register_actor(uint64_t owner, const std::string& app, const JsonValue& msg);
    bool unregister_actor(int64_t handle);

    struct Purged {
        bool vars   = false;
        bool actors = false;
    };
    /// Drop everything bound by a closed engine connection.
    Purged purge_owner(uint64_t owner);

    std::optional<uint64_t> owner_of(const std::string& name) const;

    std::size_t var_count() const { return vars_.size(); }
    std::size_t actor_count() const { return actors_.size(); }
    std::vector<BoundVar>  vars() const;
    std::vector<ActorNode> actors() const;

    /// Wire forms: [{name, kind, meta, value, app}] and [{handle, parent, name, app}].
    JsonValue vars_json() const;
    JsonValue actors_json() const;

private:
    std::map<std::string, BoundVar> vars_;
    std::map<int64_t, ActorNode>    actors_;
};

} // namespace ergo::studio::plugins::variable
