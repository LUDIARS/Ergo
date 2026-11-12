#pragma once
/// lesson_action_feel terminal — turns key presses into a per-frame Intent.
///
/// A terminal reports key presses (and the keyboard's auto-repeat) but never a
/// key release. So a direction key walks for `hold_seconds` of real time after
/// its last press: a tap is a short step, holding the key walks continuously
/// once the keyboard's auto-repeat starts (after its repeat delay). A new
/// direction replaces the old one at once.
#include "key_bindings.h"

#include "combatant.h"

namespace lesson_action_feel::terminal {

class MoveLatch {
public:
    explicit MoveLatch(float hold_seconds = 0.12f) : hold_(hold_seconds) {}

    /// Feed one command (only the four moves and Attack matter).
    void press(Command c);
    /// The intent for a frame of `real_dt`; consumes this frame's attack press.
    Intent take(float real_dt);

private:
    float hold_;
    Vec2  dir_;
    float left_   = 0.0f;
    bool  attack_ = false;
};

} // namespace lesson_action_feel::terminal
