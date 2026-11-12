#include "presentation.h"

#include "motion_blur.h"

#include <algorithm>
#include <cmath>

namespace lesson_action_feel {

namespace {

constexpr float kTwoPi = 6.2831853f;

template <class Fn>
void for_each_fighter(const ActionGame& g, Fn&& fn) {
    fn(g.player());
    for (std::size_t i = 0; i < g.enemy_count(); ++i) fn(g.enemy(i));
}

} // namespace

Vec2 Presentation::motion_of(int id) const {
    const auto it = motion_.find(id);
    return it == motion_.end() ? Vec2{} : it->second;
}

void Presentation::update(const ActionGame& game, const ActionGame::Frame& frame, float real_dt,
                          const FeelToggles& toggles) {
    // ---- motion vectors (always; the debug view and the Studio read them) ----
    if (game.epoch() != epoch_) {   // a restart: ids start over, nothing continues
        epoch_ = game.epoch();
        prev_pos_.clear();
        flash_.clear();
        popups_.clear();
        telegraphing_.clear();
        marked_.clear();
        last_hit_target_ = -1;
        hp_seen_ = game.player().health.hp();
        hp_trail_ = static_cast<float>(hp_seen_);
    }
    std::map<int, Vec2> now;
    motion_.clear();
    for_each_fighter(game, [&](const Combatant& c) {
        const auto it = prev_pos_.find(c.id);
        motion_[c.id] = it == prev_pos_.end() ? Vec2{} : c.pos - it->second;
        now[c.id]     = c.pos;
    });
    prev_pos_.swap(now);

    // ---- camera shake, sparks, damage numbers (real time) ----
    shake_ *= std::exp(-t_.shake_decay * real_dt);
    if (shake_ < 1e-3f) shake_ = 0.0f;
    shake_clock_ += real_dt;
    for (auto& [id, left] : flash_) left = std::max(0.0f, left - real_dt);
    for (DamagePopup& p : popups_) p.age += real_dt;
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
                                 [this](const DamagePopup& p) { return p.age >= t_.popup_seconds; }),
                  popups_.end());
    for (const HitEvent& h : frame.hits) {
        if (h.target_side == Side::Enemy) last_hit_target_ = h.target_id;
        if (h.roar) continue;   // a roar is not a strike: no spark, no number, no shake
        if (toggles[Effect::CameraShake]) {
            const float a = std::min(h.outcome.power * t_.shake_per_power, t_.shake_max);
            shake_        = std::max(shake_, a);
            observed_.camera_shake_last_amplitude = a;
        }
        if (toggles[Effect::HitSpark]) {
            flash_[h.target_id] = t_.hit_flash_seconds;
            ++observed_.hit_spark_count;
        }
        if (toggles[Effect::DamageNumbers] && h.outcome.damage > 0) {
            popups_.push_back({h.target_pos, h.outcome.damage, 0.0f});
            ++observed_.damage_numbers_shown;
        }
    }
    if (!toggles[Effect::CameraShake]) shake_ = 0.0f;
    if (!toggles[Effect::HitSpark]) flash_.clear();
    if (!toggles[Effect::DamageNumbers]) popups_.clear();

    // ---- HP gauge: the lost part stays, then drains (real time) ----
    const std::int32_t hp = game.player().health.hp();
    if (hp < hp_seen_) {
        if (toggles[Effect::HpGauge]) {
            gauge_hold_ = t_.gauge_hold;
            observed_.hp_gauge_last_drain = hp_trail_ - static_cast<float>(hp);
        }
    }
    hp_seen_ = hp;
    if (!toggles[Effect::HpGauge] || hp_trail_ < static_cast<float>(hp)) {
        hp_trail_ = static_cast<float>(hp);
    } else if (gauge_hold_ > 0.0f) {
        gauge_hold_ -= real_dt;
    } else {
        hp_trail_ = std::max(static_cast<float>(hp), hp_trail_ - t_.gauge_drain * real_dt);
    }

    // ---- telegraph and defeat marks (counted once each) ----
    std::set<int> tele;
    for_each_fighter(game, [&](const Combatant& c) {
        const bool winding = c.side == Side::Enemy && c.action == Action::Attack && c.phase == AttackPhase::WindUp && !c.weapon.roar;
        if (winding && toggles[Effect::Telegraph]) {
            tele.insert(c.id);
            if (telegraphing_.count(c.id) == 0) ++observed_.telegraph_count;
        }
        if (c.action == Action::Dead && c.side == Side::Enemy && toggles[Effect::DefeatMark] && marked_.insert(c.id).second)
            ++observed_.defeat_mark_count;
    });
    telegraphing_.swap(tele);

    // ---- boss blur ----
    blur_ = boss_blur_.update(game.boss(), real_dt, toggles[Effect::BossBlur], t_.boss_blur);
    observed_.boss_blur_last_peak = boss_blur_.last_peak();

    // ---- motion blur: the longest trail this frame ----
    if (toggles[Effect::MotionBlur]) {
        float longest = 0.0f;
        for (const auto& [id, mv] : motion_) longest = std::max(longest, motion_streak(mv, t_.motion_blur).length);
        if (longest > 0.0f) observed_.motion_blur_last_length = longest;
    }
}

void Presentation::decorate(GameView& v, const FeelToggles& toggles) const {
    if (toggles[Effect::CameraShake] && shake_ > 0.0f) {
        const float a   = static_cast<float>(shake_clock_) * kTwoPi;
        v.camera_offset = {shake_ * std::sin(a * 13.0f), shake_ * std::cos(a * 9.0f)};
    }
    v.camera_motion = {};
    v.blur          = blur_;
    if (toggles[Effect::DamageNumbers]) v.popups = popups_;
    v.player_hp_trail = v.fighters.empty() ? 0 : std::max(v.fighters.front().hp, static_cast<std::int32_t>(std::lround(hp_trail_)));
    for (FighterView& f : v.fighters) {
        f.motion = motion_of(f.id);
        if (toggles[Effect::MotionBlur]) {
            const Streak s = motion_streak(f.motion, t_.motion_blur);
            if (s.any()) f.streak = s.dir * s.length;
        }
        const auto fl = flash_.find(f.id);
        f.spark     = toggles[Effect::HitSpark] && fl != flash_.end() && fl->second > 0.0f;
        f.telegraph = telegraphing_.count(f.id) > 0;
        f.visible   = f.action != Action::Dead || f.side == Side::Player || toggles[Effect::DefeatMark];
    }
}

} // namespace lesson_action_feel
