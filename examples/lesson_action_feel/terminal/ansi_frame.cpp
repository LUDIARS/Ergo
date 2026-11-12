#include "ansi_frame.h"

namespace lesson_action_feel::terminal {

std::string compose_frame(const std::vector<std::string>& rows) {
    std::string out = "\x1b[H";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) out += "\r\n";
        out += rows[i];
        out += "\x1b[K";
    }
    out += "\x1b[J";
    return out;
}

std::string enter_sequence() { return "\x1b[?25l\x1b[2J\x1b[H"; }

std::string leave_sequence() { return "\x1b[?25h\r\n"; }

} // namespace lesson_action_feel::terminal
