#pragma once
/// lesson_action_feel terminal — draws one frame as rows of ASCII text from
/// the game's read-only view and the HUD state. Pure: no terminal I/O.
///
/// Arena: 1 world unit = 2 columns x 1 row (terminal cells are about twice as
/// tall as wide). The whole arena, walls included, is shifted by the camera
/// shake offset inside a fixed margin; the HUD above and below never moves.
/// Glyphs: @ you (& while flinching, an arrow shows where you face),
/// e enemy (E while its super armor applies), ! enemy winding up (its reach is
/// dotted: the telegraph), ~ flinching, x defeated; + your swing, : an enemy
/// swing, * hit spark (drawn on real time, so it stays during a hit stop).
#include "combat_settings.h"
#include "feel_tuning.h"
#include "game_view.h"

#include <cstddef>
#include <string>
#include <vector>

namespace lesson_action_feel::terminal {

struct HudState {
    FeelToggles    toggles;
    CombatSettings settings;
    CombatObserved observed;
    bool           paused    = false;
    bool           show_help = true;
    std::string    message;   ///< the last event worth telling (a switch changed, a wave, ...)
    std::string    link;      ///< Studio link state, e.g. "Studio linked"
};

constexpr int kColsPerUnit = 2;
constexpr int kMarginCols  = 2;
constexpr int kMarginRows  = 1;
constexpr std::size_t kHudRows = 6;   ///< rows above the arena
/// Every row fits an 80-column terminal; the whole screen is 24 rows with the default 30 x 12 arena.
constexpr std::size_t kMaxColumns = 80;

std::vector<std::string> render_screen(const GameView& view, const HudState& hud);

} // namespace lesson_action_feel::terminal
