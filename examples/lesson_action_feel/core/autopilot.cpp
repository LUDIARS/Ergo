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
    // Get out of the way of the boss's telegraphed big moves (its roar and its charge).
    for (std::size_t i = 1; i < view.fighters.size(); ++i) {
        const FighterView& b = view.fighters[i];
        if (!b.boss || b.action != Action::Attack || b.phase == AttackPhase::Recovery || !(b.roar || b.charge)) continue;
        const Vec2 away = me.pos - b.pos;
        if (away.length() <= b.reach + me.radius + 1.0f) {
            cmd.intent.move = away.is_zero() ? Vec2{1.0f, 0.0f} : away;
            return cmd;
        }
    }
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
