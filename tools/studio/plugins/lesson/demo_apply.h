#pragma once
/// Presenter role: which demo features have to be switched in the running
/// game, and which of those switches are still waiting. No HTTP and no
/// threads; the game is reached through IGameVariables only.
#include "game_variables.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::lesson {

enum class DemoStatus {
    NotCued,          ///< the timeline has not set this feature yet
    Applied,          ///< the set request reached the game that binds the variable
    WaitingForGame,   ///< no connected game binds the variable; retried on every apply()
    NotBool,          ///< the variable is bound, but not as a bool; retried on every apply()
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
    /// Take the server's view. A new `seq` makes every cued feature pending
    /// again; within one seq an already applied switch is not repeated.
    void observe(uint64_t seq, const std::map<std::string, bool>& states,
                 const std::map<std::string, std::string>& features);
    /// Request the pending switches. Call after observe(), once per poll.
    void apply(IGameVariables& variables);
    void reset();

    /// Every known feature (sorted by name), then cued features without a variable.
    const std::vector<DemoEntry>& entries() const { return entries_; }

private:
    std::optional<uint64_t> seq_;
    std::vector<DemoEntry>  entries_;
};

} // namespace ergo::studio::plugins::lesson
