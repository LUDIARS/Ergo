// ergo_package_model: `<build>/ergo-packages.status.json` (spec/data/ergo-packages-status.md) の読み手。
// 書き手 (ergo_package_manager の host_status) は import しない。

#include "ergo/package_model/host_state_reader.h"

#include "json_access.h"

#include <utility>

namespace ergo::package_model {

namespace {

using detail::get_string;
using detail::jsonm::JsonValue;

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

long long get_count(const JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    return (v && v->is_number()) ? static_cast<long long>(v->n) : 0;
}

} // namespace

bool read_host_status_text(const std::string& json, HostStatus& out, std::string* error) {
    JsonValue doc;
    if (!detail::jsonm::parse(json, doc) || !doc.is_object()) {
        return fail(error, "ergo-packages.status.json: malformed JSON");
    }
    const auto* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->n != 1.0) {
        return fail(error, "ergo-packages.status.json: unsupported schema (expected 1)");
    }
    HostStatus s;
    s.host          = get_string(doc, "host");
    s.head          = get_string(doc, "head");
    s.configured_at = get_string(doc, "configured_at");
    if (const auto* tests = detail::get_object(doc, "tests")) {
        const std::string source = get_string(*tests, "source");
        if (source == "junit" || source == "last-test-log") {
            s.tests.source  = source;
            s.tests.total   = get_count(*tests, "total");
            s.tests.passed  = get_count(*tests, "passed");
            s.tests.failed  = get_count(*tests, "failed");
            s.tests.skipped = get_count(*tests, "skipped");
        }
    }
    out = std::move(s);
    return true;
}

} // namespace ergo::package_model
