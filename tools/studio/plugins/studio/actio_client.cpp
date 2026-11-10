#include "actio_client.h"

#include "concordia_client.h"

#include "ergo/studio/platform/http_client.h"

namespace ergo::studio::plugins::studio {
namespace {
ServiceResult with_data(ServiceResult r, JsonValue data) {
    if (r.ok) r.data = std::move(data);
    return r;
}
} // namespace

std::map<std::string, std::string> ActioClient::headers() const {
    if (!token_) return {};
    return {{"Authorization", "Bearer " + *token_}};
}

ServiceResult ActioClient::tasks(const std::string& project, bool include_done) const {
    std::string q = "scope=all&include=description&limit=200";
    if (!project.empty()) q += "&project=" + platform::url_encode(project);
    if (!include_done) q += "&status=" + platform::url_encode("open,in_progress,blocked");
    auto r = caller_.call(ep_.url(Service::Actio), "/api/tasks?" + q, "GET", JsonValue::make_null(), headers());
    return with_data(r, list_of(r.data, "tasks"));
}

ServiceResult ActioClient::task(const std::string& id) const {
    auto r = caller_.call(ep_.url(Service::Actio), "/api/tasks/" + platform::url_encode(id), "GET",
                          JsonValue::make_null(), headers());
    return with_data(r, unwrap(r.data, "task"));
}

ServiceResult ActioClient::create(const JsonValue& input) const {
    auto r = caller_.call(ep_.url(Service::Actio), "/api/tasks", "POST", input, headers());
    return with_data(r, unwrap(r.data, "task"));
}

ServiceResult ActioClient::update(const std::string& id, const JsonValue& input) const {
    auto r = caller_.call(ep_.url(Service::Actio), "/api/tasks/" + platform::url_encode(id), "PATCH", input, headers());
    return with_data(r, unwrap(r.data, "task"));
}

ServiceResult ActioClient::me() const {
    return caller_.call(ep_.url(Service::Actio), "/api/auth/me", "GET", JsonValue::make_null(), headers());
}

} // namespace ergo::studio::plugins::studio
