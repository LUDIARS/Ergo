#include "vfx_catalog.h"

#include <set>
#include <stdexcept>

extern const char        ergo_vfx_catalog_json[];
extern const std::size_t ergo_vfx_catalog_json_size;

namespace ergo::studio::plugins::vfx_suggest {
namespace {

struct Data {
    std::vector<VfxPreset>                                    presets;
    std::vector<std::pair<std::string, std::vector<std::string>>> hints;
};

/// The embedded file is part of the build: a broken one is a bug here, so
/// it throws (the built-in plugin then fails to load, loudly).
const Data& data() {
    static const Data d = [] {
        JsonValue root;
        if (!ergo::common::jsonm::parse(std::string(ergo_vfx_catalog_json, ergo_vfx_catalog_json_size), root) ||
            !field(root, "presets") || !field(root, "hints")) {
            throw std::runtime_error("vfx_suggest: embedded catalog.json is broken");
        }
        Data out;
        for (const auto& p : *field(root, "presets")->a) {
            out.presets.push_back({field(p, "name")->as_string(), field(p, "rationale")->as_string(), *field(p, "config")});
        }
        for (const auto& [game, list] : *field(root, "hints")->o) {
            std::vector<std::string> names;
            for (const auto& n : *list.a) names.push_back(n.as_string());
            out.hints.emplace_back(game, std::move(names));
        }
        return out;
    }();
    return d;
}

} // namespace

JsonValue to_json_value(const VfxPreset& p) {
    return jobj({{"name", jstr(p.name)}, {"rationale", jstr(p.rationale)}, {"config", p.config}});
}

const std::vector<std::pair<std::string, std::string>>& game_labels() {
    static const std::vector<std::pair<std::string, std::string>> v{
        {"ks", "PrivateGame"}, {"ac", "AdventureCube"}, {"ul", "UniLand"}, {"generic", "\xE6\xB1\x8E\xE7\x94\xA8"}};
    return v;
}

bool is_game_id(const std::string& id) {
    for (const auto& [k, label] : game_labels()) {
        if (k == id) return true;
    }
    return false;
}

const std::vector<VfxPreset>& catalog() { return data().presets; }

std::vector<VfxPreset> catalog_examples(const std::string& game, std::size_t n) {
    std::vector<const VfxPreset*> ordered;
    for (const auto& [g, names] : data().hints) {
        if (g != game) continue;
        for (const auto& name : names) {
            for (const auto& p : catalog()) {
                if (p.name == name) ordered.push_back(&p);
            }
        }
    }
    for (const auto& p : catalog()) ordered.push_back(&p);
    std::set<std::string>  seen;
    std::vector<VfxPreset> out;
    for (const VfxPreset* p : ordered) {
        if (out.size() >= n) break;
        if (seen.insert(p->name).second) out.push_back(*p);
    }
    return out;
}

} // namespace ergo::studio::plugins::vfx_suggest
