#include "plugin_installer.h"

#include "endpoints.h"
#include "git_client.h"

#include "ergo/studio/plugin_abi.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <random>
#include <system_error>

namespace ergo::studio::plugins::studio {
namespace fs = std::filesystem;
namespace {

constexpr auto kPreviewTtl = std::chrono::minutes(10);

std::string utf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string random_token() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    static const char* hx = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < 32; ++i) s.push_back(hx[gen() & 15]);
    return s;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string repo_url(const PluginSource& s) { return "https://github.com/" + s.owner + "/" + s.repo + ".git"; }

/// UTC ISO-8601 of now + `d` (the preview expiry shown to the person approving).
std::string iso_after(std::chrono::seconds d) {
    const auto t    = std::chrono::system_clock::now() + d;
    const auto secs = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                  tm.tm_min, tm.tm_sec);
    return buf;
}

} // namespace

JsonValue to_json_value(const PluginPreview& p) {
    JsonValue plugins = JsonValue::make_array();
    for (const auto& n : p.plugins) plugins.push(jstr(n));
    return jobj({{"token", jstr(p.token)}, {"id", jstr(p.id)}, {"owner", jstr(p.owner)}, {"repo", jstr(p.repo)},
                 {"subpath", jstr(p.subpath)}, {"rev", jstr(p.rev)}, {"requested", jstr(p.requested)},
                 {"commit", jobj({{"subject", jstr(p.commit_subject)}, {"date", jstr(p.commit_date)}})}, {"plugins", plugins},
                 {"build", jstr("cmake")}, {"replaces", p.replaces ? jstr(*p.replaces) : JsonValue::make_null()},
                 {"expiresAt", jstr(p.expires_at)}});
}

PluginInstaller::PluginInstaller(fs::path home, fs::path sdk_dir, std::shared_ptr<platform::IProcessRunner> runner, int timeout_ms)
    : home_(std::move(home)), sdk_dir_(std::move(sdk_dir)), runner_(std::move(runner)), timeout_ms_(timeout_ms) {}

platform::ProcessResult PluginInstaller::run(const std::string& program, const std::vector<std::string>& args,
                                             const fs::path& cwd, int timeout_ms) const {
    platform::ProcessSpec spec;
    spec.program    = program;
    spec.args       = args;
    spec.cwd        = cwd;
    spec.timeout_ms = timeout_ms;
    return runner_->run(spec);
}

bool PluginInstaller::git(const std::vector<std::string>& args, const fs::path& cwd, std::string& out, std::string& err) const {
    const auto r = run("git", args, cwd, timeout_ms_ * 8);
    if (r.started && r.exit_code == 0) {
        out = r.out;
        return true;
    }
    err = process_failure(r, "git " + (args.empty() ? std::string() : args[0]));
    return false;
}

std::optional<std::string> PluginInstaller::resolve_rev(const PluginSource& s, std::string& err) const {
    if (is_full_sha(s.rev)) return s.rev;
    const auto r = run("git", {"ls-remote", repo_url(s), s.rev}, {}, timeout_ms_ * 4);
    if (!r.started || r.exit_code != 0) {
        err = "rev を解決できません: " + process_failure(r, "git ls-remote");
        return std::nullopt;
    }
    const std::string first = trim(r.out.substr(0, r.out.find('\n')));
    const std::string sha   = first.substr(0, first.find_first_of(" \t"));
    if (!is_full_sha(sha)) {
        err = "rev " + s.rev + " が見つかりません";
        return std::nullopt;
    }
    return sha;
}

void PluginInstaller::sweep() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = staged_.begin(); it != staged_.end();) {
        if (now - it->second.at > kPreviewTtl) {
            std::error_code ec;
            fs::remove_all(it->second.dir, ec);
            it = staged_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<PluginPreview> PluginInstaller::preview(const PluginSource& s, std::string& err) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        sweep();
    }
    if (home_.empty()) {
        err = "Studio のデータ置き場が決まりません (ERGO_STUDIO_HOME)";
        return std::nullopt;
    }
    const auto sha = resolve_rev(s, err);
    if (!sha) return std::nullopt;
    const std::string token = random_token();
    const fs::path dir = home_ / "plugins" / "staging" / token;
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto fail = [&](const std::string& e) -> std::optional<PluginPreview> {
        err = e;
        std::error_code e2;
        fs::remove_all(dir, e2);
        return std::nullopt;
    };
    std::string out;
    if (!git({"init", "-q"}, dir, out, err) || !git({"remote", "add", "origin", repo_url(s)}, dir, out, err) ||
        !git({"sparse-checkout", "set", "--no-cone", "/" + s.subpath + "/"}, dir, out, err) ||
        !git({"fetch", "-q", "--depth", "1", "origin", *sha}, dir, out, err) || !git({"checkout", "-q", "FETCH_HEAD"}, dir, out, err)) {
        return fail(err);
    }
    std::string log;
    if (!git({"log", "-1", "--format=%s%x00%cI"}, dir, log, err)) return fail(err);
    log = trim(log);
    const auto nul = log.find('\0');

    const fs::path pack = dir / fs::u8path(s.subpath);
    if (!fs::is_directory(pack, ec)) return fail(s.subpath + " はこのリビジョンにありません");
    PluginPreview p;
    for (const auto& ent : fs::directory_iterator(pack, ec)) {
        std::error_code e2;
        if (ent.is_directory(e2) && fs::is_regular_file(ent.path() / "CMakeLists.txt", e2)) p.plugins.push_back(utf8(ent.path().filename()));
    }
    if (p.plugins.empty()) return fail("プラグイン (CMakeLists.txt を持つサブディレクトリ) がありません");
    std::sort(p.plugins.begin(), p.plugins.end());
    p.token          = token;
    p.id             = pack_id(s);
    p.owner          = s.owner;
    p.repo           = s.repo;
    p.subpath        = s.subpath;
    p.rev            = *sha;
    p.requested      = s.rev;
    p.commit_subject = nul == std::string::npos ? log : log.substr(0, nul);
    p.commit_date    = nul == std::string::npos ? std::string() : log.substr(nul + 1);
    for (const auto& e : read_ledger(home_).packs) {
        if (e.id == p.id) p.replaces = e.rev;
    }
    p.expires_at = iso_after(kPreviewTtl);
    std::lock_guard<std::mutex> lk(mtx_);
    staged_[token] = {p, dir, std::chrono::steady_clock::now()};
    return p;
}

bool PluginInstaller::build_plugin(const fs::path& dir, std::string& err) const {
    const fs::path build = dir / "build";
    const std::string sdk_include = utf8(sdk_dir_ / "include");
    auto r = run("cmake", {"-S", utf8(dir), "-B", utf8(build), "-DERGO_STUDIO_SDK_INCLUDE=" + sdk_include}, dir, timeout_ms_ * 30);
    if (!r.started || r.exit_code != 0) {
        err = utf8(dir.filename()) + ": cmake configure: " + process_failure(r, "cmake");
        return false;
    }
    r = run("cmake", {"--build", utf8(build), "--config", "Release"}, dir, timeout_ms_ * 60);
    if (!r.started || r.exit_code != 0) {
        err = utf8(dir.filename()) + ": cmake build: " + process_failure(r, "cmake --build");
        return false;
    }
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(build, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->path().filename() == ERGO_STUDIO_PLUGIN_FILE) {
            fs::copy_file(it->path(), dir / ERGO_STUDIO_PLUGIN_FILE, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                err = "copy " + std::string(ERGO_STUDIO_PLUGIN_FILE) + ": " + ec.message();
                return false;
            }
            return true;
        }
    }
    err = utf8(dir.filename()) + ": the build produced no " + ERGO_STUDIO_PLUGIN_FILE;
    return false;
}

std::optional<LedgerPack> PluginInstaller::install(const std::string& token, std::string& err) {
    Staged st;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        sweep();
        const auto it = staged_.find(token);
        if (it == staged_.end()) {
            err = "プレビューが見つからないか期限切れです。もう一度プレビューしてください";
            return std::nullopt;
        }
        st = it->second;
        staged_.erase(it);
    }
    const PluginPreview& p = st.preview;
    std::string flat = p.subpath;
    for (std::size_t i = 0; (i = flat.find('/', i)) != std::string::npos;) flat.replace(i, 1, "__");
    const fs::path checkout = home_ / "plugins" / "packs" / fs::u8path(p.owner) / fs::u8path(p.repo) / p.rev / fs::u8path(flat);
    std::error_code ec;
    if (!fs::exists(checkout, ec)) {
        fs::create_directories(checkout.parent_path(), ec);
        fs::rename(st.dir, checkout, ec);
        if (ec) {
            err = "staging を移せません: " + ec.message();
            fs::remove_all(st.dir, ec);
            return std::nullopt;
        }
    } else {
        fs::remove_all(st.dir, ec);
    }
    const fs::path pack = checkout / fs::u8path(p.subpath);
    for (const auto& name : p.plugins) {
        if (!build_plugin(pack / fs::u8path(name), err)) return std::nullopt;
    }
    LedgerPack entry{p.id, p.owner, p.repo, p.subpath, p.rev, utf8(checkout), true, now_iso_utc(), p.plugins};
    Ledger l = read_ledger(home_);
    l.packs.erase(std::remove_if(l.packs.begin(), l.packs.end(), [&](const LedgerPack& x) { return x.id == p.id; }), l.packs.end());
    l.packs.push_back(entry);
    if (!write_ledger(home_, l, err)) return std::nullopt;
    return entry;
}

std::optional<LedgerPack> PluginInstaller::set_enabled(const std::string& id, bool enabled, std::string& err) {
    Ledger l = read_ledger(home_);
    for (auto& p : l.packs) {
        if (p.id != id) continue;
        p.enabled = enabled;
        if (!write_ledger(home_, l, err)) return std::nullopt;
        return p;
    }
    err = "パックが見つかりません";
    return std::nullopt;
}

bool PluginInstaller::remove(const std::string& id, std::string& err) {
    Ledger l = read_ledger(home_);
    const auto it = std::find_if(l.packs.begin(), l.packs.end(), [&](const LedgerPack& p) { return p.id == id; });
    if (it == l.packs.end()) {
        err = "パックが見つかりません";
        return false;
    }
    const std::string dir = it->checkout_dir;
    l.packs.erase(it);
    if (!write_ledger(home_, l, err)) return false;
    const bool shared = std::any_of(l.packs.begin(), l.packs.end(), [&](const LedgerPack& p) { return p.checkout_dir == dir; });
    if (!shared) {
        std::error_code ec;
        fs::remove_all(fs::u8path(dir), ec);
    }
    return true;
}

std::vector<LedgerPack> PluginInstaller::list() const { return read_ledger(home_).packs; }

} // namespace ergo::studio::plugins::studio
