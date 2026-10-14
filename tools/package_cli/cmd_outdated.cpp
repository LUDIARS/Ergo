// ergo_pkg outdated — §3 の判定 (tag / track / off-main) を今すぐ行って表示する。 前回状態は読み書きしない。
// 取れない依存は「未確認」と出し、 「更新なし」とは書かない。 --fail-on <kind> はその種類があれば終了 1。

#include "commands.h"
#include "file_io.h"
#include "ref_source_router.h"
#include "watch_host.h"
#include "watch_observe.h"
#include "watch_paths.h"

#include "ergo/package_watch/watch_report.h"

#include <cstdio>
#include <ctime>

namespace ergo::package_cli {

int run_outdated(const Args& args) {
    pw::UpdateKind fail_kind = pw::UpdateKind::Tag;
    if (!args.fail_on.empty() && !pw::parse_update_kind(args.fail_on, fail_kind)) {
        std::fprintf(stderr, "ergo_pkg outdated: --fail-on wants tag, track or off-main (got '%s')\n",
                     args.fail_on.c_str());
        return kExitInput;
    }
    WatchHost host;
    std::string error;
    if (!load_watch_host(args, args.host, host, &error)) {
        std::fprintf(stderr, "ergo_pkg outdated: %s\n", error.c_str());
        return kExitInput;
    }
    RefSourceRouter source(watch_sibling_root(args), watch_cache_dir(args));
    const auto results = observe_host(host, source, source, pw::WatchState{});
    const auto now     = static_cast<std::int64_t>(std::time(nullptr));

    if (args.json) {
        std::printf("%s\n", jsonm::serialize(pw::outdated_json(host.code, results, now)).c_str());
    } else {
        for (const auto& n : host.notes) std::fprintf(stderr, "ergo_pkg outdated: %s\n", n.c_str());
        std::printf("%s", pw::render_outdated_text(host.code, results).c_str());
    }
    if (args.fail_on.empty()) return kExitOk;
    for (const auto& r : results) {
        for (const auto& c : r.updates) {
            if (c.kind == fail_kind) return kExitFindings;
        }
    }
    return kExitOk;
}

} // namespace ergo::package_cli
