// ergo_pkg remove <name> — host.use / optional / features と非公開 packages から外す。
// Ergo のモジュールが modules.*.needs に持つ名前は拒む (終了 2)。 ネットワークに出ない。

#include "commands.h"
#include "edit_support.h"
#include "file_io.h"
#include "host_context.h"

#include "ergo/package_manager/init_plan.h"

#include <cstdio>

namespace ergo::package_cli {

int run_remove(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg remove: %s\n", error.c_str());
        return kExitInput;
    }
    const std::string path = join(args.host, pm::kHostDeclarationPath);
    if (!ctx.has_declaration || !ctx.declaration_text) {
        std::fprintf(stderr, "ergo_pkg remove: %s has no host declaration\n", path.c_str());
        return kExitInput;
    }
    const pm::ErgoManifest* ergo = ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr;
    return write_edit("remove", path, pm::plan_remove(*ctx.declaration_text, ergo, args.names.front()));
}

} // namespace ergo::package_cli
