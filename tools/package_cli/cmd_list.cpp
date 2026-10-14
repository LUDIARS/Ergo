// ergo_pkg list — 宣言・Ergo のマニフェスト・resolved.json・UPM の宣言を合わせた一覧。 何も書き換えない。

#include "commands.h"
#include "file_io.h"
#include "host_context.h"
#include "upm_host.h"

#include "ergo/package_manager/package_list.h"

#include <cstdio>

namespace ergo::package_cli {

int run_list(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg list: %s\n", error.c_str());
        return kExitInput;
    }

    jsonm::JsonValue resolved;
    bool has_resolved = false;
    if (!args.build.empty()) {
        if (!read_json(join(args.build, "ergo-packages.resolved.json"), resolved, &error)) {
            std::fprintf(stderr, "ergo_pkg list: %s\n", error.c_str());
            return kExitInput;
        }
        has_resolved = true;
    }
    pm::UpmDeclaration upm;
    const bool has_upm = load_upm_declaration(args.host, upm);

    pm::ListInput in;
    in.host     = ctx.has_declaration ? &ctx.declaration : nullptr;
    in.ergo     = ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr;
    in.resolved = has_resolved ? &resolved : nullptr;
    in.upm      = has_upm ? &upm : nullptr;
    const auto rows = pm::make_list(in);
    const std::string code = ctx.has_declaration ? ctx.declaration.code : upm.code;

    if (args.json) {
        std::printf("%s\n", jsonm::serialize(pm::list_to_json(rows, code)).c_str());
    } else {
        for (const auto& n : ctx.notes) std::fprintf(stderr, "ergo_pkg list: %s\n", n.c_str());
        std::printf("host: %s\n%s", code.empty() ? "-" : code.c_str(), pm::render_list_text(rows).c_str());
    }
    return kExitOk;
}

} // namespace ergo::package_cli
