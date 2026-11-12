#pragma once
/// lesson_action_feel — the read-only picture of one moment of the game that
/// a front-end draws (the seam between the core and the terminal / a future
/// Pictor window). Front-ends never touch the core's state; they get this and
/// send a PlayerCommand back.
#include "combatant.h"
#include "vec2.h"

#include <cstdint>
#include <vector>

namespace lesson_action_feel {

enum class GameStatus : std::uint8_t { Playing, Won, Lost };

struct FighterView {
    int         id = 0;
    Side        side = Side::Enemy;
    Vec2        pos;
    Vec2        facing;
    float       radius = 0.5f;
    std::int32_t hp = 0;
    std::int32_t max_hp = 0;
    Action      action = Action::Ready;
    AttackPhase phase  = AttackPhase::WindUp;
    bool        armored_now = false;   ///< super armor would absorb a hit right now
    bool        spark       = false;   ///< it was hit within the last hit_flash_seconds (real time)
    const char* weapon      = "";
    std::int32_t weapon_damage = 0;
    float       weapon_power = 0.0f;
    float       reach   = 0.0f;
    float       arc_cos = 0.0f;
};

struct GameView {
    float arena_width  = 0.0f;
    float arena_height = 0.0f;
    std::vector<FighterView> fighters;   ///< [0] is the player; then live and dying enemies
    Vec2  camera_offset;                 ///< camera shake in world units; the renderer shifts the viewport by it
    bool  frozen = false;                ///< the last frame's world dt was 0 (a hit stop is running)
    int   wave = 0;
    int   waves_to_win = 0;              ///< 0 = endless
    bool  between_waves = false;
    GameStatus   status = GameStatus::Playing;
    std::int64_t defeated = 0;
    std::int64_t hits     = 0;           ///< hits the player landed
    std::int64_t score    = 0;
    std::int32_t combo    = 0;
    double world_seconds = 0.0;
    double real_seconds  = 0.0;
};

} // namespace lesson_action_feel
