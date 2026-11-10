#include "breed_request.h"

#include "../particle/particle_schema.h"
#include "breed_rng.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <set>

namespace ergo::studio::plugins::vfx_breed {
namespace {

/// Number of a field as the TS did: a number, or a non-blank numeric string.
std::optional<double> num(const JsonValue* v) {
    if (!v) return std::nullopt;
    if (v->is_number()) return std::isfinite(v->n) ? std::optional<double>(v->n) : std::nullopt;
    if (v->is_string() && v->s.find_first_not_of(" \t\r\n") != std::string::npos) {
        char* e = nullptr;
        const double d = std::strtod(v->s.c_str(), &e);
        if (std::string(e).find_first_not_of(" \t\r\n") == std::string::npos && std::isfinite(d)) return d;
    }
    return std::nullopt;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// First `n` code points of UTF-8 text.
std::string take_code_points(const std::string& s, std::size_t n) {
    std::size_t i = 0, cps = 0;
    while (i < s.size() && cps < n) {
        const auto c = static_cast<unsigned char>(s[i]);
        i += c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        ++cps;
    }
    return s.substr(0, std::min(i, s.size()));
}

double to_uint32(double v) {
    const double m = std::fmod(std::floor(v), 4294967296.0);
    return m < 0 ? m + 4294967296.0 : m;
}

} // namespace

std::variant<GenerationRequest, std::string> parse_generation_request(const JsonValue& b,
                                                                      const std::function<uint32_t()>& seed_source) {
    if (!b.is_object()) return std::string("request body must be a JSON object");
    const JsonValue* all = b.find("parents");
    if (all && !all->is_null() && !all->is_array()) return std::string("parents must be an array of configs");
    const std::vector<JsonValue> list = all && all->is_array() ? *all->a : std::vector<JsonValue>{};
    for (const auto& p : list) {
        if (!p.is_object()) return std::string("every parent must be a config object");
    }

    GenerationRequest r;
    r.parents = list;
    if (const JsonValue* sel = b.find("selected"); sel && !sel->is_null()) {
        if (!sel->is_array()) return std::string("selected must be indices into parents");
        std::vector<JsonValue> picked;
        std::set<std::size_t>  seen;
        for (const auto& i : *sel->a) {
            if (!i.is_number() || std::floor(i.n) != i.n || i.n < 0 || i.n >= static_cast<double>(list.size())) {
                return std::string("selected must be indices into parents");
            }
            const auto idx = static_cast<std::size_t>(i.n);
            if (seen.insert(idx).second) picked.push_back(list[idx]);
        }
        r.parents = std::move(picked);
    }
    if (r.parents.empty()) r.parents.push_back(particle::default_effect());

    const double count = js_round(num(b.find("count")).value_or(kCountDefault));
    r.count            = static_cast<int>(std::min<double>(kCountMax, std::max<double>(kCountMin, count)));
    r.strength         = std::min(1.0, std::max(0.0, num(b.find("strength")).value_or(kStrengthDefault)));
    const auto seed    = num(b.find("seed"));
    r.seed             = seed ? to_uint32(*seed) : static_cast<double>(seed_source());
    r.generation       = static_cast<int>(std::max(0.0, std::floor(num(b.find("generation")).value_or(0))));
    if (const JsonValue* d = b.find("direction"); d && d->is_string()) r.direction = take_code_points(trim(d->s), kDirectionMaxChars);
    return r;
}

} // namespace ergo::studio::plugins::vfx_breed
