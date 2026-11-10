#include "concordia_client.h"

#include "ergo/studio/platform/http_client.h"

namespace ergo::studio::plugins::studio {

JsonValue list_of(const JsonValue& data, const char* key) {
    if (data.is_array()) return data;
    const JsonValue* v = field(data, key);
    return v && v->is_array() ? *v : JsonValue::make_array();
}

JsonValue unwrap(const JsonValue& data, const char* key) {
    const JsonValue* v = field(data, key);
    return v && v->is_object() ? *v : data;
}

namespace {
ServiceResult with_data(ServiceResult r, JsonValue data) {
    if (r.ok) r.data = std::move(data);
    return r;
}
} // namespace

ServiceResult ConcordiaClient::projects() const {
    auto r = caller_.call(ep_.url(Service::Concordia), "/v1/project-codes/admin");
    return with_data(r, list_of(r.data, "entries"));
}

ServiceResult ConcordiaClient::register_project(const JsonValue& input) const {
    return caller_.call(ep_.url(Service::Concordia), "/v1/project-codes", "POST", input);
}

ServiceResult ConcordiaClient::templates() const {
    auto r = caller_.call(ep_.url(Service::Concordia), "/v1/delegation/templates");
    JsonValue active = JsonValue::make_array();
    const JsonValue all = list_of(r.data, "templates");
    for (const auto& t : *all.a) {
        const JsonValue* on = field(t, "is_active");
        const bool yes = on && ((on->is_bool() && on->b) || (on->is_number() && on->n != 0));
        if (yes) active.push(t);
    }
    return with_data(r, active);
}

ServiceResult ConcordiaClient::invoke(const JsonValue& req) const {
    return caller_.call(ep_.url(Service::Concordia), "/v1/delegation/invoke", "POST", req, {}, caller_.timeout_ms() * 4);
}

ServiceResult ConcordiaClient::runs(int limit) const {
    auto r = caller_.call(ep_.url(Service::Concordia), "/v1/delegation/runs?limit=" + std::to_string(limit));
    return with_data(r, list_of(r.data, "runs"));
}

ServiceResult ConcordiaClient::run(const std::string& id) const {
    auto r = caller_.call(ep_.url(Service::Concordia), "/v1/delegation/runs/" + platform::url_encode(id));
    return with_data(r, unwrap(r.data, "run"));
}

} // namespace ergo::studio::plugins::studio
