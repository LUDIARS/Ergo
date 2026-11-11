#include "lesson_variables.h"

#include "plugins/variable/variable_plugin.h"

namespace ergo::studio::app {
namespace {

using plugins::variable::VariablePlugin;

/// Forwards everything to the VariablePlugin it owns; detaches the bridge before that plugin is destroyed.
class BridgedVariablePlugin final : public host::IPlugin {
public:
    BridgedVariablePlugin(std::unique_ptr<VariablePlugin> inner, std::shared_ptr<GameVariableBridge> bridge)
        : inner_(std::move(inner)), bridge_(std::move(bridge)) {
        bridge_->attach(inner_.get());
    }
    ~BridgedVariablePlugin() override { bridge_->detach(); }

    const host::PluginInfo& info() const override { return inner_->info(); }
    host::IPluginHttp*      http() override { return inner_->http(); }
    host::IPluginSocket*    socket() override { return inner_->socket(); }
    host::IPluginHealth*    health() override { return inner_->health(); }
    host::IPluginView*      view() override { return inner_->view(); }

private:
    std::unique_ptr<VariablePlugin>     inner_;
    std::shared_ptr<GameVariableBridge> bridge_;
};

} // namespace

std::optional<plugins::lesson::GameVariable> GameVariableBridge::find(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!plugin_) return std::nullopt;
    for (const auto& v : plugin_->vars()) {
        if (v.name == name) return plugins::lesson::GameVariable{v.kind, plugins::deep_copy(v.value)};
    }
    return std::nullopt;
}

bool GameVariableBridge::request_set(const std::string& name, const plugins::JsonValue& value) {
    std::lock_guard<std::mutex> lk(mtx_);
    return plugin_ && plugin_->request_set(name, value);
}

void GameVariableBridge::attach(VariablePlugin* plugin) {
    std::lock_guard<std::mutex> lk(mtx_);
    plugin_ = plugin;
}

void GameVariableBridge::detach() {
    std::lock_guard<std::mutex> lk(mtx_);
    plugin_ = nullptr;
}

std::unique_ptr<host::IPlugin> make_bridged_variable_plugin(std::shared_ptr<GameVariableBridge> bridge, const host::LogFn&) {
    return std::make_unique<BridgedVariablePlugin>(std::make_unique<VariablePlugin>(), std::move(bridge));
}

} // namespace ergo::studio::app
