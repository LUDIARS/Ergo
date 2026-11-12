#include "session.h"

namespace lesson_action_feel {

ActionGame::Frame GameSession::frame(float real_dt, const PlayerCommand& cmd, const FeelToggles& toggles,
                                     const CombatSettings& settings, ergo::world_time::Engine& world) {
    ActionGame::Frame f = game_.frame(real_dt, cmd, toggles, settings, world);
    fx_.update(game_, f, real_dt, toggles);
    return f;
}

GameView GameSession::view(const CombatSettings& settings, const FeelToggles& toggles) const {
    GameView v = game_.view(settings);
    fx_.decorate(v, toggles);
    return v;
}

} // namespace lesson_action_feel
