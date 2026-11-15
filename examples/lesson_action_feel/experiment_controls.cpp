#include "experiment_controls.h"

namespace lesson_action_feel {
void ExperimentControls::select(Experiment preset, GameSession& session, FeelToggles& toggles, CombatSettings& settings) {
    const auto setup = experiment_setup(preset, options_, module_.current());
    // Build replacement before changing the running trial. Assignment preserves addresses used
    // by LessonBindings; the new simulation and presentation start with fresh observations.
    GameSession replacement(setup.feel, initial_rules(options_, options_.endless || options_.autopilot), setup.presentation);
    ergo::world_time::Engine::instance().force_stop();
    session = std::move(replacement);
    toggles = initial_toggles(options_);
    settings = options_.combat;
    selected_ = preset;
    message_ = setup.label;
}
void ExperimentControls::reload(GameSession& session, FeelToggles& toggles, CombatSettings& settings) {
    std::string error;
    if (!module_.reload(error)) {
        message_ = L"RELOAD FAILED / previous trial kept: " + std::wstring(error.begin(), error.end());
        return;
    }
    select(Experiment::Custom, session, toggles, settings);
}
void ExperimentControls::rollback(GameSession& session, FeelToggles& toggles, CombatSettings& settings) {
    if (!module_.rollback()) { message_ = L"No loaded rule to roll back / F1 baseline"; return; }
    select(module_.current() ? Experiment::Custom : Experiment::Baseline, session, toggles, settings);
    message_ += L" / rolled back";
}
void ExperimentControls::restart(GameSession& session, FeelToggles& toggles, CombatSettings& settings) {
    select(selected_, session, toggles, settings);
}
void ExperimentControls::reject(GameSession& session, FeelToggles& toggles, CombatSettings& settings, const std::string& reason) {
    select(Experiment::Baseline, session, toggles, settings);
    message_ = L"RULE ERROR / baseline restored, paused: " + std::wstring(reason.begin(), reason.end());
}
} // namespace lesson_action_feel
