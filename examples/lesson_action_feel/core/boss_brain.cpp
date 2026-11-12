#include "boss_brain.h"

namespace lesson_action_feel {

int boss_next_technique(const BrainState& brain) { return brain.chain % 3 == 0 ? kBossRoar : kBossCharge; }

Intent think_boss(const Combatant& self, const Combatant& player, BrainState& brain, float dt, const FeelTuning& t) {
    brain.cooldown -= dt;
    Intent in;
    if (!self.is_alive() || !player.is_alive() || self.action != Action::Ready) return in;
    const Vec2  to   = player.pos - self.pos;
    const float dist = to.length();
    const int   next = boss_next_technique(brain);
    const Weapon& w  = next == kBossRoar ? t.boss_roar : t.boss_charge;
    const float reach = next == kBossRoar ? w.reach : w.reach * 0.9f + player.radius;
    if (dist > reach) {
        in.move = to;
        return in;
    }
    in.aim = to;
    if (brain.cooldown > 0.0f) return in;
    brain.cooldown = t.boss_cooldown + w.windup + w.active + w.recovery;
    ++brain.chain;   // used here as the position in the pattern
    in.attack    = true;
    in.technique = next;
    return in;
}

} // namespace lesson_action_feel
