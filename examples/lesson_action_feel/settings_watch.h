#pragma once
/// lesson_action_feel — notices every change of a switch, whoever made it
/// (a key, the Studio, the lesson's presenter tool), and words it as
/// "<variable>: <before> -> <after>" for the log or the HUD.
#include "combat_settings.h"
#include "feel_tuning.h"

#include <string>
#include <vector>

namespace lesson_action_feel {

class SettingsWatch {
public:
    SettingsWatch(const FeelToggles& t, const CombatSettings& s) : toggles_(t), settings_(s) {}

    /// Changes since the last call (or construction), then remembers the new values.
    std::vector<std::string> changes(const FeelToggles& t, const CombatSettings& s);

private:
    FeelToggles    toggles_;
    CombatSettings settings_;
};

/// One line with every switch, e.g. for the start-up log.
std::string describe_settings(const FeelToggles& t, const CombatSettings& s);

} // namespace lesson_action_feel
