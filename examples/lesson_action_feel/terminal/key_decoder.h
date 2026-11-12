#pragma once
/// lesson_action_feel terminal — turns the bytes a terminal in raw mode sends
/// into keys: printable characters, the arrow keys (ESC [ A..D and ESC O A..D)
/// and Ctrl+C (0x03, which raw mode delivers as a byte instead of SIGINT).
/// An escape sequence split across two reads is completed by the next feed().
#include <string>
#include <string_view>
#include <vector>

namespace lesson_action_feel::terminal {

enum class KeyKind { Char, Up, Down, Left, Right, CtrlC };

struct Key {
    KeyKind kind = KeyKind::Char;
    char    ch   = 0;   ///< for KeyKind::Char
    bool operator==(const Key& o) const { return kind == o.kind && ch == o.ch; }
};

class KeyDecoder {
public:
    /// Decode `bytes` (plus whatever an earlier call left incomplete).
    std::vector<Key> feed(std::string_view bytes);
    /// Forget a pending incomplete sequence (a lone ESC press), e.g. once per frame without input.
    void flush() { pending_.clear(); }

private:
    std::string pending_;
};

} // namespace lesson_action_feel::terminal
