#include "ergo/package_watch/watch_report.h"

#include "ergo/package_watch/time_text.h"
#include "json_util.h"

#include <algorithm>

namespace ergo::package_watch {

using detail::opt_int;
using detail::str;

namespace {

constexpr int kSchema = 1;

jsonm::JsonValue number(int v) { return jsonm::JsonValue::make_number(v); }

/// 依存の名札 (報告・出来事で共通)。 取得元の URL は載せない。
void set_identity(jsonm::JsonValue& o, const WatchDependency& d) {
    o.set("code", str(d.code));
    o.set("owner", str(d.owner));
    o.set("private", detail::boolean(d.is_private));
}

jsonm::JsonValue pending_json(const std::string& host, const StateEntry& e) {
    auto o = candidate_to_json(e.update);
    o.set("id", str(event_id(host, e.update.dependency, e.update.kind, e.update.subject)));
    o.set("first_seen", str(format_utc(e.first_seen)));
    o.set("last_notified", str(format_utc(e.last_notified)));
    return o;
}

jsonm::JsonValue dependencies_json(const std::vector<DependencyResult>& results) {
    std::vector<const DependencyResult*> ordered;
    for (const auto& r : results) ordered.push_back(&r);
    std::stable_sort(ordered.begin(), ordered.end(), [](const DependencyResult* a, const DependencyResult* b) {
        return a->dependency.name < b->dependency.name;
    });
    auto arr = jsonm::JsonValue::make_array();
    for (const auto* r : ordered) arr.push(dependency_json(*r));
    return arr;
}

jsonm::JsonValue sources_json(const WatchSources& s) {
    auto o = jsonm::JsonValue::make_object();
    o.set("concordia", str(s.concordia));
    o.set("revisor", str(s.revisor));
    return o;
}

std::string short_sha(const std::string& sha) { return sha.substr(0, 7); }

std::string version_text(const std::string& tag, const std::string& rev) {
    if (rev.empty()) return "-";
    return tag.empty() ? short_sha(rev) : tag + " " + short_sha(rev);
}

std::string pad(const std::string& s, std::size_t width) {
    return s.size() >= width ? s + " " : s + std::string(width - s.size(), ' ');
}

void collect_leaks(const jsonm::JsonValue& doc, const std::string& prefix, WatchOutputs& out) {
    for (auto& leak : ergo::package_manager::find_leaks(doc)) {
        leak.where = prefix + ":" + leak.where;
        out.leaks.push_back(std::move(leak));
    }
}

} // namespace

jsonm::JsonValue dependency_json(const DependencyResult& result) {
    const auto& d   = result.dependency;
    const auto& obs = result.observation;
    auto o = jsonm::JsonValue::make_object();
    o.set("name", str(d.name));
    set_identity(o, d);
    o.set("observed", detail::boolean(obs.observed));
    o.set("reason", str(obs.reason));
    o.set("as_of", str(obs.as_of));
    auto pin = jsonm::JsonValue::make_object();
    pin.set("rev", str(d.rev));
    pin.set("tag", str(d.expect_tag));
    o.set("pin", std::move(pin));
    auto track = jsonm::JsonValue::make_object();
    track.set("branch", str(d.track_branch));
    track.set("head", str(obs.track_head));
    track.set("behind", opt_int(obs.behind));
    track.set("on_track", obs.pin_on_track ? detail::boolean(*obs.pin_on_track) : jsonm::JsonValue::make_null());
    o.set("track", std::move(track));
    o.set("sibling_behind", opt_int(obs.sibling_behind));
    auto updates = jsonm::JsonValue::make_array();
    for (const auto& c : result.updates) updates.push(candidate_to_json(c));
    o.set("updates", std::move(updates));
    return o;
}

jsonm::JsonValue event_json(const WatchEvent& event) {
    auto o = candidate_to_json(event.update);
    o.set("id", str(event.id));
    o.set("host", str(event.host));
    set_identity(o, event.dependency);
    o.set("as_of", str(event.as_of));
    o.set("at", str(format_utc(event.at)));
    o.set("since_notified", opt_int(event.since_notified));
    return o;
}

jsonm::JsonValue host_report_json(const HostWatch& watch, std::int64_t now) {
    auto doc = jsonm::JsonValue::make_object();
    doc.set("schema", number(kSchema));
    doc.set("host", str(watch.host));
    doc.set("generated_at", str(format_utc(now)));
    doc.set("dependencies", dependencies_json(watch.results));
    auto events = jsonm::JsonValue::make_array();
    for (const auto& e : watch.diff.events) events.push(event_json(e));
    doc.set("events", std::move(events));
    auto pending = jsonm::JsonValue::make_array();
    for (const auto& e : watch.diff.next.entries) pending.push(pending_json(watch.host, e));
    doc.set("pending", std::move(pending));
    doc.set("sources", sources_json(watch.sources));
    return doc;
}

jsonm::JsonValue events_json(const std::vector<HostWatch>& watches, std::int64_t now,
                             const WatchSources& sources) {
    std::vector<const HostWatch*> ordered;
    for (const auto& w : watches) ordered.push_back(&w);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const HostWatch* a, const HostWatch* b) { return a->host < b->host; });
    auto doc = jsonm::JsonValue::make_object();
    doc.set("schema", number(kSchema));
    doc.set("generated_at", str(format_utc(now)));
    auto hosts  = jsonm::JsonValue::make_array();
    auto events = jsonm::JsonValue::make_array();
    for (const auto* w : ordered) {
        hosts.push(str(w->host));
        for (const auto& e : w->diff.events) events.push(event_json(e));
    }
    doc.set("hosts", std::move(hosts));
    doc.set("events", std::move(events));
    doc.set("sources", sources_json(sources));
    return doc;
}

jsonm::JsonValue outdated_json(const std::string& host, const std::vector<DependencyResult>& results,
                               std::int64_t now) {
    auto doc = jsonm::JsonValue::make_object();
    doc.set("schema", number(kSchema));
    doc.set("host", str(host));
    doc.set("generated_at", str(format_utc(now)));
    doc.set("dependencies", dependencies_json(results));
    return doc;
}

std::string render_outdated_text(const std::string& host, const std::vector<DependencyResult>& results) {
    std::string out = "host: " + (host.empty() ? std::string("-") : host) + "\n";
    out += pad("name", 16) + pad("kind", 10) + pad("from", 18) + pad("to", 18) + pad("behind", 8) + "note\n";
    for (const auto& r : results) {
        const auto& d = r.dependency;
        if (!r.observation.observed) {
            out += pad(d.name, 16) + pad("-", 10) + pad(version_text(d.expect_tag, d.rev), 18) + pad("-", 18) +
                   pad("-", 8) + "unobserved: " + r.observation.reason + "\n";
            continue;
        }
        if (r.updates.empty()) {
            out += pad(d.name, 16) + pad("-", 10) + pad(version_text(d.expect_tag, d.rev), 18) + pad("-", 18) +
                   pad("-", 8) + "up to date (" + r.observation.as_of + ")\n";
            continue;
        }
        for (const auto& c : r.updates) {
            std::string note = c.strict_ok ? "STRICT ok" : "not STRICT (no tag)";
            if (c.warning) note = "warning: pin is not on " + d.track_branch;
            out += pad(d.name, 16) + pad(update_kind_name(c.kind), 10) +
                   pad(version_text(c.from_tag, c.from_rev), 18) + pad(version_text(c.to_tag, c.to_rev), 18) +
                   pad(c.behind ? std::to_string(*c.behind) : std::string("-"), 8) + note + "\n";
        }
    }
    return out;
}

WatchOutputs prepare_watch_outputs(const std::vector<HostWatch>& watches, std::int64_t now,
                                   const WatchSources& sources) {
    WatchOutputs out;
    for (const auto& w : watches) {
        const auto doc = host_report_json(w, now);
        collect_leaks(doc, w.host, out);
        out.reports.push_back({w.host, jsonm::serialize(doc) + "\n"});
    }
    const auto events = events_json(watches, now, sources);
    collect_leaks(events, "events", out);
    out.events = {"", jsonm::serialize(events) + "\n"};
    out.ok     = out.leaks.empty();
    if (!out.ok) {
        out.reports.clear();
        out.events = {};
    }
    return out;
}

} // namespace ergo::package_watch
