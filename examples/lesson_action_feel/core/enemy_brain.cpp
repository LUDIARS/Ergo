#include "enemy_brain.h"

#include "combat_rules.h"

namespace lesson_action_feel {

Intent think(const Combatant& self, const Combatant& player, BrainState& brain, const SideSettings& side,
             bool may_attack, float dt, const FeelTuning& t) {
    brain.cooldown -= dt;
    Intent in;
    if (!self.is_alive() || !player.is_alive()) return in;

    const Vec2  to       = player.pos - self.pos;
    const bool  in_reach = to.length() <= self.weapon.reach * 0.9f + player.radius;

    if (self.action == Action::Attack) {
        if (can_cancel(self, side) && in_reach && may_attack && brain.chain < t.enemy_chain_max) {
            ++brain.chain;
            in.aim    = to;
            in.attack = true;   // step_action cancels the recovery into this attack
        }
        return in;
    }
    if (self.action != Action::Ready) return in;

    if (!in_reach) {
        in.move = to;
        return in;
    }
    in.aim = to;
    if (brain.cooldown <= 0.0f && may_attack) {
        brain.cooldown = t.enemy_attack_cooldown;
        brain.chain    = 0;
        in.attack      = true;
    }
    return in;
}

} // namespace lesson_action_feel
