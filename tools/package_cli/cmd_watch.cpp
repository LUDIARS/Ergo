// ergo_pkg watch — 定期実行の本体 (spec/feature/package-manager.md §2.2 / §3 / §4)。
//
//   判定 → 前回状態との差分 → 新しい出来事を <out>/events.json、 ホストごとの報告を <host>/.ergo/watch-report.json、
//   次の状態を <state>/<code>.watch-state.json へ。 配送 (Discord) はしない (Excubitor のジョブが events.json を読む)。
//
// 書く前に全ての報告と出来事を漏れ検査 (§8) に通し、 当たれば何も書かずに終了 3。 取れなかった依存は
// その依存の前回状態を変えない。

#include "commands.h"
#include "file_io.h"
#include "ref_source_router.h"
#include "watch_host.h"
#include "watch_observe.h"
#include "watch_paths.h"

#include "ergo/package_watch/watch_report.h"

#include <cstdio>
#include <ctime>
#include <set>

namespace ergo::package_cli {

namespace {

struct LoadedHost {
    WatchHost      host;
    pw::WatchState previous;
};

/// 前回状態 (無ければ空)。 壊れていれば false。
bool load_state(const std::string& path, const std::string& code, pw::WatchState& out, std::string* error) {
    out      = {};
    out.host = code;
    if (!read_file(path)) return true;
    jsonm::JsonValue doc;
    std::string why;
    if (!read_json(path, doc, &why) || !pw::read_state(doc, out, &why)) {
        if (error) *error = path + ": " + why + " (remove it to start over)";
        return false;
    }
    return true;
}

bool load_hosts(const Args& args, const std::string& state_dir, std::vector<LoadedHost>& out,
                std::string* error) {
    const std::vector<std::string> dirs = args.hosts.empty() ? std::vector<std::string>{args.host} : args.hosts;
    std::set<std::string> codes;
    for (const auto& dir : dirs) {
        LoadedHost h;
        if (!load_watch_host(args, dir, h.host, error)) return false;
        if (!codes.insert(h.host.code).second) {
            if (error) *error = "two hosts share the code '" + h.host.code + "'";
            return false;
        }
        if (!load_state(state_file(state_dir, h.host.code), h.host.code, h.previous, error)) return false;
        out.push_back(std::move(h));
    }
    return true;
}

bool write_all(const std::vector<LoadedHost>& hosts, const std::vector<pw::HostWatch>& watches,
               const pw::WatchOutputs& outputs, const std::string& state_dir, const std::string& out_dir) {
    for (std::size_t i = 0; i < hosts.size(); ++i) {
        if (!write_file(join(hosts[i].host.dir, kWatchReportPath), outputs.reports[i].content)) return false;
    }
    if (!write_file(join(out_dir, kEventsFile), outputs.events.content)) return false;
    for (const auto& w : watches) {
        const std::string text = jsonm::serialize(pw::state_to_json(w.diff.next)) + "\n";
        if (!write_file(state_file(state_dir, w.host), text)) return false;
    }
    return true;
}

void print_summary(const std::vector<pw::HostWatch>& watches) {
    std::size_t total = 0;
    for (const auto& w : watches) {
        for (const auto& r : w.results) {
            if (!r.observation.observed) {
                std::printf("%s/%s: unobserved (%s)\n", w.host.c_str(), r.dependency.name.c_str(),
                            r.observation.reason.c_str());
            }
        }
        for (const auto& e : w.diff.events) {
            std::printf("new %s: %s -> %s\n", pw::update_kind_name(e.update.kind), e.id.c_str(),
                        e.update.next.c_str());
            ++total;
        }
    }
    std::printf("ergo_pkg watch: %zu new event(s) for %zu host(s)\n", total, watches.size());
}

} // namespace

int run_watch(const Args& args) {
    const std::string state_dir = watch_state_dir(args);
    if (state_dir.empty()) {
        std::fprintf(stderr, "ergo_pkg watch: no state directory (pass --state <dir>)\n");
        return kExitInput;
    }
    const std::string out_dir = args.out.empty() ? state_dir : args.out;

    std::vector<LoadedHost> hosts;
    std::string error;
    if (!load_hosts(args, state_dir, hosts, &error)) {
        std::fprintf(stderr, "ergo_pkg watch: %s\n", error.c_str());
        return kExitInput;
    }

    RefSourceRouter source(watch_sibling_root(args), watch_cache_dir(args));
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    std::vector<pw::HostWatch> watches;
    for (const auto& h : hosts) {
        for (const auto& n : h.host.notes) std::fprintf(stderr, "ergo_pkg watch: %s: %s\n", h.host.code.c_str(), n.c_str());
        pw::HostWatch w;
        w.host    = h.host.code;
        w.results = observe_host(h.host, source, source, h.previous);
        w.diff    = pw::diff(h.previous, w.host, w.results, now);
        watches.push_back(std::move(w));
    }

    const auto outputs = pw::prepare_watch_outputs(watches, now);
    if (!outputs.ok) {
        for (const auto& leak : outputs.leaks) {
            std::fprintf(stderr, "ergo_pkg watch: LEAK %s: %s\n", leak.where.c_str(), leak.reason.c_str());
        }
        std::fprintf(stderr, "ergo_pkg watch: nothing written (the report would leak, spec §8)\n");
        return kExitIo;
    }
    if (!write_all(hosts, watches, outputs, state_dir, out_dir)) {
        std::fprintf(stderr, "ergo_pkg watch: could not write the report, events or state\n");
        return kExitIo;
    }
    print_summary(watches);
    return kExitOk;
}

} // namespace ergo::package_cli
