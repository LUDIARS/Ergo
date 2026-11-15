#pragma once
#include "experiment_presets.h"
#include "runtime/rule_module.h"
#include "session.h"

namespace lesson_action_feel {
// Keyboard actions for matched, independently reversible trials; no platform input code here.
class ExperimentControls {
public:
    explicit ExperimentControls(const Options& options) : options_(options) {}
    void select(Experiment preset, GameSession& session, FeelToggles& toggles, CombatSettings& settings);
    void reload(GameSession& session, FeelToggles& toggles, CombatSettings& settings);
    void rollback(GameSession& session, FeelToggles& toggles, CombatSettings& settings);
    void restart(GameSession& session, FeelToggles& toggles, CombatSettings& settings);
    void reject(GameSession& session, FeelToggles& toggles, CombatSettings& settings, const std::string& reason);
    const std::wstring& message() const { return message_; }
private:
    Options options_;
    Experiment selected_ = Experiment::Baseline;
    RuleModule module_;
    std::wstring message_ = L"BASELINE / F1 baseline / F2-4 traps / F5 reload C++ / F6 rollback";
};
} // namespace lesson_action_feel
