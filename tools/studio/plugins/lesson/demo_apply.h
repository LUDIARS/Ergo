#pragma once
/// Presenter role: keeps the running game's demo features where the lesson
/// wants them. On every poll the value the game publishes is compared with
/// the wanted one, and a set is requested again whenever they differ (the
/// game restarted, something else flipped it, an earlier request was lost).
/// No HTTP and no threads; the game is reached through IGameVariables only.
#include "game_variables.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::lesson {

enum class DemoStatus {
    NotCued,          ///< the timeline has not set this feature yet
    Applied,          ///< the game publishes the wanted value
    Requested,        ///< the game published another value; a set was requested at this poll
    NotFollowing,     ///< a set was already requested at an earlier poll and the game still publishes another value
                      ///< (a read-only variable, or the game changes it back); requested again, once per poll
    WaitingForGame,   ///< no connected game binds the variable; looked up again on every apply()
    NotBool,          ///< the variable is bound, but not as a bool; looked up again on every apply()
    NoVariable,       ///< the lesson cued a feature that `features` does not map to a variable
};

struct DemoEntry {
    std::string         feature;
    std::string         variable;   ///< empty for NoVariable
    std::optional<bool> on;         ///< what the timeline wants; nullopt for NotCued
    DemoStatus          status = DemoStatus::NotCued;
};
bool operator==(const DemoEntry& a, const DemoEntry& b);

class DemoApplier {
public:
    /// Take the server's view (`demo.states` is the whole wanted state, so the
    /// cue counter is not needed here). A feature whose variable and wanted
    /// value did not change keeps its status, so a game that does not follow
    /// is still recognised as such.
    void observe(const std::map<std::string, bool>& states, const std::map<std::string, std::string>& features);
    /// Compare every cued feature with what the game publishes and request a
    /// set for each one that differs: at most one request per feature per call.
    /// Call after observe(), once per poll.
    void apply(IGameVariables& variables);
    void reset();

    /// Every known feature (sorted by name), then cued features without a variable.
    const std::vector<DemoEntry>& entries() const { return entries_; }

private:
    std::vector<DemoEntry> entries_;
};

} // namespace ergo::studio::plugins::lesson
