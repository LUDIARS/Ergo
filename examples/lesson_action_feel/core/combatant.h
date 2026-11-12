#pragma once
/// lesson_action_feel — one fighter. The player and the enemies are the same
/// type with the same action model (ready / attack with wind-up, active and
/// recovery / flinch / dead); only their settings and who drives their Intent
/// differ (the keyboard or the script for the player, EnemyBrain for enemies).
#include "feel_tuning.h"
#include "vec2.h"

#include "ergo/health/health.h"

#include <cstdint>
#include <vector>

namespace lesson_action_feel {

enum class Side : std::uint8_t { Player, Enemy };

enum class Action : std::uint8_t {
    Ready,    ///< free: may move, turn, start an attack
    Attack,   ///< committed to an attack's phases (unless motion cancel allows leaving the recovery)
    Flinch,   ///< hit and staggered: no move, no attack, no turn until flinch_left runs out
    Dead,
};

enum class AttackPhase : std::uint8_t { WindUp, Active, Recovery };

/// What the driver wants this frame. The same for keyboard, script and AI.
struct Intent {
    Vec2 move;              ///< direction to walk (normalised when used); zero = stand
    Vec2 aim;               ///< face this way without walking (zero = follow `move`)
    bool attack = false;    ///< start an attack (kept for FeelTuning::input_buffer if it cannot start yet)
};

struct Combatant {
    int    id   = 0;
    Side   side = Side::Enemy;
    Vec2   pos;
    Vec2   facing{1.0f, 0.0f};   ///< unit vector
    Vec2   push_velocity;        ///< knockback speed, fades with push_friction
    float  radius = 0.5f;
    float  speed  = 3.0f;
    Weapon weapon;
    ergo::health::Health health;

    Action      action      = Action::Ready;
    AttackPhase phase       = AttackPhase::WindUp;
    float       phase_left  = 0.0f;   ///< world s left in the current attack phase
    float       flinch_left = 0.0f;   ///< world s left in the flinch
    float       buffer_left = 0.0f;   ///< world s an early attack press is kept
    float       dead_left   = 0.0f;   ///< world s a corpse stays
    bool        attack_landed = false;   ///< the current attack hit something (opens the on-hit cancel)
    std::vector<int> struck;             ///< ids the current attack already hit (each at most once)

    float hit_flash = 0.0f;   ///< real s left of the spark drawn at this fighter
    int   spawn_slot = 0;     ///< which spawn point it came from (staggers enemy attacks)

    bool is_alive() const { return action != Action::Dead; }
    bool is_swinging() const { return action == Action::Attack && phase != AttackPhase::Recovery; }
};

Combatant make_combatant(int id, Side side, Vec2 pos, const Weapon& weapon, std::int32_t hp, float speed, float radius);

} // namespace lesson_action_feel
