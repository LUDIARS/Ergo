#include "melpomene_config.h"

#include "../common/file_io.h"

#include <cmath>
#include <system_error>

namespace ergo::studio::plugins::musa_melpomene {

const char* const kKeyringService = "ars-melpomene";
const char* const kKeyringAccount = "github_token";

std::string MelpomeneConfig::api_base_url() const {
    return "https://api.github.com/repos/" + repository_owner + "/" + repository_name;
}

bool is_valid_repo_part(const std::string& s) {
    if (s.empty() || s == "." || s == "..") return false;
    for (char c : s) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

JsonValue to_json_value(const MelpomeneConfig& c, bool include_has_token) {
    JsonValue labels = JsonValue::make_array();
    for (const auto& l : c.default_labels) labels.push(jstr(l));
    JsonValue o = jobj({{"repository_owner", jstr(c.repository_owner)}, {"repository_name", jstr(c.repository_name)},
                        {"default_labels", labels}, {"default_priority", jstr(c.default_priority)},
                        {"default_category", jstr(c.default_category)},
                        {"cache_duration_minutes", jnum(static_cast<double>(c.cache_duration_minutes))}});
    if (include_has_token) o.set("has_token", jbool(c.github_token && !c.github_token->empty()));
    return o;
}

std::optional<MelpomeneConfig> merge_config(const MelpomeneConfig& base, const JsonValue& v, std::string& err) {
    if (!v.is_object()) {
        err = "config must be a JSON object";
        return std::nullopt;
    }
    MelpomeneConfig c = base;
    auto take_str = [&](const char* key, std::string& out) {
        const JsonValue* x = v.find(key);
        if (!x) return true;
        if (!x->is_string()) { err = std::string(key) + " must be a string"; return false; }
        out = x->s;
        return true;
    };
    if (!take_str("repository_owner", c.repository_owner) || !take_str("repository_name", c.repository_name) ||
        !take_str("default_priority", c.default_priority) || !take_str("default_category", c.default_category)) {
        return std::nullopt;
    }
    if ((!c.repository_owner.empty() && !is_valid_repo_part(c.repository_owner)) ||
        (!c.repository_name.empty() && !is_valid_repo_part(c.repository_name))) {
        err = "repository owner / name may only contain letters, digits, '.', '_' and '-'";
        return std::nullopt;
    }
    if (const JsonValue* l = v.find("default_labels")) {
        if (!l->is_array()) { err = "default_labels must be an array"; return std::nullopt; }
        c.default_labels.clear();
        for (const auto& x : *l->a) {
            if (!x.is_string()) { err = "default_labels must be strings"; return std::nullopt; }
            c.default_labels.push_back(x.s);
        }
    }
    if (const JsonValue* m = v.find("cache_duration_minutes")) {
        if (!m->is_number() || m->n < 0 || std::floor(m->n) != m->n) {
            err = "cache_duration_minutes must be a non-negative integer";
            return std::nullopt;
        }
        c.cache_duration_minutes = static_cast<uint64_t>(m->n);
    }
    if (const JsonValue* t = v.find("github_token")) {
        if (t->is_null()) c.github_token.reset();
        else if (t->is_string()) c.github_token = t->s;
        else { err = "github_token must be a string or null"; return std::nullopt; }
    }
    return c;
}

std::filesystem::path settings_path(const std::string& home) {
    if (home.empty()) return {};
    return std::filesystem::u8path(home) / ".ars" / "melpomene" / "settings.json";
}

ConfigStore::ConfigStore(std::filesystem::path file, std::shared_ptr<platform::ICredentialStore> creds)
    : file_(std::move(file)), creds_(std::move(creds)) {}

ConfigStore::Loaded ConfigStore::load() const {
    Loaded out;
    if (file_.empty()) {
        out.error = "settings path is not known (no home directory)";
        return out;
    }
    std::optional<std::string> legacy;
    std::error_code ec;
    if (std::filesystem::exists(file_, ec)) {
        std::string err;
        const auto text = read_text_file(file_, err);
        JsonValue v;
        if (!text) {
            out.error = err;
        } else if (!ergo::common::jsonm::parse(*text, v)) {
            out.error = file_.u8string() + ": not valid JSON";
        } else if (auto c = merge_config(out.config, v, err)) {
            legacy = c->github_token;   // plain text from an old version, if any
            c->github_token.reset();
            out.config = std::move(*c);
        } else {
            out.error = file_.u8string() + ": " + err;
        }
    }
    std::string cerr;
    out.config.github_token = creds_->get(kKeyringService, kKeyringAccount, cerr);
    if (!cerr.empty() && out.error.empty()) out.error = "credential store: " + cerr;
    if (legacy && !legacy->empty()) {
        // Move the old plain-text token into the store and rewrite the file without it.
        if (!out.config.github_token) out.config.github_token = legacy;
        std::string serr;
        if (!save(out.config, serr) && out.error.empty()) out.error = "migrating the plain-text token failed: " + serr;
    }
    return out;
}

bool ConfigStore::save(const MelpomeneConfig& c, std::string& err) const {
    if (file_.empty()) {
        err = "settings path is not known (no home directory)";
        return false;
    }
    if (!write_text_file(file_, to_json(to_json_value(c, false)), err)) return false;
    if (c.github_token && !c.github_token->empty()) return creds_->set(kKeyringService, kKeyringAccount, *c.github_token, err);
    return creds_->remove(kKeyringService, kKeyringAccount, err);
}

} // namespace ergo::studio::plugins::musa_melpomene
