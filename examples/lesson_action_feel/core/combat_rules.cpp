#include "combat_rules.h"

#include <algorithm>

namespace lesson_action_feel {

Combatant make_combatant(int id, Side side, Vec2 pos, const Weapon& weapon, std::int32_t hp, float speed, float radius) {
    Combatant c;
    c.id     = id;
    c.side   = side;
    c.pos    = pos;
    c.weapon = weapon;
    ergo::health::Config cfg;
    cfg.max_hp = hp;
    c.health   = ergo::health::Health(cfg);
    c.speed    = speed;
    c.radius   = radius;
    return c;
}

bool armor_applies(const Combatant& c, const SideSettings& s) {
    switch (s.super_armor) {
    case SuperArmor::Off: return false;
    case SuperArmor::Always: return c.is_alive();
    case SuperArmor::Attack: return c.is_swinging();
    }
    return false;
}

bool can_cancel(const Combatant& c, const SideSettings& s) {
    return s.motion_cancel && c.action == Action::Attack && c.phase == AttackPhase::Recovery && c.attack_landed;
}

namespace {

void start_attack(Combatant& c, const Intent& in) {
    const Vec2 dir = !in.aim.is_zero() ? in.aim.normalized() : in.move.normalized();
    if (!dir.is_zero()) c.facing = dir;
    c.action        = Action::Attack;
    c.phase         = AttackPhase::WindUp;
    c.phase_left    = c.weapon.windup;
    c.attack_landed = false;
    c.struck.clear();
    c.buffer_left = 0.0f;
}

/// Run the attack's phase clock; leaves Action::Ready when the recovery ends.
void advance_attack(Combatant& c, float dt) {
    c.phase_left -= dt;
    while (c.action == Action::Attack && c.phase_left <= 0.0f) {
        if (c.phase == AttackPhase::WindUp) {
            c.phase = AttackPhase::Active;
            c.phase_left += c.weapon.active;
        } else if (c.phase == AttackPhase::Active) {
            c.phase = AttackPhase::Recovery;
            c.phase_left += c.weapon.recovery;
        } else {
            c.action     = Action::Ready;
            c.phase_left = 0.0f;
        }
    }
}

} // namespace

void step_action(Combatant& c, const Intent& in, float dt, const SideSettings& s, const FeelTuning& t,
                 SideCounters& counters) {
    if (c.action == Action::Dead) {
        c.dead_left -= dt;
        return;
    }
    if (in.attack) c.buffer_left = t.input_buffer;
    else c.buffer_left = std::max(0.0f, c.buffer_left - dt);

    if (c.action == Action::Flinch) {
        c.flinch_left -= dt;
        if (c.flinch_left > 0.0f) return;
        c.flinch_left = 0.0f;
        c.action      = Action::Ready;
    }
    if (c.action == Action::Attack) {
        const bool wants_out = c.buffer_left > 0.0f || !in.move.is_zero();
        if (wants_out && can_cancel(c, s)) {
            ++counters.cancels;
            c.action = Action::Ready;   // falls through: the new attack or move starts this frame
        } else {
            advance_attack(c, dt);
            if (c.action == Action::Attack) return;
        }
    }
    // Ready
    if (c.buffer_left > 0.0f) {
        start_attack(c, in);
        return;
    }
    if (!in.move.is_zero()) {
        const Vec2 d = in.move.normalized();
        c.pos += d * (c.speed * dt);
        c.facing = d;
    } else if (!in.aim.is_zero()) {
        c.facing = in.aim.normalized();
    }
}

bool swing_touches(const Combatant& attacker, const Combatant& target) {
    const Vec2  d    = target.pos - attacker.pos;
    const float dist = d.length();
    if (dist > attacker.weapon.reach + target.radius) return false;
    if (dist < 1e-4f) return true;
    return (d * (1.0f / dist)).dot(attacker.facing) >= attacker.weapon.arc_cos;
}

Vec2 knockback_direction(const Combatant& attacker, const Combatant& target, KnockbackDirection rule) {
    if (rule == KnockbackDirection::TargetBackward) return -target.facing;
    const Vec2 away = (target.pos - attacker.pos).normalized();
    return away.is_zero() ? attacker.facing : away;
}

HitOutcome resolve_hit(const Combatant& attacker, Combatant& target, const SideSettings& target_side,
                       const FeelToggles& toggles, const FeelTuning& t, SideCounters& target_counters) {
    HitOutcome out;
    const Weapon& w            = attacker.weapon;
    const bool    armored      = armor_applies(target, target_side);   // judged at the moment of the hit
    const bool    was_attacking = target.action == Action::Attack;

    out.damage = w.damage;
    target.health.apply_damage(w.damage);
    target.hit_flash = t.hit_flash_seconds;

    if (target.health.is_dead()) {
        out.killed      = true;
        out.interrupted = was_attacking;
        target.action   = Action::Dead;
        target.dead_left = t.corpse_seconds;
    } else if (armored) {
        out.absorbed = true;
        ++target_counters.armor_absorbed;
    } else if (target_side.flinch) {
        out.flinched    = true;
        out.interrupted = was_attacking;
        ++target_counters.flinches;
        if (was_attacking) ++target_counters.interrupted;
        target.action      = Action::Flinch;
        target.flinch_left = target_side.flinch_seconds;
        target.buffer_left = 0.0f;
        target.struck.clear();
    }

    if (toggles.knockback && !armored) {
        out.impulse = w.power * t.knockback_per_power;
        target.push_velocity += knockback_direction(attacker, target, t.knockback_direction) * (out.impulse / t.mass);
    }
    if (toggles.hit_stop) out.hit_stop_seconds = w.power * t.hit_stop_per_power;
    if (toggles.camera_shake) out.shake = std::min(w.power * t.shake_per_power, t.shake_max);
    return out;
}

} // namespace lesson_action_feel
