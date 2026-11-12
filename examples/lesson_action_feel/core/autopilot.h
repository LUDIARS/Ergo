#pragma once
/// lesson_action_feel — the scripted player of the headless mode (and of the
/// tests): walk to the nearest live enemy and keep attacking it with the
/// current weapon. Deterministic: the command depends only on the view.
#include "action_game.h"
#include "game_view.h"

namespace lesson_action_feel {

PlayerCommand autopilot(const GameView& view);

} // namespace lesson_action_feel
