#include "ergo/package_manager/init_plan.h"

#include "ergo/common/json_min.h"
#include "ergo/package_manager/bootstrap.h"
#include "ergo/package_manager/host_declaration.h"

#include <cctype>

namespace ergo::package_manager {

const char* const kHostDeclarationPath = "ergo-packages.json";
const char* const kGitignorePath       = ".gitignore";
const char* const kUpmHostPath         = ".ergo/host.json";
const char* const kErgoRepository      = "https://github.com/LUDIARS/Ergo.git";

namespace {

namespace jsonm = ergo::common::jsonm;

std::string trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

void plan_upm(const InitOptions& options, const ExistingFiles& existing, InitPlan& plan) {
    if (existing.upm_host) {
        plan.notes.push_back(std::string(kUpmHostPath) + " already exists (unchanged)");
        return;
    }
    plan.writes.push_back({kUpmHostPath, render_upm_host(options.code)});
}

void plan_declaration(const InitOptions& options, const ExistingFiles& existing, InitPlan& plan) {
    if (!existing.declaration) {
        plan.writes.push_back({kHostDeclarationPath, render_host_declaration(options.code, options.ergo_rev)});
        return;
    }
    jsonm::JsonValue doc;
    if (!jsonm::parse(*existing.declaration, doc) || !is_host_declaration(doc)) {
        plan.ok    = false;
        plan.error = std::string(kHostDeclarationPath) +
                     " exists but is not a host declaration (no `host` section); is this the Ergo checkout?";
        return;
    }
    plan.notes.push_back(std::string(kHostDeclarationPath) + " already declares the host (unchanged)");
}

void plan_bootstrap(const InitOptions& options, const ExistingFiles& existing, InitPlan& plan) {
    const std::string* text = existing.bootstrap ? &*existing.bootstrap : nullptr;
    const auto status = check_bootstrap(text, options.bootstrap_template);
    switch (status.state) {
    case BootstrapState::Current:
        return;
    case BootstrapState::Foreign:
        plan.notes.push_back(std::string(kBootstrapPath) +
                             " has no ergo-bootstrap-source line (hand-written, left unchanged)");
        return;
    case BootstrapState::Stale:
        plan.notes.push_back(std::string(kBootstrapPath) + " was stale (from " + status.source +
                             "), rewritten");
        break;
    case BootstrapState::Missing:
        break;
    }
    plan.writes.push_back({kBootstrapPath, render_bootstrap(options.bootstrap_template, options.ergo_rev)});
}

void plan_gitignore(const ExistingFiles& existing, InitPlan& plan) {
    std::string out;
    if (add_gitignore_entry(existing.gitignore.value_or(std::string()), out)) {
        plan.writes.push_back({kGitignorePath, out});
    }
}

} // namespace

bool is_valid_code(const std::string& code) {
    if (code.empty() || !std::isalpha(static_cast<unsigned char>(code[0]))) return false;
    for (const char c : code) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
    }
    return true;
}

std::string render_host_declaration(const std::string& code, const std::string& ergo_rev) {
    std::string out = "{\n  \"schema\": 1,\n  \"host\": {\n";
    if (!code.empty()) out += "    \"code\": \"" + code + "\",\n";
    out += "    \"ergo\": { \"repository\": \"" + std::string(kErgoRepository) + "\", \"rev\": \"" +
           ergo_rev + "\" },\n";
    out += "    \"use\": [],\n    \"optional\": [],\n    \"features\": {},\n    \"legacy_vars\": {}\n";
    out += "  },\n  \"packages\": {}\n}\n";
    return out;
}

std::string render_upm_host(const std::string& code) {
    std::string out = "{\n  \"schema\": 1";
    if (!code.empty()) out += ",\n  \"code\": \"" + code + "\"";
    out += "\n}\n";
    return out;
}

bool add_gitignore_entry(const std::string& in, std::string& out) {
    std::size_t begin = 0;
    while (begin <= in.size()) {
        std::size_t end = in.find('\n', begin);
        if (end == std::string::npos) end = in.size();
        const std::string line = trim(in.substr(begin, end - begin));
        if (line == ".ergo/" || line == ".ergo" || line == "/.ergo/" || line == "/.ergo") return false;
        begin = end + 1;
    }
    out = in;
    if (!out.empty() && out.back() != '\n') out += (out.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
    out += (in.find("\r\n") != std::string::npos) ? ".ergo/\r\n" : ".ergo/\n";
    return true;
}

InitPlan plan_init(const InitOptions& options, const ExistingFiles& existing) {
    InitPlan plan;
    if (!options.code.empty() && !is_valid_code(options.code)) {
        plan.ok    = false;
        plan.error = "--code '" + options.code + "' is not a project code (letters, digits, '-', '_')";
        return plan;
    }
    if (options.upm) {
        plan_upm(options, existing, plan);
        return plan;
    }
    plan_declaration(options, existing, plan);
    if (!plan.ok) return plan;
    plan_bootstrap(options, existing, plan);
    plan_gitignore(existing, plan);
    return plan;
}

} // namespace ergo::package_manager
