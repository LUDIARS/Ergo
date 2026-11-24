#include "studio_deps.h"

#include <algorithm>

namespace ergo::studio::plugins::studio {
namespace {

JsonValue strings(const std::vector<std::string>& items) {
    auto out = JsonValue::make_array();
    for (const auto& item : items) out.push(jstr(item));
    return out;
}

JsonValue display(const registry::Trust& result) {
    auto rows = JsonValue::make_array();
    for (const auto& entry : result.entries) {
        const bool trusted = result.verified() && entry.reason.empty();
        rows.push(jobj({{"id", jstr(entry.id)}, {"title", jstr(entry.title)}, {"kind", jstr(entry.kind)},
                        {"domains", strings(entry.domains)}, {"styles", strings(entry.styles)},
                        {"trusted", jbool(trusted)}, {"reason", jstr(result.verified() ? entry.reason : result.reason)}}));
    }
    return jobj({{"trusted", jbool(result.verified())}, {"reason", jstr(result.reason)}, {"plugins", rows}});
}

std::optional<PluginSource> trusted_source(const registry::Entry& entry) {
    const auto split = entry.source.find('/');
    if (split == std::string::npos) return std::nullopt;
    // The signed index authorizes this owner. Manual preview still uses ERGO_PLUGIN_OWNERS.
    return parse_plugin_source(entry.source, {entry.source.substr(0, split)}).source;
}

net::HttpResponse preview(StudioDeps& d, const Route& r, const registry::Trust& result) {
    if (!result.verified()) return send_error(409, result.reason);
    const auto id = str_field(r.body, "id");
    const auto item = std::find_if(result.entries.begin(), result.entries.end(), [&](const auto& entry) { return entry.id == id; });
    if (item == result.entries.end()) return send_error(404, "索引にこのプラグインはありません");
    if (!item->reason.empty()) return send_error(409, item->reason);
    const auto source = trusted_source(*item);
    if (!source) return send_error(409, "取得できるソースがありません");
    std::string err;
    const auto p = d.installer->preview(*source, err,
        registry::Provenance{item->id, item->publisher, result.fingerprint, now_iso_utc(), result.serial});
    return p ? json_response(200, jobj({{"preview", to_json_value(*p)}})) : send_error(502, err);
}

} // namespace

std::optional<net::HttpResponse> registry_routes(StudioDeps& d, const Route& r) {
    if (r.seg == std::vector<std::string>{"api", "plugins", "registry"} && r.is("GET"))
        return json_response(200, display(d.registry->refresh(false)));
    if (r.seg == std::vector<std::string>{"api", "plugins", "registry", "preview"} && r.is("POST"))
        return preview(d, r, d.registry->refresh());
    return std::nullopt;
}

bool recheck_registry_preview(StudioDeps& d, const registry::Provenance& trust, const PluginPreview& preview) {
    const auto result = d.registry->refresh();
    if (!result.permits(trust, preview.rev)) return false;
    return std::any_of(result.entries.begin(), result.entries.end(), [&](const auto& entry) {
        if (entry.id != trust.id || entry.publisher != trust.publisher || !entry.reason.empty()) return false;
        const auto source = trusted_source(entry);
        return source && pack_id(*source) == preview.id && source->rev == preview.rev;
    });
}

} // namespace ergo::studio::plugins::studio
