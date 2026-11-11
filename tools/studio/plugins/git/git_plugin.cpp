#include "git_plugin.h"

#include "../common/json_parse.h"

#include <charconv>

namespace ergo::studio::plugins::git {
namespace fs = std::filesystem;

namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string text_of(const JsonValue& v, const char* key) {
    const JsonValue* f = field(v, key);
    return f != nullptr && f->is_string() ? f->as_string() : std::string();
}

net::HttpResponse answer(const GitOutcome& o) {
    return json_response(o.ok ? 200 : 409, jobj({{"ok", jbool(o.ok)}, {o.ok ? "message" : "err", jstr(o.message)}}));
}

} // namespace

GitPlugin::GitPlugin(GitOps ops, std::string home, std::string plugin_dirs, host::LogFn log)
    : info_{"git", "Git", "ゲームのワークスペースのリポジトリを、状態の確認・ブランチ・コミット・pull・push まで Studio から扱う。",
            "\xF0\x9F\x8C\xBF"},
      ops_(std::move(ops)),
      home_(std::move(home)),
      plugin_dirs_(std::move(plugin_dirs)),
      log_(std::move(log)) {}

std::vector<GitPlugin::Repo> GitPlugin::repos() const {
    std::vector<Repo> out;
    for (const auto& p : load_workspace_profiles(fs::u8path(home_), plugin_dirs_, &process_env).profiles) {
        for (const auto& path : p.git) {
            const bool seen = std::any_of(out.begin(), out.end(), [&](const Repo& r) { return r.path == path; });
            if (!seen) out.push_back({path, p.name});
        }
    }
    return out;
}

JsonValue GitPlugin::snapshot() const {
    const auto list = repos();
    JsonValue out = jobj({{"ok", jbool(!list.empty())}, {"repos", jnum(static_cast<double>(list.size()))}});
    if (list.empty()) out.set("err", jstr("no workspace profile lists a repository (profiles: <studio home>/profiles, ERGO_PLUGIN_DIR)"));
    return out;
}

net::HttpResponse GitPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health") return json_response(200, snapshot());
    const auto list = repos();
    if (sub == "/api/repos" && req.method == "GET") {
        JsonValue out = JsonValue::make_array();
        for (std::size_t i = 0; i < list.size(); ++i) {
            out.push(jobj({{"index", jnum(static_cast<double>(i))}, {"path", jstr(list[i].path.u8string())}, {"profile", jstr(list[i].profile)}}));
        }
        return json_response(200, out);
    }
    const std::string prefix = "/api/repos/";
    if (sub.rfind(prefix, 0) != 0) return fail(404, "not found");
    const std::string rest = sub.substr(prefix.size());
    const std::size_t slash = rest.find('/');
    std::size_t index = 0;
    const std::string number = rest.substr(0, slash);
    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), index);
    if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || index >= list.size()) {
        return fail(404, "no such repository");
    }
    return act(list[index], slash == std::string::npos ? std::string() : rest.substr(slash + 1), req);
}

net::HttpResponse GitPlugin::act(const Repo& repo, const std::string& what, const net::HttpRequest& req) {
    std::string err;
    if (req.method == "GET") {
        std::optional<JsonValue> out;
        if (what == "status") out = ops_.status(repo.path, err);
        else if (what == "log") out = ops_.log(repo.path, 30, err);
        else if (what == "branches") out = ops_.branches(repo.path, err);
        else return fail(404, "not found");
        if (!out) return fail(409, err);
        return json_response(200, *out);
    }
    if (req.method != "POST") return fail(405, "method not allowed");
    GitOutcome done;
    if (what == "fetch") done = ops_.fetch(repo.path);
    else if (what == "pull") done = ops_.pull(repo.path);
    else if (what == "push") done = ops_.push(repo.path);
    else if (what == "switch" || what == "commit") {
        const auto body = parse_json(req.body);
        if (!body) return fail(400, "body must be JSON");
        if (what == "switch") {
            const JsonValue* create = field(*body, "create");
            done = ops_.switch_to(repo.path, text_of(*body, "branch"), create != nullptr && create->as_bool());
        } else {
            done = ops_.commit_all(repo.path, text_of(*body, "message"));
        }
    } else {
        return fail(404, "not found");
    }
    if (log_) log_(done.ok ? "info" : "warn", "[git] " + what + " " + repo.path.u8string() + ": " + done.message);
    return answer(done);
}

std::unique_ptr<host::IPlugin> make_git_plugin(const std::string& studio_home, const std::string& plugin_dirs, int timeout_ms,
                                               const host::LogFn& log) {
    return std::make_unique<GitPlugin>(GitOps(platform::make_system_process_runner(), timeout_ms), studio_home, plugin_dirs, log);
}

} // namespace ergo::studio::plugins::git
