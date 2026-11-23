#include "plugin_api.h"

#include <cstdio>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

std::string ApiResult::error_text() const {
    for (const char* k : {"error", "err", "message"}) {
        if (const JsonValue* v = json.find(k); v && v->is_string() && !v->s.empty()) return v->s;
    }
    if (status == 0) return body.empty() ? "no response" : body;
    return "HTTP " + std::to_string(status);
}

std::string url_encode(const std::string& v) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : v) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

net::HttpRequest make_request(const std::string& plugin_id, const std::string& method, const std::string& sub_with_query,
                              const std::string& body) {
    net::HttpRequest r;
    r.method = method;
    const auto q = sub_with_query.find('?');
    const std::string sub = sub_with_query.substr(0, q);
    r.path   = "/" + plugin_id + sub;
    r.target = "/" + plugin_id + sub_with_query;
    if (q != std::string::npos) {
        const std::string qs = sub_with_query.substr(q + 1);
        std::size_t start = 0;
        while (start <= qs.size()) {
            const auto amp  = qs.find('&', start);
            const std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            if (!pair.empty()) {
                const auto eq = pair.find('=');
                const auto k  = net::percent_decode(pair.substr(0, eq), true);
                const auto v  = net::percent_decode(eq == std::string::npos ? std::string() : pair.substr(eq + 1), true);
                if (k && v) r.query[*k] = *v;
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
    r.headers["host"] = "127.0.0.1";
    if (!body.empty() || method != "GET") r.headers["content-type"] = "application/json";
    r.body = body;
    return r;
}

PluginApi::PluginApi(host::IPlugin& plugin) : plugin_(plugin), worker_([this] { run(); }) {}

PluginApi::~PluginApi() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
        jobs_.clear();
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void PluginApi::call(const std::string& method, const std::string& sub_with_query, const std::string& body, Done done) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        jobs_.push_back({method, sub_with_query, body, std::move(done)});
    }
    cv_.notify_one();
}

ApiResult PluginApi::call_now(const std::string& method, const std::string& sub_with_query, const std::string& body) {
    ApiResult out;
    host::IPluginHttp* http = plugin_.http();
    if (!http) {
        out.body = "this plugin has no HTTP API";
        return out;
    }
    const net::HttpRequest req = make_request(plugin_.info().id, method, sub_with_query, body);
    try {
        const net::HttpResponse res = http->handle(req, req.path.substr(plugin_.info().id.size() + 1));
        out.status = res.status;
        out.body   = res.body;
        if (!jm::parse(res.body, out.json)) out.json = JsonValue::make_null();
    } catch (const std::exception& e) {
        out.status = 0;
        out.body   = e.what();
    }
    return out;
}

void PluginApi::run() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        ApiResult r = call_now(job.method, job.path, job.body);
        if (job.done) job.done(std::move(r));
    }
}

} // namespace ergo::studio::views
