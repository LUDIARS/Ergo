// ergo_pkg status — P4 ホスト状態図 (spec/feature/package-manager.md §5.1) の入力
// `<build>/ergo-packages.status.json` を書く。 載せるのはホストのコード・HEAD・構成時刻 (CMakeCache.txt の
// 更新時刻)・テストの数 (`--junit` の XML を優先、 無ければ LastTest.log、 無ければ unknown) だけ。
// ビルドの成否と CI の結果は取らない (§9 W-6)。 書く前に漏れ検査 (§8) を通し、 当たれば書かずに終了 3。

#include "commands.h"
#include "file_io.h"
#include "host_context.h"
#include "process.h"
#include "status_inputs.h"

#include "ergo/package_manager/host_status.h"
#include "ergo/package_manager/leak_check.h"

#include <cstdio>

namespace ergo::package_cli {

namespace {

/// ホストのコード: 宣言の host.code → resolved.json の host.id → ""。
std::string host_code(const HostContext& ctx, const std::string& build) {
    if (ctx.has_declaration && !ctx.declaration.code.empty()) return ctx.declaration.code;
    jsonm::JsonValue resolved;
    if (!read_json(join(build, "ergo-packages.resolved.json"), resolved, nullptr)) return {};
    const auto* host = resolved.find("host");
    if (!host || !host->is_object()) return {};
    const auto* id = host->find("id");
    return id ? id->as_string() : std::string{};
}

} // namespace

int run_status(const Args& args) {
    if (args.build.empty()) {
        std::fprintf(stderr, "ergo_pkg status: needs --build <dir>\n");
        return kExitInput;
    }
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg status: %s\n", error.c_str());
        return kExitInput;
    }

    pm::HostStatusInput in;
    in.host          = host_code(ctx, args.build);
    in.head          = git_head(args.host);
    in.configured_at = file_mtime_utc(join(args.build, "CMakeCache.txt"));
    if (!collect_test_tally(args.build, args.junit, in.tests, &error)) {
        std::fprintf(stderr, "ergo_pkg status: %s\n", error.c_str());
        return kExitInput;
    }

    const auto doc   = pm::host_status_to_json(in);
    const auto leaks = pm::find_leaks(doc);
    if (!leaks.empty()) {
        for (const auto& l : leaks) {
            std::fprintf(stderr, "ergo_pkg status: leak at %s: %s\n", l.where.c_str(), l.reason.c_str());
        }
        return kExitIo;
    }
    const std::string out = args.out.empty() ? join(args.build, "ergo-packages.status.json") : args.out;
    if (!write_file(out, pm::host_status_text(in))) {
        std::fprintf(stderr, "ergo_pkg status: cannot write %s\n", out.c_str());
        return kExitIo;
    }
    std::printf("host: %s  tests: %s  configured: %s\n", in.host.empty() ? "-" : in.host.c_str(),
                in.tests.source.c_str(), in.configured_at.empty() ? "-" : in.configured_at.c_str());
    std::printf("wrote %s\n", out.c_str());
    return kExitOk;
}

} // namespace ergo::package_cli
