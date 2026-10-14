#include "ergo/package_model/layers.h"

#include "ergo/package_model/file_api_reader.h"
#include "json_access.h"

#include <algorithm>
#include <map>
#include <utility>

namespace ergo::package_model {

namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

/// 再帰の照合。 `*` は '/' を越えない、 `**` は越える。
bool match_from(const std::string& g, std::size_t gi, const std::string& p, std::size_t pi) {
    while (gi < g.size()) {
        if (g[gi] == '*') {
            const bool deep = gi + 1 < g.size() && g[gi + 1] == '*';
            const std::size_t next = gi + (deep ? 2 : 1);
            for (std::size_t k = pi; k <= p.size(); ++k) {
                if (match_from(g, next, p, k)) return true;
                if (k < p.size() && !deep && p[k] == '/') break;
            }
            return false;
        }
        if (pi >= p.size() || g[gi] != p[pi]) return false;
        ++gi;
        ++pi;
    }
    return pi == p.size();
}

} // namespace

bool read_layers_text(const std::string& json, std::vector<LayerRule>& rules, std::string* error) {
    detail::jsonm::JsonValue doc;
    if (!detail::jsonm::parse(json, doc) || !doc.is_object()) {
        return fail(error, "layers.json: malformed JSON");
    }
    const auto* layers = detail::get_array(doc, "layers");
    if (!layers) return fail(error, "layers.json: no \"layers\" array");
    rules.clear();
    for (const auto& entry : *layers) {
        if (!entry.is_object()) continue;
        LayerRule r{detail::get_string(entry, "glob"), detail::get_string(entry, "layer")};
        if (!r.glob.empty() && !r.layer.empty()) rules.push_back(std::move(r));
    }
    std::stable_sort(rules.begin(), rules.end(),
                     [](const LayerRule& a, const LayerRule& b) { return a.glob < b.glob; });
    return true;
}

bool read_layers_file(const std::string& path, std::vector<LayerRule>& rules, std::string* error) {
    std::string text;
    if (!detail::read_text_file(path, text)) return fail(error, "cannot read " + path);
    return read_layers_text(text, rules, error);
}

bool glob_match(const std::string& glob, const std::string& path) {
    return match_from(glob, 0, path, 0);
}

std::string layer_of(const std::vector<LayerRule>& rules, const std::string& path) {
    for (const auto& r : rules) {
        if (glob_match(r.glob, path)) return r.layer;
    }
    return {};
}

void assign_layers(PackageModel& model, const std::vector<LayerRule>& rules) {
    if (model.ergo_root.empty()) return;
    for (auto& t : model.targets) {
        if (t.package != "ergo") continue;
        std::map<std::string, std::size_t> votes;
        for (const auto& source : t.sources) {
            const std::string abs = normalize_path(source, model.source_root);
            if (!path_within(abs, model.ergo_root) || abs.size() <= model.ergo_root.size()) continue;
            const std::string rel = abs.substr(model.ergo_root.size() + 1);
            const std::string layer = layer_of(rules, rel);
            if (!layer.empty()) ++votes[layer];
        }
        std::string best;
        std::size_t best_votes = 0;
        for (const auto& [layer, count] : votes) {   // 名前順なので同数は先のもの
            if (count > best_votes) {
                best       = layer;
                best_votes = count;
            }
        }
        t.layer = best;
    }
}

} // namespace ergo::package_model
