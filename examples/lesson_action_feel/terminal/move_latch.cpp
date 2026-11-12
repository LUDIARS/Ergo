#include "move_latch.h"

namespace lesson_action_feel::terminal {

void MoveLatch::press(Command c) {
    switch (c) {
    case Command::MoveUp: dir_ = {0.0f, -1.0f}; break;
    case Command::MoveDown: dir_ = {0.0f, 1.0f}; break;
    case Command::MoveLeft: dir_ = {-1.0f, 0.0f}; break;
    case Command::MoveRight: dir_ = {1.0f, 0.0f}; break;
    case Command::Attack: attack_ = true; return;
    default: return;
    }
    left_ = hold_;
}

Intent MoveLatch::take(float real_dt) {
    Intent in;
    if (left_ > 0.0f) in.move = dir_;
    left_ -= real_dt;
    in.attack = attack_;
    attack_   = false;
    return in;
}

} // namespace lesson_action_feel::terminal
