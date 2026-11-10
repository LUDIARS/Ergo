#include "breed_direction.h"

#include "../vfx_suggest/vfx_sanitize.h"
#include "breed_genes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ergo::studio::plugins::vfx_breed {
namespace {

/// JS Number(): numbers, numeric strings, booleans.
std::optional<double> js_number(const JsonValue& v) {
    if (v.is_number()) return std::isfinite(v.n) ? std::optional<double>(v.n) : std::nullopt;
    if (v.is_bool()) return v.b ? 1.0 : 0.0;
    if (v.is_string()) {
        char* e = nullptr;
        const double d = std::strtod(v.s.c_str(), &e);
        if (!v.s.empty() && e == v.s.c_str() + v.s.size() && std::isfinite(d)) return d;
    }
    return std::nullopt;
}

} // namespace

std::string build_direction_prompt(const std::string& direction, const JsonValue& current) {
    std::string gene_lines;
    for (const auto& g : genes()) gene_lines += (gene_lines.empty() ? "" : "\n") + std::string("- ") + g.id + ": " + g.label;
    return "You steer an interactive evolution of a 2D particle effect.\n"
           "The user looked at the current effect and wrote how the next generation should change.\n"
           "Answer which genes to push and in which direction. Do NOT output config values.\n"
           "\n"
           "Genes (each is normalised to 0..1 over its range):\n" +
           gene_lines +
           "\n\n"
           "Current effect (JSON):\n" +
           to_json(current) +
           "\n\n"
           "User direction: " +
           direction +
           "\n\n"
           "Reply with JSON only: {\"weights\": {\"<gene id>\": <number in -1..1>, ...}}.\n"
           "Positive raises the gene, negative lowers it. Include only the genes that matter (usually 2-6).";
}

std::optional<DirectionWeights> parse_weights(const std::string& text) {
    JsonValue data;
    if (!ergo::common::jsonm::parse(vfx_suggest::extract_json(text), data)) return std::nullopt;
    if (!data.is_object()) return DirectionWeights{};
    const JsonValue* w = data.find("weights");
    const JsonValue& raw = w && w->is_object() ? *w : data;
    DirectionWeights out;
    for (const auto& [k, v] : *raw.o) {
        const auto n = js_number(v);
        if (gene_index(k) >= 0 && n) out[k] = *n;
    }
    return out;
}

std::vector<double> to_bias(const DirectionWeights& w) {
    std::vector<double> out;
    for (const auto& g : genes()) {
        const auto it = w.find(g.id);
        out.push_back(it == w.end() || !std::isfinite(it->second) ? 0 : std::min(1.0, std::max(-1.0, it->second)));
    }
    return out;
}

JsonValue to_json_value(const DirectionWeights& w) {
    JsonValue o = JsonValue::make_object();
    for (const auto& [k, v] : w) o.set(k, jnum(v));
    return o;
}

} // namespace ergo::studio::plugins::vfx_breed
