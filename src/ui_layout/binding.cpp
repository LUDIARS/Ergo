#include "ergo/ui_layout/ui_layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ergo::ui_layout {
namespace {

double as_num(const BindValue& v) {
    if (v.kind == BindValue::Kind::Number) return v.number;
    if (v.kind == BindValue::Kind::Bool) return v.boolean ? 1.0 : 0.0;
    if (v.kind == BindValue::Kind::String) {
        try { return std::stod(v.string); } catch (...) { return 0.0; }
    }
    return 0.0;
}

bool as_bool(const BindValue& v) {
    if (v.kind == BindValue::Kind::Bool) return v.boolean;
    if (v.kind == BindValue::Kind::Number) return std::abs(v.number) > 1e-6;
    return !v.string.empty();
}

std::string as_str(const BindValue& v) {
    if (v.kind == BindValue::Kind::String) return v.string;
    if (v.kind == BindValue::Kind::Bool) return v.boolean ? "true" : "false";
    return std::to_string(v.number);
}

std::string trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Apply a transform mini-spec to node.rect. The rect is fully recomputed from
// node.base_rect every call, so the result is idempotent across frames and
// independent of any previous spec (channels not mentioned this frame fall back
// to the authored base). Tokens are whitespace-separated; each writes distinct
// rect fields, so their order is irrelevant:
//   translate(dx,dy) -> rect.x = base.x + dx; rect.y = base.y + dy
//   translate(dx)    -> dy defaults to 0
//   scale(sx,sy)     -> rect.w = base.w * sx; rect.h = base.h * sy
//   scale(s)         -> uniform (sx = sy = s)
// Malformed / unknown tokens (e.g. rotate(...)) are ignored; the rect then
// stays at the base. Do not combine `transform` with `scale_x` on one node.
void apply_transform(Node& node, const std::string& spec) {
    node.rect = node.base_rect;
    size_t pos = 0;
    while (pos < spec.size()) {
        const auto open = spec.find('(', pos);
        if (open == std::string::npos) break;
        const auto close = spec.find(')', open);
        if (close == std::string::npos) break;
        const std::string name = trim(spec.substr(pos, open - pos));
        const std::string args = spec.substr(open + 1, close - open - 1);
        double a = 0.0, b = 0.0;
        const int n = std::sscanf(args.c_str(), " %lf , %lf", &a, &b);
        if (name == "translate" && n >= 1) {
            node.rect.x = node.base_rect.x + static_cast<float>(a);
            node.rect.y = node.base_rect.y + (n >= 2 ? static_cast<float>(b) : 0.0f);
        } else if (name == "scale" && n >= 1) {
            const float sx = static_cast<float>(a);
            const float sy = (n >= 2) ? static_cast<float>(b) : sx;
            node.rect.w = node.base_rect.w * sx;
            node.rect.h = node.base_rect.h * sy;
        }
        pos = close + 1;
    }
}

BindValue eval_expr(const std::string& expr, const BindContext& ctx) {
    const std::string e = trim(expr);
    const auto q = e.find('?');
    const auto c = e.find(':');
    if (q != std::string::npos && c != std::string::npos && q < c) {
        const auto cond = trim(e.substr(0, q));
        const auto texp = trim(e.substr(q + 1, c - q - 1));
        const auto fexp = trim(e.substr(c + 1));
        return as_bool(eval_expr(cond, ctx)) ? eval_expr(texp, ctx) : eval_expr(fexp, ctx);
    }
    if (e.rfind("fmt_mmss(", 0) == 0 && e.back() == ')') {
        const auto arg = trim(e.substr(9, e.size() - 10));
        int secs = static_cast<int>(as_num(eval_expr(arg, ctx)));
        if (secs < 0) secs = 0;
        const int mm = secs / 60;
        const int ss = secs % 60;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
        return BindValue::from_string(buf);
    }
    if (e.rfind("clamp(", 0) == 0 && e.back() == ')') {
        auto args = e.substr(6, e.size() - 7);
        const size_t p1 = args.find(',');
        const size_t p2 = args.find(',', p1 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos) {
            const auto v = as_num(eval_expr(args.substr(0, p1), ctx));
            const auto lo = as_num(eval_expr(args.substr(p1 + 1, p2 - p1 - 1), ctx));
            const auto hi = as_num(eval_expr(args.substr(p2 + 1), ctx));
            return BindValue::from_number(std::clamp(v, lo, hi));
        }
    }
    for (const char* op : {"==", "!=", ">=", "<=", ">", "<"}) {
        const auto pos = e.find(op);
        if (pos != std::string::npos) {
            const auto l = as_num(eval_expr(e.substr(0, pos), ctx));
            const auto r = as_num(eval_expr(e.substr(pos + std::strlen(op)), ctx));
            if (std::string(op) == "==") return BindValue::from_bool(std::abs(l - r) < 1e-6);
            if (std::string(op) == "!=") return BindValue::from_bool(std::abs(l - r) >= 1e-6);
            if (std::string(op) == ">=") return BindValue::from_bool(l >= r);
            if (std::string(op) == "<=") return BindValue::from_bool(l <= r);
            if (std::string(op) == ">") return BindValue::from_bool(l > r);
            return BindValue::from_bool(l < r);
        }
    }
    if (e.size() >= 2 && ((e.front() == '"' && e.back() == '"') || (e.front() == '\'' && e.back() == '\''))) {
        return BindValue::from_string(e.substr(1, e.size() - 2));
    }
    if (e == "true") return BindValue::from_bool(true);
    if (e == "false") return BindValue::from_bool(false);
    if (!e.empty() && (std::isdigit(static_cast<unsigned char>(e[0])) || e[0] == '-' || e[0] == '.')) {
        try { return BindValue::from_number(std::stod(e)); } catch (...) {}
    }
    auto it = ctx.find(e);
    if (it != ctx.end()) return it->second;
    return BindValue::from_number(0.0);
}

} // namespace

void Document::apply_binds_(Node& node, const BindContext& ctx) {
    for (const auto& b : node.binds) {
        const BindValue v = eval_expr(b.expr, ctx);
        if (b.op == "text" && b.target == "self") node.text_value = as_str(v);
        else if (b.op == "scale_x") node.rect.w = node.base_rect.w * static_cast<float>(as_num(v));
        else if (b.op == "opacity") node.opacity = static_cast<float>(as_num(v));
        else if (b.op == "visible") node.visible = as_bool(v);
        else if ((b.op == "color" || b.op == "fill_color") && v.kind == BindValue::Kind::String) node.color = v.string;
        else if (b.op == "transform") {
            // expr resolves to a transform mini-spec string (e.g. a BindContext
            // variable holding "translate(0,-8) scale(1.1)"); numeric/bool
            // results stringify to something apply_transform safely ignores.
            apply_transform(node, as_str(v));
        }
    }
    for (auto& c : node.children) apply_binds_(c, ctx);
}

} // namespace ergo::ui_layout
