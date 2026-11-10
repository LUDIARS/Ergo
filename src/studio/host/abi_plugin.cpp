#include "abi_plugin.h"

#include <stdexcept>

namespace ergo::studio::host {
namespace {

ergo_studio_str_v1 to_abi(const std::string& s) { return {s.data(), s.size()}; }
std::string        copy(ergo_studio_str_v1 s) { return s.data ? std::string(s.data, s.len) : std::string(); }
std::string        cstr(const char* s) { return s ? std::string(s) : std::string(); }

/// Releases a plugin-filled response when it leaves scope.
class ResponseGuard {
public:
    ResponseGuard(const ergo_studio_plugin_v1* p, ergo_studio_http_response_v1& r) : p_(p), r_(r) {}
    ~ResponseGuard() {
        if (p_->release_response) p_->release_response(p_->self, &r_);
    }

private:
    const ergo_studio_plugin_v1*  p_;
    ergo_studio_http_response_v1& r_;
};

std::string table_problem(const ergo_studio_plugin_v1* p) {
    if (!p) return "create returned NULL";
    if (p->abi_version != ERGO_STUDIO_PLUGIN_ABI_VERSION) {
        return "ABI version " + std::to_string(p->abi_version) + " (host speaks "
             + std::to_string(ERGO_STUDIO_PLUGIN_ABI_VERSION) + ")";
    }
    if (!p->id || !p->title) return "id / title is NULL";
    if ((p->http || p->health) && !p->release_response) return "http / health without release_response";
    if (p->ws_open && !p->ws_text) return "ws_open without ws_text";
    return {};
}

} // namespace

AbiPlugin::AbiPlugin(Key, std::shared_ptr<DynamicLibrary> lib, LogFn log) : lib_(std::move(lib)), log_(std::move(log)) {
    host_.abi_version  = ERGO_STUDIO_PLUGIN_ABI_VERSION;
    host_.ctx          = this;
    host_.log          = &AbiPlugin::host_log;
    host_.ws_send_text = &AbiPlugin::host_ws_send;
    host_.ws_close     = &AbiPlugin::host_ws_close;
}

std::unique_ptr<IPlugin> AbiPlugin::create(std::shared_ptr<DynamicLibrary> lib, ergo_studio_plugin_v1_create_fn fn,
                                           LogFn log) {
    auto self = std::make_unique<AbiPlugin>(Key{}, std::move(lib), std::move(log));
    const ergo_studio_plugin_v1* p = fn(&self->host_);
    const std::string problem = table_problem(p);
    if (!problem.empty()) {
        // A table that exists but is unusable is still the plugin's to free.
        if (p && p->destroy && p->abi_version == ERGO_STUDIO_PLUGIN_ABI_VERSION) p->destroy(p->self);
        throw std::runtime_error(problem);
    }
    self->p_    = p;
    self->info_ = {cstr(p->id), cstr(p->title), cstr(p->description), cstr(p->icon)};
    return self;
}

AbiPlugin::~AbiPlugin() {
    if (p_ && p_->destroy) p_->destroy(p_->self);
}

net::HttpResponse AbiPlugin::handle(const net::HttpRequest& req, const std::string& sub_path) {
    const auto        q     = req.target.find('?');
    const std::string query = q == std::string::npos ? std::string() : req.target.substr(q + 1);
    const std::string ctype = req.header("content-type").value_or("");
    const ergo_studio_http_request_v1 in{to_abi(req.method), to_abi(sub_path), to_abi(query), to_abi(ctype), to_abi(req.body)};

    ergo_studio_http_response_v1 out{};
    const int rc = p_->http(p_->self, &in, &out);
    ResponseGuard guard(p_, out);
    if (rc != 0) return net::HttpResponse::json(500, "{\"ok\":false,\"err\":\"plugin http failed\"}");

    net::HttpResponse res;
    res.status = out.status;
    res.body   = copy(out.body);
    const std::string ct = copy(out.content_type);
    res.headers["content-type"] = ct.empty() ? "application/json; charset=utf-8" : ct;
    return res;
}

net::WsHandlers AbiPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    {
        std::lock_guard<std::mutex> lk(conns_mtx_);
        conns_[id] = conn;
    }
    if (p_->ws_open(p_->self, id) != 0) {
        forget_conn(id);
        conn->close(1008);
        return {};
    }
    net::WsHandlers h;
    h.on_text  = [this, id](const std::string& t) { p_->ws_text(p_->self, id, to_abi(t)); };
    h.on_close = [this, id] {
        forget_conn(id);
        if (p_->ws_closed) p_->ws_closed(p_->self, id);
    };
    return h;
}

JsonValue AbiPlugin::snapshot() const {
    ergo_studio_http_response_v1 out{};
    const int rc = p_->health(p_->self, &out);
    ResponseGuard guard(p_, out);
    if (rc != 0) throw std::runtime_error("plugin health failed");
    JsonValue v;
    // An unparsable body yields a value without "ok", which the report flags.
    if (!ergo::common::jsonm::parse(copy(out.body), v)) return JsonValue::make_object();
    return v;
}

std::shared_ptr<net::IWsConnection> AbiPlugin::find_conn(uint64_t id) {
    std::lock_guard<std::mutex> lk(conns_mtx_);
    const auto it = conns_.find(id);
    return it == conns_.end() ? nullptr : it->second.lock();
}

void AbiPlugin::forget_conn(uint64_t id) {
    std::lock_guard<std::mutex> lk(conns_mtx_);
    conns_.erase(id);
}

void AbiPlugin::host_log(void* ctx, const char* level, ergo_studio_str_v1 msg) {
    auto* self = static_cast<AbiPlugin*>(ctx);
    if (!self->log_) return;
    const std::string lv = cstr(level);
    self->log_(lv == "warn" || lv == "error" ? lv : "info", "[" + self->info_.id + "] " + copy(msg));
}

void AbiPlugin::host_ws_send(void* ctx, uint64_t conn, ergo_studio_str_v1 text) {
    if (auto c = static_cast<AbiPlugin*>(ctx)->find_conn(conn)) c->send_text(copy(text));
}

void AbiPlugin::host_ws_close(void* ctx, uint64_t conn, uint16_t code) {
    if (auto c = static_cast<AbiPlugin*>(ctx)->find_conn(conn)) c->close(code);
}

} // namespace ergo::studio::host
