#pragma once
/// lesson_action_feel — the three ways to run the sample. Each returns the exit code.
#include "options.h"

namespace lesson_action_feel {

int run_headless(const Options& o);   ///< --auto (default): scripted player, Studio link, status lines
int run_play(const Options& o);       ///< --play: the terminal game
int run_check();                      ///< --check: offline self-check

} // namespace lesson_action_feel
