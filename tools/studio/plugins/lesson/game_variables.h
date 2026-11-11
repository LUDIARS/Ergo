#pragma once
/// What the lesson plugin needs from the running game's variables. Owned by
/// this plugin and implemented at the composition root (over the "variable"
/// plugin), so nothing here depends on another plugin.
#include "../common/json_util.h"

#include <optional>
#include <string>

namespace ergo::studio::plugins::lesson {

struct GameVariable {
    std::string kind;    ///< ergo_bind kind: bool / int32 / int64 / float / double / string / color / vec3
    JsonValue   value;   ///< the last value the game published
};

class IGameVariables {
public:
    virtual ~IGameVariables() = default;
    /// The variable as the game last published it; nullopt when no connected game binds `name`.
    /// Called on the lesson worker thread.
    virtual std::optional<GameVariable> find(const std::string& name) const = 0;
    /// Ask the game that owns `name` to change it. False when it is not bound
    /// (true means the request was sent, not that the game accepted it).
    virtual bool request_set(const std::string& name, const JsonValue& value) = 0;
};

} // namespace ergo::studio::plugins::lesson
