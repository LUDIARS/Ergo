#include "key_decoder.h"

namespace lesson_action_feel::terminal {

namespace {

bool arrow(char c, KeyKind& out) {
    switch (c) {
    case 'A': out = KeyKind::Up; return true;
    case 'B': out = KeyKind::Down; return true;
    case 'C': out = KeyKind::Right; return true;
    case 'D': out = KeyKind::Left; return true;
    default: return false;
    }
}

} // namespace

std::vector<Key> KeyDecoder::feed(std::string_view bytes) {
    pending_.append(bytes.data(), bytes.size());
    std::vector<Key> keys;
    std::size_t      i = 0;
    while (i < pending_.size()) {
        const char c = pending_[i];
        if (c == '\x1b') {
            if (i + 1 >= pending_.size()) break;   // maybe the rest comes with the next read
            const char intro = pending_[i + 1];
            if (intro != '[' && intro != 'O') {    // lone ESC followed by a normal key: drop the ESC
                ++i;
                continue;
            }
            if (i + 2 >= pending_.size()) break;
            KeyKind k = KeyKind::Char;
            if (arrow(pending_[i + 2], k)) keys.push_back({k, 0});
            i += 3;   // unknown sequences (function keys ...) are skipped by their first 3 bytes
            continue;
        }
        if (c == '\x03') keys.push_back({KeyKind::CtrlC, 0});
        else if (c >= 0x20 && c < 0x7f) keys.push_back({KeyKind::Char, c});
        else if (c == '\t') keys.push_back({KeyKind::Char, '\t'});
        ++i;
    }
    pending_.erase(0, i);
    return keys;
}

} // namespace lesson_action_feel::terminal
