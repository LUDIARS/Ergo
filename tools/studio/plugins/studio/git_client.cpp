#include "git_client.h"

namespace ergo::studio::plugins::studio {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

JsonValue opt(const std::optional<std::string>& s) { return s ? jstr(*s) : JsonValue::make_null(); }

} // namespace

std::string process_failure(const platform::ProcessResult& r, const std::string& what) {
    if (!r.started) return r.error.empty() ? what + " を起動できません" : r.error;
    if (r.timed_out) return what + ": " + r.error;
    const std::string e = trim(r.err);
    return e.empty() ? what + " に失敗しました (exit " + std::to_string(r.exit_code) + ")" : e.substr(0, 300);
}

JsonValue to_json_value(const SourceStatus& s) {
    JsonValue last = JsonValue::make_null();
    if (!s.last_sha.empty()) last = jobj({{"sha", jstr(s.last_sha)}, {"subject", jstr(s.last_subject)}, {"date", jstr(s.last_date)}});
    return jobj({{"repoPath", jstr(s.repo_path)}, {"branch", opt(s.branch.branch)}, {"upstream", opt(s.branch.upstream)},
                 {"ahead", jnum(s.branch.ahead)}, {"behind", jnum(s.branch.behind)}, {"changed", jnum(s.branch.changed)},
                 {"untracked", jnum(s.branch.untracked)}, {"lastCommit", last}});
}

platform::ProcessResult GitClient::git(const std::vector<std::string>& args, int timeout_ms) const {
    platform::ProcessSpec spec;
    spec.program    = "git";
    spec.args       = args;
    spec.timeout_ms = timeout_ms;
    return runner_->run(spec);
}

std::optional<SourceStatus> GitClient::status(const std::string& repo, std::string& err) const {
    const auto st = git({"-C", repo, "status", "--porcelain=v2", "--branch"}, timeout_ms_);
    if (!st.started || st.exit_code != 0) {
        err = process_failure(st, "git status");
        return std::nullopt;
    }
    SourceStatus s;
    s.repo_path = repo;
    s.branch    = parse_branch_status(st.out);
    const auto log = git({"-C", repo, "log", "-1", "--format=%h%x00%s%x00%cI"}, timeout_ms_);
    if (log.started && log.exit_code == 0) {
        const std::string t = trim(log.out);
        const auto a = t.find('\0'), b = a == std::string::npos ? std::string::npos : t.find('\0', a + 1);
        if (a != std::string::npos) {
            s.last_sha     = t.substr(0, a);
            s.last_subject = t.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
            s.last_date    = b == std::string::npos ? std::string() : t.substr(b + 1);
        } else {
            s.last_sha = t;
        }
    }
    return s;
}

GitResult GitClient::fetch(const std::string& repo) const {
    const auto r = git({"-C", repo, "fetch", "--prune", "origin"}, timeout_ms_ * 8);
    if (r.started && r.exit_code == 0) return {true, "取得しました"};
    return {false, process_failure(r, "git fetch")};
}

GitResult GitClient::fast_forward(const std::string& repo, const std::string& branch) const {
    const auto r = git({"-C", repo, "merge", "--ff-only", "origin/" + branch}, timeout_ms_ * 4);
    if (r.started && r.exit_code == 0) return {true, "早送りしました"};
    return {false, process_failure(r, "git merge --ff-only")};
}

} // namespace ergo::studio::plugins::studio
