#include "ergo/studio/web/request_pipeline.h"

#include "ergo/common/json_min.h"
#include "ergo/studio/ingress/remote_scope.h"

#include <exception>
#include <utility>

namespace ergo::studio::web {
namespace {

namespace jsonm = ergo::common::jsonm;

net::HttpResponse json_error(int status, const std::string& err) {
    jsonm::JsonValue out = jsonm::JsonValue::make_object();
    out.set("ok", jsonm::JsonValue::make_bool(false));
    out.set("err", jsonm::JsonValue::make_string(err));
    return net::HttpResponse::json(status, jsonm::serialize(out));
}

} // namespace

std::string ws_plugin_id(const std::string& path) {
    if (path.size() < 5 || path[0] != '/') return {};
    const auto slash = path.find('/', 1);
    if (slash == std::string::npos || slash == 1) return {};
    const std::string rest = path.substr(slash);
    if (rest != "/ws" && rest != "/ws/") return {};
    return path.substr(1, slash - 1);
}

RequestPipeline::RequestPipeline(ingress::EntrancePolicy policy, RouteTable routes, net::IHttpHandler& http,
                                 net::IWsAcceptor& ws, AccessLogFn access_log)
    : policy_(std::move(policy)), routes_(std::move(routes)), http_(http), ws_(ws), access_log_(std::move(access_log)) {}

net::HttpResponse RequestPipeline::refuse(const net::HttpRequest& req, int status, const std::string& why) const {
    if (access_log_) access_log_(req, status, why);
    return json_error(status, status == 404 ? std::string("not found") : why);
}

net::HttpResponse RequestPipeline::answer(net::HttpRequest& req) const {
    const ingress::Entrance entrance = ingress::check_entrance(policy_, req.gate, req, false);
    if (entrance.problem) return refuse(req, 403, *entrance.problem);
    req.who = entrance.who;
    if (!routes_.reaches(req)) return refuse(req, 404, "not on the route table");
    if (req.gate == net::Gate::remote) {
        if (const auto why = ingress::remote_refusal(req.method, req.path)) return refuse(req, 403, *why);
    }
    net::HttpResponse res;
    std::string       note;
    try {
        res = http_.handle(req);
    } catch (const std::exception& e) {
        note = std::string("handler threw: ") + e.what();
        res  = json_error(500, "internal error");
    }
    if (access_log_) access_log_(req, res.status, note);
    return res;
}

std::optional<WsRefusal> RequestPipeline::admit_ws(net::HttpRequest& req, std::string& plugin_id) const {
    const ingress::Entrance entrance = ingress::check_entrance(policy_, req.gate, req, true);
    std::optional<WsRefusal> refusal;
    if (entrance.problem) {
        refusal = WsRefusal{403, *entrance.problem};
    } else {
        req.who   = entrance.who;
        plugin_id = ws_plugin_id(req.path);
        if (plugin_id.empty() || !ws_.accepts(plugin_id)) refusal = WsRefusal{404, "no WebSocket at this path"};
    }
    if (access_log_) access_log_(req, refusal ? refusal->status : 101, refusal ? refusal->reason : std::string());
    return refusal;
}

} // namespace ergo::studio::web
