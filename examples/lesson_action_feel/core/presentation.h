#pragma once
/// lesson_action_feel — every presentation effect of the registry except the
/// two gameplay ones: it reads the game (and the frame's hit events) and adds
/// to the GameView what the switches allow. It never writes the simulation, so
/// turning any of these off cannot change the game (tests enforce it).
///
/// Motion vectors are computed here for every fighter, whatever the switches:
/// the screen-space displacement since the previous frame = the world
/// displacement (the camera does not scroll, so its motion vector is zero).
/// The camera-shake offset is not included (it would smear everything on every
/// shake). A fighter seen for the first time (spawn, wave, restart) has a zero
/// vector; a hit stop freezes positions, so vectors are zero during it.
#include "action_game.h"
#include "boss_blur.h"
#include "feel_effects.h"
#include "game_view.h"
#include "presentation_tuning.h"

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace lesson_action_feel {

/// What the presentation effects really did (0 until the effect happened; stays 0 while it is off).
struct PresentationObserved {
    double       camera_shake_last_amplitude = 0.0;
    std::int64_t hit_spark_count             = 0;
    std::int64_t damage_numbers_shown        = 0;
    double       hp_gauge_last_drain         = 0.0;   ///< HP drawn draining after the last player hit
    std::int64_t telegraph_count             = 0;     ///< wind-ups whose reach was highlighted
    std::int64_t defeat_mark_count           = 0;
    double       boss_blur_last_peak         = 0.0;
    double       motion_blur_last_length     = 0.0;   ///< longest trail of the latest frame that drew one
};

class Presentation {
public:
    explicit Presentation(PresentationTuning t = {}) : t_(t) {}

    /// After ActionGame::frame: advance every effect by `real_dt`.
    void update(const ActionGame& game, const ActionGame::Frame& frame, float real_dt, const FeelToggles& toggles);
    /// Fill the presentation part of `v` (built by ActionGame::view).
    void decorate(GameView& v, const FeelToggles& toggles) const;

    const PresentationObserved& observed() const { return observed_; }
    Vec2  motion_of(int id) const;          ///< zero for an unknown id
    int   last_hit_target() const { return last_hit_target_; }   ///< id of the most recently hit enemy (-1 none)
    float shake() const { return shake_; }
    const PresentationTuning& tuning() const { return t_; }

private:
    PresentationTuning t_;
    PresentationObserved observed_;

    float  shake_       = 0.0f;
    double shake_clock_ = 0.0;
    std::map<int, float> flash_;     ///< id -> real s of spark left
    std::vector<DamagePopup> popups_;
    float  hp_trail_    = 0.0f;
    float  gauge_hold_  = 0.0f;
    std::int32_t hp_seen_ = 0;
    std::set<int> telegraphing_;     ///< ids whose wind-up is highlighted now
    std::set<int> marked_;           ///< corpses already counted
    BossBlurEnvelope boss_blur_;
    ScreenBlur blur_;
    std::map<int, Vec2> prev_pos_;
    std::map<int, Vec2> motion_;
    int    epoch_ = -1;
    int    last_hit_target_ = -1;
};

} // namespace lesson_action_feel
