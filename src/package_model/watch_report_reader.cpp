// ergo_package_model: `<host>/.ergo/watch-report.json` (spec/data/ergo-watch-report.md) の読み手。
// 図が使うキーだけを取り出す。 書き手 (ergo_package_watch) は import しない。

#include "ergo/package_model/host_state_reader.h"

#include "json_access.h"

#include <algorithm>
#include <utility>

namespace ergo::package_model {

namespace {

using detail::get_bool;
using detail::get_string;
using detail::jsonm::JsonValue;

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::optional<long long> get_count(const JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    if (!v || !v->is_number()) return std::nullopt;
    return static_cast<long long>(v->n);
}

WatchUpdate read_update(const JsonValue& v) {
    WatchUpdate u;
    u.dependency = get_string(v, "dependency");
    u.kind       = get_string(v, "kind");
    u.subject    = get_string(v, "subject");
    u.to_tag     = get_string(v, "to_tag");
    u.behind     = get_count(v, "behind");
    u.origin     = get_string(v, "origin");
    u.review     = get_count(v, "review");
    u.head       = get_string(v, "head");
    return u;
}

WatchDependency read_dependency(const JsonValue& v) {
    WatchDependency d;
    d.name       = get_string(v, "name");
    d.code       = get_string(v, "code");
    d.owner      = get_string(v, "owner");
    d.is_private = get_bool(v, "private");
    d.observed   = get_bool(v, "observed");
    d.reason     = get_string(v, "reason");
    if (const auto* pin = detail::get_object(v, "pin")) {
        d.pin_rev = get_string(*pin, "rev");
        d.pin_tag = get_string(*pin, "tag");
    }
    if (const auto* track = detail::get_object(v, "track")) {
        d.track_branch = get_string(*track, "branch");
        d.track_behind = get_count(*track, "behind");
        if (const auto* on = track->find("on_track"); on && on->is_bool()) d.on_track = on->b;
    }
    if (const auto* updates = detail::get_array(v, "updates")) {
        for (const auto& u : *updates) {
            if (u.is_object()) d.updates.push_back(read_update(u));
        }
    }
    return d;
}

} // namespace

const WatchDependency* WatchReport::find(const std::string& name) const {
    for (const auto& d : dependencies) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

bool read_watch_report_text(const std::string& json, WatchReport& out, std::string* error) {
    JsonValue doc;
    if (!detail::jsonm::parse(json, doc) || !doc.is_object()) {
        return fail(error, "watch-report.json: malformed JSON");
    }
    const auto* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->n != 1.0) {
        return fail(error, "watch-report.json: unsupported schema (expected 1)");
    }
    WatchReport r;
    r.host         = get_string(doc, "host");
    r.generated_at = get_string(doc, "generated_at");
    if (const auto* deps = detail::get_array(doc, "dependencies")) {
        for (const auto& d : *deps) {
            if (d.is_object()) r.dependencies.push_back(read_dependency(d));
        }
    }
    if (const auto* sources = detail::get_object(doc, "sources")) {
        r.revisor   = get_string(*sources, "revisor");
        r.concordia = get_string(*sources, "concordia");
    }
    std::stable_sort(r.dependencies.begin(), r.dependencies.end(),
                     [](const WatchDependency& a, const WatchDependency& b) { return a.name < b.name; });
    out = std::move(r);
    return true;
}

} // namespace ergo::package_model
