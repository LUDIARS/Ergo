#include "terpsichore_config.h"

#include "../common/file_io.h"

#include <cmath>
#include <system_error>

namespace ergo::studio::plugins::musa_terpsichore {

const char* role_name(ServerRole r) {
    switch (r) {
        case ServerRole::Watcher: return "watcher";
        case ServerRole::Debugger: return "debugger";
        default: return "worker";
    }
}

std::optional<ServerRole> parse_role(const std::string& s) {
    if (s == "worker") return ServerRole::Worker;
    if (s == "watcher") return ServerRole::Watcher;
    if (s == "debugger") return ServerRole::Debugger;
    return std::nullopt;
}

std::optional<TerpsichoreConfig> parse_config(const JsonValue& v, std::string& err) {
    if (!v.is_object()) {
        err = "config must be a JSON object";
        return std::nullopt;
    }
    TerpsichoreConfig c;
    const JsonValue* port = v.find("port");
    if (!port || !port->is_number() || port->n < 1 || port->n > 65535 || std::floor(port->n) != port->n) {
        err = "port must be an integer 1..65535";
        return std::nullopt;
    }
    c.port = static_cast<uint16_t>(port->n);
    if (const JsonValue* e = v.find("enabled")) {
        if (!e->is_bool()) { err = "enabled must be a boolean"; return std::nullopt; }
        c.enabled = e->b;
    }
    if (const JsonValue* t = v.find("token")) {
        if (!t->is_string()) { err = "token must be a string"; return std::nullopt; }
        c.token = t->s;
    }
    if (const JsonValue* r = v.find("role")) {
        const auto role = r->is_string() ? parse_role(r->s) : std::nullopt;
        if (!role) { err = "role must be worker / watcher / debugger"; return std::nullopt; }
        c.role = *role;
    }
    if (const JsonValue* h = v.find("host")) {
        if (!h->is_string() || h->s.empty()) { err = "host must be a non-empty string"; return std::nullopt; }
        c.host = h->s;
    }
    return c;
}

JsonValue to_json_value(const TerpsichoreConfig& c) {
    return jobj({{"port", jnum(c.port)}, {"enabled", jbool(c.enabled)}, {"token", jstr(c.token)},
                 {"role", jstr(role_name(c.role))}, {"host", jstr(c.host)}});
}

std::filesystem::path config_path(const std::string& home) {
    if (home.empty()) return {};
    return std::filesystem::u8path(home) / ".ars" / "terpsichore" / "config.json";
}

LoadedConfig load_config(const std::filesystem::path& file) {
    LoadedConfig out;
    std::error_code ec;
    if (file.empty() || !std::filesystem::exists(file, ec)) return out;
    std::string err;
    const auto text = read_text_file(file, err);
    if (!text) {
        out.error = err;
        return out;
    }
    JsonValue v;
    if (!ergo::common::jsonm::parse(*text, v)) {
        out.error = file.u8string() + ": not valid JSON";
        return out;
    }
    out.config = parse_config(v, err);
    if (!out.config) out.error = file.u8string() + ": " + err;
    return out;
}

bool save_config(const std::filesystem::path& file, const TerpsichoreConfig& c, std::string& err) {
    if (file.empty()) {
        err = "config path is not known (no home directory)";
        return false;
    }
    return write_text_file(file, to_json(to_json_value(c)), err);
}

} // namespace ergo::studio::plugins::musa_terpsichore
