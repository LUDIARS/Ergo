#pragma once
#include "game_view.h"
#include "feel_effects.h"
#include "combat_settings.h"
#include <windows.h>
#include <string>

namespace lesson_action_feel::graphical {
struct Viewport {
    float scale = 1, left = 0, top = 0;
    Vec2 world(POINT p) const { return {(p.x - left) / scale, (p.y - top) / scale}; }
};
Viewport viewport(int width, int height, const GameView& view);
struct ArenaHud {
    FeelToggles toggles;
    CombatSettings settings;
    bool paused = false, effects = false, vectors = false;
    std::wstring message;
};
void paint_arena(HDC dc, int width, int height, const GameView& view,
                 const ArenaHud& hud);
} // namespace lesson_action_feel::graphical
