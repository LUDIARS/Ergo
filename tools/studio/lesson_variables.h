#pragma once
/// Composition root glue: lets the "lesson" plugin read and set the running
/// game's variables through the "variable" plugin without either plugin
/// knowing the other. builtin_plugins() shares one GameVariableBridge
/// between the two factories.
#include "plugins/lesson/game_variables.h"

#include "ergo/studio/host/plugin.h"

#include <memory>
#include <mutex>

namespace ergo::studio::plugins::variable {
class VariablePlugin;
}

namespace ergo::studio::app {

/// IGameVariables over a VariablePlugin that may not exist (yet / any more):
/// without one, every variable is "not bound". Thread-safe.
class GameVariableBridge final : public plugins::lesson::IGameVariables {
public:
    std::optional<plugins::lesson::GameVariable> find(const std::string& name) const override;
    bool request_set(const std::string& name, const plugins::JsonValue& value) override;

    /// `plugin` must stay alive until detach().
    void attach(plugins::variable::VariablePlugin* plugin);
    /// Returns once no call into the plugin is in flight.
    void detach();

private:
    mutable std::mutex                 mtx_;
    plugins::variable::VariablePlugin* plugin_ = nullptr;
};

/// The variable plugin, attached to `bridge` for exactly its lifetime. The
/// plugin host destroys plugins in load order, so "variable" goes away while
/// the lesson worker may still run: the returned plugin detaches first.
std::unique_ptr<host::IPlugin> make_bridged_variable_plugin(std::shared_ptr<GameVariableBridge> bridge,
                                                            const host::LogFn& log);

} // namespace ergo::studio::app
