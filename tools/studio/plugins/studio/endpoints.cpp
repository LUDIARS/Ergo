#include "endpoints.h"

#include "../common/file_io.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace ergo::studio::plugins::studio {
namespace {

std::string strip_slash(std::string u) {
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

} // namespace

const char* service_key(Service s) {
    switch (s) {
        case Service::Concordia: return "concordia";
        case Service::Actio: return "actio";
        case Service::Breviarium: return "breviarium";
        case Service::Curare: return "curare";
        case Service::Revisor: return "revisor";
        case Service::Excubitor: return "excubitor";
        case Service::Cernere: return "cernere";
    }
    return "";
}

const char* topology_env_name(Service s) {
    switch (s) {
        case Service::Concordia: return "CONCORDIA_URL";
        case Service::Actio: return "ACTIO_URL";
        case Service::Breviarium: return "BREVIARIUM_URL";
        case Service::Curare: return "CURARE_URL";
        case Service::Revisor: return "REVISOR_URL";
        case Service::Excubitor: return "EXCUBITOR_URL";
        case Service::Cernere: return "CERNERE_URL";
    }
    return "";
}

const std::vector<Service>& all_services() {
    static const std::vector<Service> v{Service::Concordia, Service::Actio,     Service::Breviarium, Service::Curare,
                                        Service::Revisor,   Service::Excubitor, Service::Cernere};
    return v;
}

std::optional<int> excubitor_port_from_catalog(const std::string& yaml) {
    std::string current;
    std::size_t start = 0;
    while (start <= yaml.size()) {
        const auto end = yaml.find('\n', start);
        std::string line = yaml.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = end == std::string::npos ? yaml.size() + 1 : end + 1;

        // "<spaces>-<spaces>code: <value>"
        const std::string t = trim(line);
        const std::string after_dash = t.size() > 1 && t[0] == '-' && (t[1] == ' ' || t[1] == '\t') ? trim(t.substr(1)) : "";
        if (after_dash.rfind("code:", 0) == 0) {
            std::string v = trim(after_dash.substr(5));
            const auto hash = v.find_first_of(" \t#");
            current = hash == std::string::npos ? v : v.substr(0, hash);
            continue;
        }
        if (current != "excubitor") continue;
        // exactly four spaces of indentation, then "port: <digits>" and an optional comment
        if (line.size() > 9 && line.compare(0, 4, "    ") == 0 && line[4] != ' ' && line.compare(4, 5, "port:") == 0) {
            std::string rest = trim(line.substr(9));
            const auto hash = rest.find('#');
            if (hash != std::string::npos) rest = trim(rest.substr(0, hash));
            if (!rest.empty() && rest.find_first_not_of("0123456789") == std::string::npos) return std::stoi(rest);
        }
    }
    return std::nullopt;
}

std::optional<std::string> resolve_excubitor_url(const StudioConfig& cfg) {
    if (cfg.excubitor_url) return cfg.excubitor_url;
    if (!cfg.ludiars_root) return std::nullopt;
    const auto path = std::filesystem::u8path(*cfg.ludiars_root) / "Excubitor" / "excubitor.catalog.yaml";
    std::string err;
    const auto text = read_text_file(path, err);
    if (!text) return std::nullopt;
    const auto port = excubitor_port_from_catalog(*text);
    if (!port) return std::nullopt;
    return "http://127.0.0.1:" + std::to_string(*port);
}

EndpointSnapshot pick_endpoints(const std::map<std::string, std::string>& env, const StudioConfig& cfg,
                                const std::optional<std::string>& excubitor) {
    EndpointSnapshot s;
    for (Service k : all_services()) {
        const auto it = env.find(topology_env_name(k));
        s.urls[k] = it == env.end() || it->second.empty() ? std::nullopt : std::optional<std::string>(strip_slash(it->second));
    }
    s.urls[Service::Excubitor] = excubitor;
    if (cfg.curare_url) s.urls[Service::Curare] = cfg.curare_url;
    for (const auto& [k, v] : env) {
        if (k.size() > 4 && k.compare(k.size() - 4, 4, "_URL") == 0) s.all[k.substr(0, k.size() - 4)] = v;
    }
    return s;
}

EndpointResolver::EndpointResolver(const StudioConfig& cfg, const ServiceCaller& caller, std::chrono::milliseconds ttl)
    : cfg_(cfg), caller_(caller), ttl_(ttl) {}

EndpointSnapshot EndpointResolver::snapshot(bool force) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!force && at_ && std::chrono::steady_clock::now() - *at_ < ttl_) return cache_;
    }
    const auto ex = resolve_excubitor_url(cfg_);
    EndpointSnapshot snap = pick_endpoints({}, cfg_, ex);
    if (ex) {
        const auto r = caller_.call(ex, "/api/v1/topology");
        const JsonValue* env = r.ok ? field(r.data, "env") : nullptr;
        if (env && env->is_object()) {
            std::map<std::string, std::string> m;
            for (const auto& [k, v] : *env->o) {
                if (v.is_string()) m[k] = v.s;
            }
            snap          = pick_endpoints(m, cfg_, ex);
            snap.topology = "ok";
        } else {
            snap.topology = "unavailable";
        }
    }
    snap.resolved_at = now_iso_utc();
    std::lock_guard<std::mutex> lk(mtx_);
    cache_ = snap;
    at_    = std::chrono::steady_clock::now();
    return snap;
}

std::optional<std::string> EndpointResolver::url(Service s) { return snapshot().urls[s]; }

std::string now_iso_utc() {
    const auto now  = std::chrono::system_clock::now();
    const auto secs = std::chrono::system_clock::to_time_t(now);
    const auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms));
    return buf;
}

} // namespace ergo::studio::plugins::studio
