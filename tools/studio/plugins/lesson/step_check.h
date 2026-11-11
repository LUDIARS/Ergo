#pragma once
/// A step's automatic check ("the game's variable X is > 0") and its
/// evaluation against what the game currently publishes. Pure.
#include "game_variables.h"

#include <optional>
#include <string>

namespace ergo::studio::plugins::lesson {

enum class CheckOp { Gt, Ge, Lt, Le, Eq, Ne };

/// "gt" / "ge" / "lt" / "le" / "eq" / "ne"; nullopt for anything else.
std::optional<CheckOp> parse_check_op(const std::string& op);

struct StepCheck {
    std::string variable;
    CheckOp     op = CheckOp::Eq;
    JsonValue   value;   ///< number, bool or string
};

/// gt / ge / lt / le: both sides numeric (variable kind int32 / int64 / float / double).
/// eq / ne: number with number, bool with bool, string with string.
/// A missing variable or a kind mismatch is "not satisfied" (also for ne), never an error.
/// int64 values travel as JSON numbers, so they compare as doubles (exact up to 2^53).
bool check_satisfied(const StepCheck& check, const std::optional<GameVariable>& variable);

} // namespace ergo::studio::plugins::lesson
