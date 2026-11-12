/// lesson_action_feel — the Odeum lesson's sample: a small arena action game
/// (core/) with the three feel effects and the per-side combat concepts
/// (flinch, super armor, motion cancel), one core and several front-ends:
///
///   (default) --auto   headless: a scripted player, Studio link, status lines (headless_mode.cpp)
///   --play             the terminal game a person plays (play_mode.cpp, terminal/)
///   --check            offline self-check (check_mode.cpp)
///
/// Every switch is settable at start (options.cpp), from the Studio
/// (lesson_bindings.cpp) and, in --play, with a key (terminal/key_bindings.cpp).
/// Spec: spec/feature/tool/lesson-action-game.md.
#include "modes.h"
#include "options.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    using namespace lesson_action_feel;
    Options opt;
    if (const std::string err = parse_options(argc, argv, opt); !err.empty()) {
        std::fprintf(stderr, "ergo_example_lesson_action_feel: %s (--help lists the options)\n", err.c_str());
        return 2;
    }
    switch (opt.mode) {
    case Mode::Help: std::fputs(kUsage, stdout); return 0;
    case Mode::Check: return run_check();
    case Mode::Play: return run_play(opt);
    case Mode::Auto: break;
    }
    return run_headless(opt);
}
