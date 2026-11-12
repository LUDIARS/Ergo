#pragma once
/// lesson_action_feel — the "what really happened" value of each registry
/// entry (EffectInfo::observation), read from a running session. 0 means the
/// effect has not happened (it stays 0 while the effect is off).
#include "feel_effects.h"
#include "session.h"

namespace lesson_action_feel {

double effect_observation(Effect e, const GameSession& s);

} // namespace lesson_action_feel
