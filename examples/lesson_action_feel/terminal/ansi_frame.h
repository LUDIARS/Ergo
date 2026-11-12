#pragma once
/// lesson_action_feel terminal — the bytes that put a frame of rows on the
/// screen: cursor home, each row with "erase to end of line", CR LF between
/// rows (raw mode turns off the newline translation), then "erase below".
/// Redrawing in place (no clear) keeps the picture from flickering.
#include <string>
#include <vector>

namespace lesson_action_feel::terminal {

std::string compose_frame(const std::vector<std::string>& rows);

/// Sent once when the game takes the terminal: hide the cursor, clear the screen.
std::string enter_sequence();
/// Sent when it gives the terminal back: show the cursor, leave the cursor below the picture.
std::string leave_sequence();

} // namespace lesson_action_feel::terminal
