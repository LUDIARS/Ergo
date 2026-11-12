#include "action_game.h"

#include "boss_brain.h"
#include "wave_plan.h"

#include <algorithm>
#include <cmath>

namespace lesson_action_feel {

namespace {

ergo::score::Config score_config() {
    ergo::score::Config c;
    c.combo_multiplier = true;
    c.combo_factor     = 0.1f;
    return c;
}

const SideSettings& side_of(const CombatSettings& s, Side side) {
    return side == Side::Player ? s.player : s.enemy;
}

bool swinging_active(const Combatant& c) { return c.action == Action::Attack && c.phase == AttackPhase::Active; }

/// The roar reaches every live body within its reach (all around).
bool roar_reaches(const Combatant& roarer, const Combatant& target) {
    return (target.pos - roarer.pos).length() <= roarer.weapon.reach + target.radius;
}

} // namespace

ActionGame::ActionGame(FeelTuning tuning, GameRules rules) : tuning_(tuning), rules_(rules), score_(score_config()) {
    restart();
}

void ActionGame::restart() {
    fighters_.clear();
    brains_.clear();
    weapon_index_   = 0;
    switch_pending_ = false;
    Combatant p     = make_combatant(0, Side::Player, {tuning_.arena_width * 0.5f, tuning_.arena_height * 0.5f},
                                     tuning_.player_weapons[0], tuning_.player_hp, tuning_.player_speed, tuning_.radius);
    p.mass = tuning_.mass;
    fighters_.push_back(p);
    brains_.emplace_back();
    next_id_ = 1;
    ++epoch_;
    score_.reset();
    combo_.reset();
    since_player_hit_  = 0.0f;
    status_            = GameStatus::Playing;
    wave_              = std::max(1, rules_.start_wave) - 1;
    between_waves_     = false;
    intermission_left_ = 0.0f;
    defeated_          = 0;
    player_hits_       = 0;
    spawn_wave();
}

void ActionGame::spawn_wave() {
    ++wave_;
    const int  n    = wave_size(wave_, rules_.endless, tuning_);
    const bool boss = is_boss_wave(wave_, tuning_);
    for (const Spawn& s : spawn_points(n, player().pos, tuning_)) {
        Combatant e = boss ? make_combatant(next_id_++, Side::Enemy, s.pos, tuning_.boss_roar, tuning_.boss_hp,
                                            tuning_.boss_speed, tuning_.boss_radius)
                           : make_combatant(next_id_++, Side::Enemy, s.pos, tuning_.enemy_weapon, tuning_.enemy_hp,
                                            tuning_.enemy_speed, tuning_.radius);
        e.mass = boss ? tuning_.boss_mass : tuning_.mass;
        if (boss) {
            e.is_boss          = true;
            e.min_flinch_power = tuning_.boss_min_flinch_power;
            e.techniques       = {tuning_.boss_roar, tuning_.boss_charge};
        }
        e.spawn_slot = s.slot;
        e.facing     = (player().pos - s.pos).normalized();
        fighters_.push_back(e);
        BrainState b;
        b.cooldown = tuning_.enemy_first_delay + tuning_.enemy_delay_step * static_cast<float>(s.slot);
        brains_.push_back(b);
    }
}

const Combatant* ActionGame::boss() const {
    for (std::size_t i = 1; i < fighters_.size(); ++i)
        if (fighters_[i].is_boss) return &fighters_[i];
    return nullptr;
}

const BrainState* ActionGame::boss_brain() const {
    for (std::size_t i = 1; i < fighters_.size(); ++i)
        if (fighters_[i].is_boss) return &brains_[i];
    return nullptr;
}

ActionGame::Frame ActionGame::frame(float real_dt, const PlayerCommand& cmd, const FeelToggles& toggles,
                                    const CombatSettings& settings, ergo::world_time::Engine& world) {
    Frame out;
    out.world_dt = world.update(real_dt);
    real_seconds_ += real_dt;
    world_seconds_ += out.world_dt;
    frozen_ = real_dt > 0.0f && out.world_dt <= 0.0f;

    if (status_ != GameStatus::Playing) return out;
    Combatant& p = fighters_.front();
    if (cmd.switch_weapon) switch_pending_ = true;
    if (switch_pending_ && p.action == Action::Ready) {
        switch_pending_ = false;
        weapon_index_   = (weapon_index_ + 1) % tuning_.player_weapons.size();
        p.weapon        = tuning_.player_weapons[weapon_index_];
    }
    if (out.world_dt <= 0.0f) {
        // Frozen world: nothing moves, but an attack pressed now is kept for when time runs again.
        if (cmd.intent.attack && p.is_alive()) p.buffer_left = tuning_.input_buffer;
        return out;
    }
    step_world(out.world_dt, cmd, toggles, settings, world, out);
    return out;
}

void ActionGame::step_world(float dt, const PlayerCommand& cmd, const FeelToggles& toggles,
                            const CombatSettings& settings, ergo::world_time::Engine& world, Frame& out) {
    Combatant& p = fighters_.front();
    step_action(p, cmd.intent, dt, settings.player, tuning_, combat_.player);

    int swinging = 0;   // normal enemies only: the boss does not take an attack slot
    for (std::size_t i = 1; i < fighters_.size(); ++i)
        if (fighters_[i].is_swinging() && !fighters_[i].is_boss) ++swinging;
    for (std::size_t i = 1; i < fighters_.size(); ++i) {
        Combatant& e = fighters_[i];
        if (e.is_boss) {
            const Intent in = think_boss(e, fighters_.front(), brains_[i], dt, tuning_);
            step_action(e, in, dt, settings.enemy, tuning_, combat_.enemy);
            continue;
        }
        const bool   was_swing  = e.is_swinging();
        const bool   may_attack = swinging < tuning_.enemy_max_attackers;
        const Intent in = think(e, fighters_.front(), brains_[i], settings.enemy, may_attack, dt, tuning_);
        step_action(e, in, dt, settings.enemy, tuning_, combat_.enemy);
        if (!was_swing && e.is_swinging()) ++swinging;
    }

    move_bodies(dt);
    resolve_swings(toggles, settings, world, out);

    since_player_hit_ += dt;
    if (since_player_hit_ > tuning_.combo_window) combo_.break_();

    update_waves(dt, out);
}

void ActionGame::move_bodies(float dt) {
    const float fade = std::exp(-tuning_.push_friction * dt);
    for (Combatant& c : fighters_) {
        const Vec2 step = c.push_velocity * dt;
        c.pos += step;
        if (c.side == Side::Enemy) knockback_travel_ += step.length();
        c.push_velocity *= fade;
        if (c.push_velocity.length() < 1e-3f) c.push_velocity = {};
    }
    // Bodies do not overlap: push every overlapping live pair apart in inverse proportion to their mass.
    for (std::size_t i = 0; i < fighters_.size(); ++i) {
        for (std::size_t j = i + 1; j < fighters_.size(); ++j) {
            Combatant& a = fighters_[i];
            Combatant& b = fighters_[j];
            if (!a.is_alive() || !b.is_alive()) continue;
            const Vec2  d       = b.pos - a.pos;
            const float dist    = d.length();
            const float overlap = a.radius + b.radius - dist;
            if (overlap <= 0.0f) continue;
            const Vec2  n     = dist > 1e-4f ? d * (1.0f / dist) : Vec2{1.0f, 0.0f};
            const float share = b.mass / (a.mass + b.mass);
            a.pos -= n * (overlap * share);
            b.pos += n * (overlap * (1.0f - share));
        }
    }
    for (Combatant& c : fighters_) {
        c.pos.x = std::clamp(c.pos.x, c.radius, tuning_.arena_width - c.radius);
        c.pos.y = std::clamp(c.pos.y, c.radius, tuning_.arena_height - c.radius);
    }
}

void ActionGame::land(Combatant& attacker, Combatant& target, const HitOutcome& hit, bool roar,
                      ergo::world_time::Engine& world, Frame& out) {
    if (hit.hit_stop_seconds > 0.0f) {
        world.hit_stop(hit.hit_stop_seconds);
        if (world.is_hit_stop()) observed_.hit_stop_last_seconds = hit.hit_stop_seconds;   // the engine really froze
    }
    if (hit.impulse > 0.0f) observed_.knockback_last_impulse = hit.impulse;
    out.hits.push_back({attacker.id, target.id, target.side, target.pos, hit, roar});
    if (attacker.side == Side::Player) {
        ++out.player_hits;
        ++player_hits_;
        combo_.hit();
        since_player_hit_ = 0.0f;
        score_.add(hit.damage, combo_.count());
        if (hit.killed) {
            ++out.defeated;
            ++defeated_;
            score_.add(target.is_boss ? tuning_.boss_points : tuning_.defeat_points, combo_.count());
        }
    } else {
        ++out.enemy_hits;
        combo_.break_();
        if (hit.killed) status_ = GameStatus::Lost;
    }
}

void ActionGame::resolve_swings(const FeelToggles& toggles, const CombatSettings& settings,
                                ergo::world_time::Engine& world, Frame& out) {
    // The player's swing is resolved first: on a trade in the same frame the player's hit lands first.
    for (std::size_t a = 0; a < fighters_.size(); ++a) {
        if (!swinging_active(fighters_[a])) continue;
        for (std::size_t b = 0; b < fighters_.size(); ++b) {
            Combatant& attacker = fighters_[a];
            Combatant& target   = fighters_[b];
            if (!swinging_active(attacker)) break;   // interrupted
            if (target.side == attacker.side || !target.is_alive()) continue;
            if (std::find(attacker.struck.begin(), attacker.struck.end(), target.id) != attacker.struck.end()) continue;
            const bool roar = attacker.weapon.roar;
            if (roar ? !roar_reaches(attacker, target) : !swing_touches(attacker, target)) continue;
            attacker.struck.push_back(target.id);
            attacker.attack_landed = true;
            SideCounters& counters = target.side == Side::Player ? combat_.player : combat_.enemy;
            const SideSettings& ts = side_of(settings, target.side);
            const HitOutcome hit   = roar ? resolve_roar(attacker, target, ts, counters)
                                          : resolve_hit(attacker, target, ts, toggles, tuning_, counters);
            land(attacker, target, hit, roar, world, out);
        }
    }
}

void ActionGame::update_waves(float dt, Frame& out) {
    // Corpses leave after corpse_seconds of world time (a rule: it decides when the next wave may come).
    for (std::size_t i = fighters_.size(); i-- > 1;) {
        if (fighters_[i].action == Action::Dead && fighters_[i].dead_left <= 0.0f) {
            fighters_.erase(fighters_.begin() + static_cast<std::ptrdiff_t>(i));
            brains_.erase(brains_.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    if (status_ != GameStatus::Playing || fighters_.size() > 1) return;
    if (!between_waves_) {
        const int to_win = waves_to_win(rules_.endless, tuning_);
        if (to_win > 0 && wave_ >= to_win) {
            status_ = GameStatus::Won;
            return;
        }
        between_waves_     = true;
        intermission_left_ = tuning_.intermission;
        return;
    }
    intermission_left_ -= dt;
    if (intermission_left_ > 0.0f) return;
    between_waves_ = false;
    spawn_wave();
    out.wave_started = true;
}

GameView ActionGame::view(const CombatSettings& settings) const {
    GameView v;
    v.arena_width  = tuning_.arena_width;
    v.arena_height = tuning_.arena_height;
    for (const Combatant& c : fighters_) {
        FighterView f;
        f.id            = c.id;
        f.side          = c.side;
        f.boss          = c.is_boss;
        f.pos           = c.pos;
        f.facing        = c.facing;
        f.radius        = c.radius;
        f.hp            = c.health.hp();
        f.max_hp        = c.health.max_hp();
        f.action        = c.action;
        f.phase         = c.phase;
        f.armored_now   = armor_applies(c, side_of(settings, c.side));
        f.weapon        = c.weapon.name;
        f.weapon_damage = c.weapon.damage;
        f.weapon_power  = c.weapon.power;
        f.reach         = c.weapon.reach;
        f.arc_cos       = c.weapon.arc_cos;
        f.roar          = c.weapon.roar;
        f.charge        = c.weapon.charge;
        v.fighters.push_back(f);
    }
    v.frozen        = frozen_;
    v.wave          = wave_;
    v.waves_to_win  = waves_to_win(rules_.endless, tuning_);
    v.boss_wave     = is_boss_wave(wave_, tuning_);
    v.between_waves = between_waves_;
    v.status        = status_;
    v.defeated      = defeated_;
    v.hits          = player_hits_;
    v.score         = score_.score();
    v.combo         = combo_.count();
    v.world_seconds = world_seconds_;
    v.real_seconds  = real_seconds_;
    return v;
}

} // namespace lesson_action_feel
