#pragma once
/// lesson_action_feel — the read-only picture of one moment of the game that
/// a front-end draws (the seam between the core and the terminal / a future
/// Pictor window). ActionGame::view fills the simulation part; Presentation
/// adds the effects (each only while its registry switch is on). Front-ends
/// never touch the core's state; they get this and send a PlayerCommand back.
#include "combatant.h"
#include "vec2.h"

#include <cstdint>
#include <vector>

namespace lesson_action_feel {

enum class GameStatus : std::uint8_t { Playing, Won, Lost };

enum class BlurKind : std::uint8_t {
    None,
    Radial,   ///< the boss's roar: streaks spreading out from the boss
    Zoom,     ///< the boss's charge: streaks converging on the boss from the edges
};

/// The boss blur: one full-screen effect (not per object).
struct ScreenBlur {
    float    strength = 0.0f;   ///< 0..1
    BlurKind kind     = BlurKind::None;
    Vec2     centre;            ///< world position (the boss)
};

struct DamagePopup {
    Vec2         pos;           ///< where the hit landed (world)
    std::int32_t amount = 0;
    float        age    = 0.0f; ///< real s since the hit
};

struct FighterView {
    int          id = 0;
    Side         side = Side::Enemy;
    bool         boss = false;
    Vec2         pos;
    Vec2         facing;
    float        radius = 0.5f;
    std::int32_t hp = 0;
    std::int32_t max_hp = 0;
    Action       action = Action::Ready;
    AttackPhase  phase  = AttackPhase::WindUp;
    bool         armored_now = false;   ///< super armor would absorb a hit right now
    const char*  weapon      = "";
    std::int32_t weapon_damage = 0;
    float        weapon_power  = 0.0f;
    float        reach   = 0.0f;
    float        arc_cos = 0.0f;
    bool         roar    = false;       ///< its current technique is the roar
    bool         charge  = false;       ///< its current technique is the charge attack

    // ---- presentation (Presentation fills these) --------------------------
    bool  visible   = true;    ///< false: a corpse while feel.defeat_mark is off
    bool  spark     = false;   ///< hit within the last hit_flash_seconds (feel.hit_spark)
    bool  telegraph = false;   ///< draw the reach of its coming swing (feel.telegraph)
    Vec2  motion;              ///< motion vector: screen-space displacement since the last frame (world units)
    Vec2  streak;              ///< motion-blur trail: from the body backwards (zero = none; feel.motion_blur)
};

struct GameView {
    float arena_width  = 0.0f;
    float arena_height = 0.0f;
    std::vector<FighterView> fighters;   ///< [0] is the player; then live and dying enemies
    bool  frozen = false;                ///< the last frame's world dt was 0 (a hit stop is running)
    int   wave = 0;
    int   waves_to_win = 0;              ///< 0 = endless
    bool  boss_wave = false;
    bool  between_waves = false;
    GameStatus   status = GameStatus::Playing;
    std::int64_t defeated = 0;
    std::int64_t hits     = 0;           ///< hits the player landed
    std::int64_t score    = 0;
    std::int32_t combo    = 0;
    double world_seconds = 0.0;
    double real_seconds  = 0.0;

    // ---- presentation -------------------------------------------------------
    Vec2         camera_offset;          ///< camera shake (world units); the renderer shifts the viewport by it
    Vec2         camera_motion;          ///< the camera's own motion vector (always zero: the camera does not scroll)
    ScreenBlur   blur;                   ///< boss blur
    std::vector<DamagePopup> popups;     ///< damage numbers
    std::int32_t player_hp_trail = 0;    ///< HP gauge: the drained-to value drawn behind the HP (>= HP while draining)
};

} // namespace lesson_action_feel
