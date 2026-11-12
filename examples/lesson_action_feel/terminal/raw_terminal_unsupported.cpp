// Windows: the playable terminal front-end is not implemented (a console
// raw-input backend would need ReadConsoleInput and virtual-terminal output,
// and could not be run where this sample was written). open() fails loudly so
// `--play` stops with this message instead of doing nothing; the headless
// modes (--check, the Studio link) do not use this file.
#include "raw_terminal.h"

namespace lesson_action_feel::terminal {

RawTerminal::~RawTerminal() = default;

std::string RawTerminal::open() {
    return "--play is not available on Windows in this build (the terminal front-end is POSIX only; "
           "use the headless mode or WSL)";
}

void RawTerminal::close() {}

std::string RawTerminal::read_available() { return {}; }

bool RawTerminal::write_all(std::string_view) { return false; }

bool RawTerminal::stop_requested() { return false; }

} // namespace lesson_action_feel::terminal
