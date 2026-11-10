#include "breviarium_client.h"

#include "concordia_client.h"

#include "ergo/studio/platform/http_client.h"

namespace ergo::studio::plugins::studio {

ServiceResult BreviariumClient::projects() const {
    auto r = caller_.call(ep_.url(Service::Breviarium), "/api/projects");
    if (r.ok) r.data = list_of(r.data, "projects");
    return r;
}

ServiceResult BreviariumClient::overview(const std::string& code) const {
    return caller_.call(ep_.url(Service::Breviarium), "/api/projects/" + platform::url_encode(code) + "/overview");
}

ServiceResult BreviariumClient::refresh(const std::string& code) const {
    return caller_.call(ep_.url(Service::Breviarium), "/api/projects/" + platform::url_encode(code) + "/refresh", "POST",
                        JsonValue::make_object(), {}, caller_.timeout_ms() * 4);
}

ServiceResult BreviariumClient::health() const { return caller_.call(ep_.url(Service::Breviarium), "/health"); }

} // namespace ergo::studio::plugins::studio
