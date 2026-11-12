#include "options.h"

#include <cstdlib>
#include <cstring>

namespace lesson_action_feel {

const char* const kUsage =
    "usage: ergo_example_lesson_action_feel [--auto] [--host H] [--port P] [--no-studio] [--plain]\n"
    "                                       [--seconds S] [--status-every S] [combat options]\n"
    "       ergo_example_lesson_action_feel --play [--endless | --autopilot] [--host H] [--port P] [--no-studio] [--plain]\n"
    "                                       [--seconds S] [combat options]\n"
    "       ergo_example_lesson_action_feel --check\n"
    "       ergo_example_lesson_action_feel --help | -h\n"
    "\n"
    "Arena action game for the Odeum lesson. Publishes feel.hit_stop, feel.knockback,\n"
    "feel.camera_shake, feel.super_armor and the combat.* settings (writable) and what really\n"
    "happened (feel.hit_stop_last_seconds, feel.knockback_last_impulse, combat.*.flinches, ...)\n"
    "to the Studio's variable plugin at ws://H:P/variable/ws.\n"
    "\n"
    "modes:\n"
    "  --auto             (default) headless: a scripted player fights endless waves; status lines on stdout\n"
    "  --play             play it in this terminal (POSIX terminals; keys are shown on screen)\n"
    "  --check            offline self-check without a Studio (exit 0 = the observations follow the switches)\n"
    "  --help, -h         print this text and exit\n"
    "\n"
    "options:\n"
    "  --host H           Studio host (default 127.0.0.1)\n"
    "  --port P           Studio port, 1..65535 (default 5170; the Studio's PORT)\n"
    "  --no-studio        do not connect to a Studio\n"
    "  --plain            start with hit stop, knockback and camera shake OFF\n"
    "  --endless          --play: waves never end (default: clear 3 waves to win)\n"
    "  --autopilot        --play: the scripted player plays endless waves on screen (for a demo);\n"
    "                     keys 1-9, P, R and Q still work\n"
    "  --seconds S        stop after S seconds of real time (default 0 = until stopped)\n"
    "  --status-every S   --auto: status lines every S seconds, 0.1..3600 (default 2)\n"
    "\n"
    "combat options (per side; the same switches as keys 4-9 and the combat.* variables):\n"
    "  --enemy-flinch on|off              default on\n"
    "  --enemy-super-armor off|attack|always   default off (attack = during wind-up and swing)\n"
    "  --super-armor                      same as --enemy-super-armor attack\n"
    "  --enemy-cancel on|off              motion cancel on hit, default off\n"
    "  --player-flinch on|off             default on\n"
    "  --player-super-armor off|attack|always  default off\n"
    "  --player-cancel on|off             default on\n"
    "  --knockback-dir away|backward      away from the attacker (default) or the target's backward direction\n";

namespace {

bool parse_number(const char* s, double lo, double hi, double& out) {
    char*        end = nullptr;
    const double v   = std::strtod(s, &end);
    if (end == s || *end != '\0' || v < lo || v > hi) return false;
    out = v;
    return true;
}

bool parse_on_off(const char* s, bool& out) {
    if (std::strcmp(s, "on") == 0) out = true;
    else if (std::strcmp(s, "off") == 0) out = false;
    else return false;
    return true;
}

bool parse_armor(const char* s, SuperArmor& out) {
    if (std::strcmp(s, "off") == 0) out = SuperArmor::Off;
    else if (std::strcmp(s, "attack") == 0) out = SuperArmor::Attack;
    else if (std::strcmp(s, "always") == 0) out = SuperArmor::Always;
    else return false;
    return true;
}

bool parse_direction(const char* s, KnockbackDirection& out) {
    if (std::strcmp(s, "away") == 0) out = KnockbackDirection::AwayFromAttacker;
    else if (std::strcmp(s, "backward") == 0) out = KnockbackDirection::TargetBackward;
    else return false;
    return true;
}

bool takes_value(const std::string& a) {
    static const char* const kWithValue[] = {"--host", "--port", "--seconds", "--status-every", "--enemy-flinch",
                                             "--enemy-super-armor", "--enemy-cancel", "--player-flinch",
                                             "--player-super-armor", "--player-cancel", "--knockback-dir"};
    for (const char* o : kWithValue)
        if (a == o) return true;
    return false;
}

/// Parse the value of `a`; false = bad value.
bool parse_value(const std::string& a, const char* v, Options& o) {
    double n = 0.0;
    if (a == "--host") {
        o.host = v;
        return *v != '\0';
    }
    if (a == "--port") {
        if (!parse_number(v, 1, 65535, n)) return false;
        o.port = static_cast<std::uint16_t>(n);
        return true;
    }
    if (a == "--seconds") return parse_number(v, 0, 1e9, o.seconds);
    if (a == "--status-every") return parse_number(v, 0.1, 3600, o.status_every);
    if (a == "--enemy-flinch") return parse_on_off(v, o.combat.enemy.flinch);
    if (a == "--player-flinch") return parse_on_off(v, o.combat.player.flinch);
    if (a == "--enemy-cancel") return parse_on_off(v, o.combat.enemy.motion_cancel);
    if (a == "--player-cancel") return parse_on_off(v, o.combat.player.motion_cancel);
    if (a == "--enemy-super-armor") return parse_armor(v, o.combat.enemy.super_armor);
    if (a == "--player-super-armor") return parse_armor(v, o.combat.player.super_armor);
    if (a == "--knockback-dir") return parse_direction(v, o.knockback_direction);
    return false;
}

std::string set_mode(Options& o, Mode m, bool& mode_given, const std::string& a) {
    if (mode_given && o.mode != m) return "option " + a + " conflicts with an earlier mode option";
    o.mode     = m;
    mode_given = true;
    return {};
}

} // namespace

std::string parse_options(int argc, char** argv, Options& o) {
    bool mode_given = false;
    bool help       = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        std::string       err;
        if (a == "--help" || a == "-h") help = true;
        else if (a == "--check") err = set_mode(o, Mode::Check, mode_given, a);
        else if (a == "--play") err = set_mode(o, Mode::Play, mode_given, a);
        else if (a == "--auto") err = set_mode(o, Mode::Auto, mode_given, a);
        else if (a == "--plain") o.plain = true;
        else if (a == "--endless") o.endless = true;
        else if (a == "--autopilot") o.autopilot = true;
        else if (a == "--no-studio") o.studio = false;
        else if (a == "--super-armor") o.combat.enemy.super_armor = SuperArmor::Attack;
        else if (takes_value(a)) {
            if (i + 1 >= argc) return "option " + a + " needs a value";
            const char* v = argv[++i];
            if (!parse_value(a, v, o)) return "bad value for " + a + ": \"" + v + "\"";
        } else {
            return "unknown option \"" + a + "\"";
        }
        if (!err.empty()) return err;
    }
    if (o.autopilot && o.mode != Mode::Play) return "option --autopilot needs --play";
    if (help) o.mode = Mode::Help;
    return {};
}

FeelToggles initial_toggles(const Options& o) {
    return o.plain ? FeelToggles{false, false, false} : FeelToggles{};
}

FeelTuning initial_tuning(const Options& o) {
    FeelTuning t;
    t.knockback_direction = o.knockback_direction;
    return t;
}

} // namespace lesson_action_feel
