#include "autopilot.h"

namespace lesson_action_feel {

PlayerCommand autopilot(const GameView& view) {
    PlayerCommand cmd;
    if (view.fighters.empty() || view.status != GameStatus::Playing) return cmd;
    const FighterView& me = view.fighters.front();
    const FighterView* target = nullptr;
    float best = 0.0f;
    for (std::size_t i = 1; i < view.fighters.size(); ++i) {
        const FighterView& e = view.fighters[i];
        if (e.action == Action::Dead) continue;
        const float d = (e.pos - me.pos).length();
        if (target == nullptr || d < best) {
            target = &e;
            best   = d;
        }
    }
    if (target == nullptr) return cmd;
    const Vec2 to = target->pos - me.pos;
    if (best > me.reach * 0.8f + target->radius) {
        cmd.intent.move = to;
    } else {
        cmd.intent.aim    = to;
        cmd.intent.attack = true;
    }
    return cmd;
}

} // namespace lesson_action_feel
