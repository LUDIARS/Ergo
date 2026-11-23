#include "git_ops.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ergo::studio::plugins::git {
namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = s.find(sep, start);
        out.push_back(s.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) return out;
        start = end + 1;
    }
}

/// The porcelain v2 XY pair as a word a person reads.
std::string state_word(const std::string& xy) {
    if (xy.find('A') != std::string::npos) return "added";
    if (xy.find('D') != std::string::npos) return "deleted";
    if (xy.find('R') != std::string::npos) return "renamed";
    return "modified";
}

} // namespace

bool valid_branch(const std::string& name) {
    if (name.empty() || name.size() > 200 || name.front() == '-' || name.front() == '/' || name.back() == '/') return false;
    if (name.find("..") != std::string::npos || name.find("@{") != std::string::npos) return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '-' || c == '_' || c == '.';
    });
}

JsonValue parse_status(const std::string& porcelain) {
    JsonValue files = JsonValue::make_array();
    std::string branch, upstream;
    double ahead = 0, behind = 0;
    for (const std::string& line : lines_of(porcelain)) {
        if (line.rfind("# branch.head ", 0) == 0) branch = line.substr(14);
        else if (line.rfind("# branch.upstream ", 0) == 0) upstream = line.substr(18);
        else if (line.rfind("# branch.ab ", 0) == 0) {
            const auto parts = split(line.substr(12), ' ');
            if (parts.size() == 2) {
                ahead = std::abs(std::stod(parts[0]));
                behind = std::abs(std::stod(parts[1]));
            }
        } else if (line.rfind("1 ", 0) == 0 || line.rfind("2 ", 0) == 0) {
            // "1 XY sub mH mI mW hH hI path" / "2 XY ... Rscore path<TAB>orig"
            const auto parts = split(line, ' ');
            const std::size_t fields = line[0] == '1' ? 8 : 9;
            if (parts.size() <= fields) continue;
            std::string path = line.substr(line.find(parts[fields]));
            if (const std::size_t tab = path.find('\t'); tab != std::string::npos) path = path.substr(0, tab);
            files.push(jobj({{"path", jstr(path)}, {"state", jstr(state_word(parts[1]))}}));
        } else if (line.rfind("? ", 0) == 0) {
            files.push(jobj({{"path", jstr(line.substr(2))}, {"state", jstr("untracked")}}));
        } else if (line.rfind("u ", 0) == 0) {
            const auto parts = split(line, ' ');
            if (parts.size() > 10) files.push(jobj({{"path", jstr(parts.back())}, {"state", jstr("conflict")}}));
        }
    }
    return jobj({{"branch", jstr(branch)},
                 {"upstream", jstr(upstream)},
                 {"ahead", jnum(ahead)},
                 {"behind", jnum(behind)},
                 {"files", files}});
}

platform::ProcessResult GitOps::git(const fs::path& repo, std::vector<std::string> args, int timeout_ms) const {
    platform::ProcessSpec spec;
    spec.program = "git";
    spec.args = {"-C", repo.u8string()};
    spec.args.insert(spec.args.end(), args.begin(), args.end());
    spec.timeout_ms = timeout_ms;
    return runner_->run(spec);
}

GitOutcome GitOps::outcome(const platform::ProcessResult& r, const std::string& what) const {
    if (!r.started) return {false, what + ": " + (r.error.empty() ? "git did not start" : r.error)};
    if (r.timed_out) return {false, what + ": timed out"};
    const std::string text = trim(r.exit_code == 0 ? (r.out.empty() ? r.err : r.out) : (r.err.empty() ? r.out : r.err));
    return {r.exit_code == 0, text.empty() ? what : text};
}

std::optional<JsonValue> GitOps::status(const fs::path& repo, std::string& err) const {
    const auto r = git(repo, {"status", "--porcelain=v2", "--branch", "--untracked-files=all"}, timeout_ms_);
    if (!r.started || r.exit_code != 0) {
        err = outcome(r, "git status").message;
        return std::nullopt;
    }
    return parse_status(r.out);
}

std::optional<JsonValue> GitOps::log(const fs::path& repo, int count, std::string& err) const {
    const auto r = git(repo, {"log", "-" + std::to_string(std::clamp(count, 1, 200)), "--format=%h%x1f%s%x1f%an%x1f%cI"}, timeout_ms_);
    if (!r.started || r.exit_code != 0) {
        err = outcome(r, "git log").message;
        return std::nullopt;
    }
    JsonValue out = JsonValue::make_array();
    for (const std::string& line : lines_of(r.out)) {
        const auto parts = split(line, '\x1f');
        if (parts.size() < 4) continue;
        out.push(jobj({{"sha", jstr(parts[0])}, {"subject", jstr(parts[1])}, {"author", jstr(parts[2])}, {"date", jstr(parts[3])}}));
    }
    return out;
}

std::optional<JsonValue> GitOps::branches(const fs::path& repo, std::string& err) const {
    const auto r = git(repo, {"branch", "--format=%(HEAD)%(refname:short)"}, timeout_ms_);
    if (!r.started || r.exit_code != 0) {
        err = outcome(r, "git branch").message;
        return std::nullopt;
    }
    JsonValue out = JsonValue::make_array();
    for (const std::string& line : lines_of(r.out)) {
        const bool current = line.front() == '*';
        out.push(jobj({{"name", jstr(trim(line.substr(1)))}, {"current", jbool(current)}}));
    }
    return out;
}

GitOutcome GitOps::fetch(const fs::path& repo) const {
    return outcome(git(repo, {"fetch", "--prune", "origin"}, timeout_ms_ * 8), "git fetch");
}

GitOutcome GitOps::pull(const fs::path& repo) const {
    return outcome(git(repo, {"pull", "--ff-only"}, timeout_ms_ * 8), "git pull --ff-only");
}

GitOutcome GitOps::switch_to(const fs::path& repo, const std::string& branch, bool create) const {
    if (!valid_branch(branch)) return {false, "bad branch name: " + branch};
    std::vector<std::string> args{"switch"};
    if (create) args.push_back("-c");
    args.push_back(branch);
    return outcome(git(repo, args, timeout_ms_ * 2), "git switch");
}

GitOutcome GitOps::commit_all(const fs::path& repo, const std::string& message) const {
    if (trim(message).empty()) return {false, "a commit needs a message"};
    const GitOutcome added = outcome(git(repo, {"add", "-A"}, timeout_ms_ * 4), "git add -A");
    if (!added.ok) return added;
    // The message goes through a file: any text, any length, no quoting.
    static std::atomic<unsigned> counter{0};
    const fs::path file = fs::temp_directory_path() / ("ergo-studio-commit-" + std::to_string(++counter) + ".txt");
    {
        std::ofstream out(file, std::ios::binary);
        out << message;
    }
    const GitOutcome done = outcome(git(repo, {"commit", "-F", file.u8string()}, timeout_ms_ * 4), "git commit");
    std::error_code ec;
    fs::remove(file, ec);
    return done;
}

GitOutcome GitOps::push(const fs::path& repo) const {
    return outcome(git(repo, {"push", "--set-upstream", "origin", "HEAD"}, timeout_ms_ * 15), "git push");
}

} // namespace ergo::studio::plugins::git
