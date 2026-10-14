#include "provenance_endpoints.h"

#include "ergo/package_watch/loopback_url.h"

#include <cstdlib>

namespace ergo::package_cli {

namespace {

std::string pick(const std::string& arg, const char* env_name) {
    std::string v = arg;
    if (v.empty()) {
        const char* e = std::getenv(env_name);
        if (e) v = e;
    }
    while (!v.empty() && v.back() == '/') v.pop_back();
    return v;
}

bool check(const std::string& url, const char* what, std::string* error) {
    if (url.empty() || ergo::package_watch::is_loopback_url(url)) return true;
    // URL そのものは出さない (ログに残しても困らない形にする)。
    if (error) *error = std::string(what) + " must be a loopback http(s) URL (localhost / 127.0.0.1 / [::1])";
    return false;
}

} // namespace

bool resolve_provenance_endpoints(const Args& args, ProvenanceEndpoints& out, std::string* error) {
    out.revisor   = pick(args.revisor_url, "ERGO_WATCH_REVISOR_URL");
    out.concordia = pick(args.concordia_url, "ERGO_WATCH_CONCORDIA_URL");
    return check(out.revisor, "--revisor-url / ERGO_WATCH_REVISOR_URL", error) &&
           check(out.concordia, "--concordia-url / ERGO_WATCH_CONCORDIA_URL", error);
}

} // namespace ergo::package_cli
