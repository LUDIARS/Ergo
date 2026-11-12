#pragma once
/// lesson_action_feel — the command line (every switch can be set at start).
#include "combat_settings.h"
#include "feel_tuning.h"

#include <cstdint>
#include <string>

namespace lesson_action_feel {

enum class Mode {
    Auto,    ///< headless: the scripted player plays endless waves, status lines on stdout (default)
    Play,    ///< the terminal game a person plays
    Check,   ///< offline self-check, no Studio
    Help,
};

struct Options {
    Mode          mode = Mode::Auto;
    std::string   host = "127.0.0.1";
    std::uint16_t port = 5170;
    bool          studio       = true;    ///< connect to the Studio (--no-studio: do not)
    bool          plain        = false;   ///< the three feel effects start OFF
    bool          endless      = false;   ///< --play: waves never end (Auto is always endless)
    bool          autopilot    = false;   ///< --play: the scripted player plays (endless); keys still flip switches
    double        seconds      = 0.0;     ///< 0 = until stopped
    double        status_every = 2.0;
    CombatSettings     combat;
    KnockbackDirection knockback_direction = KnockbackDirection::AwayFromAttacker;
};

/// "" when every argument is understood, else what is wrong with the first one that is not.
std::string parse_options(int argc, char** argv, Options& o);

FeelToggles initial_toggles(const Options& o);
FeelTuning  initial_tuning(const Options& o);

extern const char* const kUsage;

} // namespace lesson_action_feel
