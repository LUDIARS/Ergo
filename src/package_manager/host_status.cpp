#include "ergo/package_manager/host_status.h"

namespace ergo::package_manager {

namespace {

jsonm::JsonValue count(long long v) {
    return jsonm::JsonValue::make_number(static_cast<double>(v));
}

} // namespace

jsonm::JsonValue host_status_to_json(const HostStatusInput& in) {
    auto tests = jsonm::JsonValue::make_object();
    const bool known = in.tests.source == "junit" || in.tests.source == "last-test-log";
    tests.set("source", jsonm::JsonValue::make_string(known ? in.tests.source : "unknown"));
    if (known) {
        tests.set("total", count(in.tests.total));
        tests.set("passed", count(in.tests.passed));
        tests.set("failed", count(in.tests.failed));
        tests.set("skipped", count(in.tests.skipped));
    }
    auto doc = jsonm::JsonValue::make_object();
    doc.set("schema", count(1));
    doc.set("host", jsonm::JsonValue::make_string(in.host));
    doc.set("head", jsonm::JsonValue::make_string(in.head));
    doc.set("configured_at", jsonm::JsonValue::make_string(in.configured_at));
    doc.set("tests", std::move(tests));
    return doc;
}

std::string host_status_text(const HostStatusInput& in) {
    return jsonm::serialize(host_status_to_json(in)) + "\n";
}

} // namespace ergo::package_manager
