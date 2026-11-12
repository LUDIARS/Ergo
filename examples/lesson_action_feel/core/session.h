#pragma once
/// lesson_action_feel — one running game: the simulation (ActionGame) and its
/// presentation (Presentation), stepped together. What every front-end holds.
#include "action_game.h"
#include "presentation.h"

namespace lesson_action_feel {

class GameSession {
public:
    explicit GameSession(FeelTuning tuning = {}, GameRules rules = {}, PresentationTuning fx = {})
        : game_(tuning, rules), fx_(fx) {}

    ActionGame::Frame frame(float real_dt, const PlayerCommand& cmd, const FeelToggles& toggles,
                            const CombatSettings& settings, ergo::world_time::Engine& world);
    GameView view(const CombatSettings& settings, const FeelToggles& toggles) const;
    void     restart() { game_.restart(); }

    const ActionGame&   game() const { return game_; }
    const Presentation& fx() const { return fx_; }

private:
    ActionGame   game_;
    Presentation fx_;
};

} // namespace lesson_action_feel
