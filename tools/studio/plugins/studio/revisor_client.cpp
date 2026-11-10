#include "revisor_client.h"

#include "git_client.h"

#include <filesystem>
#include <system_error>

namespace ergo::studio::plugins::studio {
namespace {

const char* const kNoCli = "Revisor の CLI が見つかりません (LUDIARS_ROOT 未設定か未クローン)";

std::string last_line(const std::string& s) {
    std::string t = s;
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    const auto nl = t.rfind('\n');
    return nl == std::string::npos ? t : t.substr(nl + 1);
}

JsonValue str_or(const JsonValue& o, const char* k, bool nullable) {
    const JsonValue* v = field(o, k);
    if (v && v->is_string()) return *v;
    if (v && v->is_number()) return jstr(to_json(*v));
    return nullable ? JsonValue::make_null() : jstr("");
}

} // namespace

RevisorClient::RevisorClient(std::shared_ptr<platform::IProcessRunner> runner, std::optional<std::string> ludiars_root,
                             int timeout_ms)
    : runner_(std::move(runner)), root_(std::move(ludiars_root)), timeout_ms_(timeout_ms) {}

std::optional<std::string> RevisorClient::cli() const {
    if (!root_) return std::nullopt;
    const auto p = std::filesystem::u8path(*root_) / "Revisor" / "src" / "cli.mjs";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec)) return std::nullopt;
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

platform::ProcessResult RevisorClient::node(const std::vector<std::string>& args) const {
    platform::ProcessSpec spec;
    spec.program    = "node";
    spec.args       = args;
    spec.timeout_ms = timeout_ms_ * 2;
    return runner_->run(spec);
}

std::string RevisorClient::ping() const {
    const auto c = cli();
    if (!c) return kNoCli;
    const auto r = node({*c, "repo", "list", "--json"});
    if (r.started && r.exit_code == 0) return {};
    return r.started ? last_line(r.err).substr(0, 300) : process_failure(r, "node");
}

std::optional<JsonValue> RevisorClient::prs(const std::string& repository, std::string& err) const {
    const auto c = cli();
    if (!c) {
        err = kNoCli;
        return std::nullopt;
    }
    std::vector<std::string> args{*c, "pr", "list", "--json"};
    if (!repository.empty()) {
        args.push_back("--repository");
        args.push_back(repository);
    }
    const auto r = node(args);
    if (!r.started || r.exit_code != 0) {
        err = r.started ? last_line(r.err).substr(0, 300) : process_failure(r, "node");
        if (err.empty()) err = "Revisor 呼び出しに失敗";
        return std::nullopt;
    }
    JsonValue list;
    if (!ergo::common::jsonm::parse(r.out, list) || !list.is_array()) {
        err = "Revisor の応答が JSON ではありません";
        return std::nullopt;
    }
    JsonValue out = JsonValue::make_array();
    for (const auto& p : *list.a) {
        const JsonValue* num = field(p, "number");
        JsonValue o = jobj({{"number", jnum(num ? num->as_number() : 0)}, {"repository", str_or(p, "repository", false)},
                            {"title", str_or(p, "title", false)}, {"headRef", str_or(p, "headRef", false)},
                            {"baseRef", str_or(p, "baseRef", false)}, {"status", str_or(p, "status", false)},
                            {"checkStatus", str_or(p, "checkStatus", true)}});
        for (const char* k : {"updatedAt", "createdAt"}) {
            const JsonValue* v = field(p, k);
            if (v && v->is_string()) o.set(k, *v);
        }
        out.push(std::move(o));
    }
    return out;
}

} // namespace ergo::studio::plugins::studio
