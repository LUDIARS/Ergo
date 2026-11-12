#include "combat_settings.h"

namespace lesson_action_feel {

const char* super_armor_name(SuperArmor a) {
    switch (a) {
    case SuperArmor::Off: return "off";
    case SuperArmor::Attack: return "attack";
    case SuperArmor::Always: return "always";
    }
    return "?";
}

} // namespace lesson_action_feel
