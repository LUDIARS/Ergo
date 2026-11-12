#pragma once
/// lesson_action_feel — the one registry of presentation ("feel") effects.
///
/// Every effect the game implements is an entry here, and nothing
/// presentational is hard-wired on: each entry is a bool switch with the same
/// three entrances (CLI `--effect <name>=on|off`, the Studio variable
/// `feel.<name>`, the in-game effects screen) and one "what really happened"
/// observation that stays 0 while the effect is off. `--plain` turns every
/// entry off; `feel.all` turns all of them off or on at once.
///
/// Two entries change the simulation (`gameplay = true`): knockback moves
/// bodies and hit stop changes timing on the world clock. Every other entry is
/// drawn from the game state and must not change it (tests enforce this).
/// Combat rules (flinch, super armor, motion cancel per side) are not effects
/// and are not in this table.
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace lesson_action_feel {

enum class Effect : std::size_t {
    HitStop,         ///< freeze the world on a hit (power / 24 s)          — ActionGameSample "HitStop"
    Knockback,       ///< push the target (impulse power x 6)               — "Knockback"
    CameraShake,     ///< jitter the viewport                               — "CameraShake"
    HitSpark,        ///< sparks at the hit target                          — "ParticleEffect"
    DamageNumbers,   ///< "-20" rising above the target                     — "DamageUI"
    HpGauge,         ///< the player's HP bar drains visibly after a hit    — "HPBarEffect"
    Telegraph,       ///< the reach of a coming swing is highlighted
    DefeatMark,      ///< a defeated enemy stays drawn as x
    BossBlur,        ///< screen blur while the boss roars or charges
    MotionBlur,      ///< per-object trails along each object's motion vector
    Count
};

constexpr std::size_t kEffectCount = static_cast<std::size_t>(Effect::Count);

struct EffectInfo {
    Effect           id;
    std::string_view name;          ///< CLI / variable suffix: feel.<name>
    std::string_view observation;   ///< read-only variable, 0 until the effect really happened
    bool             gameplay;      ///< changes the simulation (only hit stop and knockback)
    std::string_view what;
};

const std::array<EffectInfo, kEffectCount>& effect_table();
const EffectInfo&                           effect_info(Effect e);
std::optional<Effect>                       effect_by_name(std::string_view name);

/// One bool per registry entry. Default: every effect on.
class FeelToggles {
public:
    FeelToggles() { on_.fill(true); }
    static FeelToggles all(bool on);

    bool  operator[](Effect e) const { return on_[static_cast<std::size_t>(e)]; }
    bool& operator[](Effect e) { return on_[static_cast<std::size_t>(e)]; }
    bool  all_on() const;
    void  set_all(bool on) { on_.fill(on); }
    bool operator==(const FeelToggles& o) const { return on_ == o.on_; }
    bool operator!=(const FeelToggles& o) const { return on_ != o.on_; }

    // The three the course started with, by name (readability at the call sites).
    bool hit_stop() const { return (*this)[Effect::HitStop]; }
    bool knockback() const { return (*this)[Effect::Knockback]; }
    bool camera_shake() const { return (*this)[Effect::CameraShake]; }

private:
    std::array<bool, kEffectCount> on_{};
};

} // namespace lesson_action_feel
