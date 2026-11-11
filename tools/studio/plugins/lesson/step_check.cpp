#include "step_check.h"

namespace ergo::studio::plugins::lesson {
namespace {

bool is_numeric_kind(const std::string& kind) {
    return kind == "int32" || kind == "int64" || kind == "float" || kind == "double";
}

template <class T>
bool compare(CheckOp op, const T& actual, const T& expected) {
    switch (op) {
    case CheckOp::Gt: return actual > expected;
    case CheckOp::Ge: return actual >= expected;
    case CheckOp::Lt: return actual < expected;
    case CheckOp::Le: return actual <= expected;
    case CheckOp::Eq: return actual == expected;
    case CheckOp::Ne: return actual != expected;
    }
    return false;
}

bool is_equality(CheckOp op) { return op == CheckOp::Eq || op == CheckOp::Ne; }

} // namespace

std::optional<CheckOp> parse_check_op(const std::string& op) {
    if (op == "gt") return CheckOp::Gt;
    if (op == "ge") return CheckOp::Ge;
    if (op == "lt") return CheckOp::Lt;
    if (op == "le") return CheckOp::Le;
    if (op == "eq") return CheckOp::Eq;
    if (op == "ne") return CheckOp::Ne;
    return std::nullopt;
}

bool check_satisfied(const StepCheck& check, const std::optional<GameVariable>& variable) {
    if (!variable) return false;
    const JsonValue& actual   = variable->value;
    const JsonValue& expected = check.value;
    if (expected.is_number()) {
        if (!is_numeric_kind(variable->kind) || !actual.is_number()) return false;
        return compare(check.op, actual.n, expected.n);
    }
    if (!is_equality(check.op)) return false;   // ordering needs numbers
    if (expected.is_bool()) {
        if (variable->kind != "bool" || !actual.is_bool()) return false;
        return compare(check.op, actual.b, expected.b);
    }
    if (expected.is_string()) {
        if (variable->kind != "string" || !actual.is_string()) return false;
        return compare(check.op, actual.s, expected.s);
    }
    return false;
}

} // namespace ergo::studio::plugins::lesson
