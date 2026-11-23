#include "ergo/studio/project/launcher_routes.h"

#include "ergo/common/json_min.h"
#include "ergo/studio/net/request_guard.h"

#include <utility>

namespace ergo::studio::project {

namespace jsonm = ergo::common::jsonm;

namespace {

constexpr const char* kPrefix = "/api/launcher/";

jsonm::JsonValue str(const std::string& s) { return jsonm::JsonValue::make_string(s); }

net::HttpResponse fail(int status, const std::string& err) {
    jsonm::JsonValue out = jsonm::JsonValue::make_object();
    out.set("ok", jsonm::JsonValue::make_bool(false));
    out.set("err", str(err));
    return net::HttpResponse::json(status, jsonm::serialize(out));
}

net::HttpResponse ok() { return net::HttpResponse::json(200, "{\"ok\":true}"); }

} // namespace

LauncherRoutes::LauncherRoutes(Launcher& launcher, net::IHttpHandler& next, std::string bind_host)
    : launcher_(launcher), next_(next), bind_host_(std::move(bind_host)) {}

net::HttpResponse LauncherRoutes::handle(const net::HttpRequest& req) {
    if (req.path.rfind(kPrefix, 0) != 0) return next_.handle(req);
    const std::string what = req.path.substr(std::char_traits<char>::length(kPrefix));
    if (what == "projects") {
        if (req.method != "GET") return fail(405, "method not allowed");
        return list();
    }
    if (what != "open" && what != "close" && what != "add" && what != "remove") return fail(404, "not found");
    if (req.method != "POST") return fail(405, "method not allowed");
    return act(what, req);
}

net::HttpResponse LauncherRoutes::list() const {
    jsonm::JsonValue rows = jsonm::JsonValue::make_array();
    for (const LauncherRow& r : launcher_.rows()) {
        jsonm::JsonValue item = jsonm::JsonValue::make_object();
        item.set("source", str(r.source));
        item.set("name", str(r.name));
        item.set("root", str(r.root));
        item.set("style", str(r.style));
        item.set("domain", str(r.domain));
        item.set("last_opened", str(r.last_opened));
        item.set("recent", jsonm::JsonValue::make_bool(r.recent));
        item.set("missing", jsonm::JsonValue::make_bool(r.missing));
        item.set("open", jsonm::JsonValue::make_bool(r.open));
        rows.push(std::move(item));
    }
    jsonm::JsonValue out = jsonm::JsonValue::make_object();
    const auto current = launcher_.project().current();
    if (current) {
        jsonm::JsonValue open = jsonm::JsonValue::make_object();
        open.set("source", str(current->source));
        open.set("name", str(current->name));
        open.set("root", str(current->root));
        open.set("style", str(current->style));
        open.set("domain", str(current->domain));
        out.set("open", std::move(open));
    } else {
        out.set("open", jsonm::JsonValue::make_null());
    }
    out.set("projects", std::move(rows));
    out.set("notice", str(launcher_.notice()));
    return net::HttpResponse::json(200, jsonm::serialize(out));
}

net::HttpResponse LauncherRoutes::act(const std::string& what, const net::HttpRequest& req) {
    if ((what == "add" || what == "remove") && !net::is_trusted_host(req.header("host"), bind_host_)) {
        return fail(403, "the project list can only be changed from this machine");
    }
    if (what == "close") {
        launcher_.close();
        return ok();
    }
    jsonm::JsonValue body;
    if (!jsonm::parse(req.body, body) || !body.is_object()) return fail(400, "body must be a JSON object");
    const jsonm::JsonValue* source = body.find("source");
    if (source == nullptr || !source->is_string() || source->s.empty()) return fail(400, "source is required");
    std::string err;
    bool done = false;
    if (what == "open") done = launcher_.open(source->s, err);
    else if (what == "add") done = launcher_.add(source->s, err);
    else done = launcher_.remove(source->s, err);
    return done ? ok() : fail(what == "remove" ? 404 : 409, err);
}

} // namespace ergo::studio::project
