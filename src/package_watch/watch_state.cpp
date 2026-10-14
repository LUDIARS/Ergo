#include "ergo/package_watch/watch_state.h"

#include "ergo/package_watch/time_text.h"
#include "json_util.h"

#include <algorithm>
#include <tuple>

namespace ergo::package_watch {

using detail::get_bool;
using detail::get_opt_int;
using detail::get_string;
using detail::opt_int;
using detail::str;

void sort_entries(std::vector<StateEntry>& entries) {
    std::sort(entries.begin(), entries.end(), [](const StateEntry& a, const StateEntry& b) {
        return std::make_tuple(a.update.dependency, static_cast<int>(a.update.kind), a.update.subject) <
               std::make_tuple(b.update.dependency, static_cast<int>(b.update.kind), b.update.subject);
    });
}

jsonm::JsonValue candidate_to_json(const Candidate& c) {
    auto o = jsonm::JsonValue::make_object();
    o.set("dependency", str(c.dependency));
    o.set("kind", str(update_kind_name(c.kind)));
    o.set("subject", str(c.subject));
    o.set("from_rev", str(c.from_rev));
    o.set("from_tag", str(c.from_tag));
    o.set("to_rev", str(c.to_rev));
    o.set("to_tag", str(c.to_tag));
    o.set("behind", opt_int(c.behind));
    o.set("warning", detail::boolean(c.warning));
    o.set("strict_ok", detail::boolean(c.strict_ok));
    o.set("next", str(c.next));
    if (is_provenance_kind(c.kind)) {  // W3 の種類の形 (バイト列) は変えない
        o.set("origin", str(c.origin));
        o.set("trigger", str(c.trigger));
        o.set("review", opt_int(c.review));
        o.set("head", str(c.head));
    }
    return o;
}

bool read_candidate(const jsonm::JsonValue& doc, Candidate& out) {
    if (!doc.is_object()) return false;
    if (!parse_update_kind(get_string(doc, "kind"), out.kind)) return false;
    out.dependency = get_string(doc, "dependency");
    out.subject    = get_string(doc, "subject");
    out.from_rev   = get_string(doc, "from_rev");
    out.from_tag   = get_string(doc, "from_tag");
    out.to_rev     = get_string(doc, "to_rev");
    out.to_tag     = get_string(doc, "to_tag");
    out.behind     = get_opt_int(doc, "behind");
    out.warning    = get_bool(doc, "warning");
    out.strict_ok  = get_bool(doc, "strict_ok");
    out.next       = get_string(doc, "next");
    out.origin     = get_string(doc, "origin");
    out.trigger    = get_string(doc, "trigger");
    out.review     = get_opt_int(doc, "review");
    out.head       = get_string(doc, "head");
    return !out.dependency.empty() && !out.subject.empty();
}

jsonm::JsonValue state_to_json(const WatchState& state) {
    auto doc = jsonm::JsonValue::make_object();
    doc.set("schema", jsonm::JsonValue::make_number(1));
    doc.set("host", str(state.host));
    auto fps = jsonm::JsonValue::make_object();
    for (const auto& [name, fp] : state.fingerprints) fps.set(name, str(fp));
    doc.set("fingerprints", std::move(fps));
    std::vector<StateEntry> entries = state.entries;
    sort_entries(entries);
    auto arr = jsonm::JsonValue::make_array();
    for (const auto& e : entries) {
        auto o = candidate_to_json(e.update);
        o.set("first_seen", str(format_utc(e.first_seen)));
        o.set("last_notified", str(format_utc(e.last_notified)));
        o.set("notified_behind", opt_int(e.notified_behind));
        arr.push(std::move(o));
    }
    doc.set("entries", std::move(arr));
    return doc;
}

bool read_state(const jsonm::JsonValue& doc, WatchState& out, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (!doc.is_object()) return fail("state is not a JSON object");
    const auto* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->n != 1) return fail("state schema is not 1");
    out.host = get_string(doc, "host");
    out.fingerprints.clear();
    out.entries.clear();
    if (const auto* fps = doc.find("fingerprints"); fps && fps->is_object() && fps->o) {
        for (const auto& [name, v] : *fps->o) {
            if (v.is_string()) out.fingerprints[name] = v.s;
        }
    }
    const auto* entries = doc.find("entries");
    if (entries && !entries->is_array()) return fail("state entries is not an array");
    if (entries && entries->a) {
        for (const auto& item : *entries->a) {
            StateEntry e;
            if (!read_candidate(item, e.update)) return fail("state entry without kind / dependency / subject");
            if (!parse_utc(get_string(item, "first_seen"), e.first_seen) ||
                !parse_utc(get_string(item, "last_notified"), e.last_notified)) {
                return fail("state entry time is not YYYY-MM-DDTHH:MM:SSZ");
            }
            e.notified_behind = get_opt_int(item, "notified_behind");
            out.entries.push_back(std::move(e));
        }
    }
    sort_entries(out.entries);
    return true;
}

} // namespace ergo::package_watch
