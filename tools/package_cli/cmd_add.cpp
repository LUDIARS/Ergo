// ergo_pkg add <name> — host.use に足す。 Ergo のマニフェストに無い名前は非公開 packages に足す
// (--repository と --rev が要る。 --rev がタグ・ブランチならミラーで解決する。 40 桁 SHA ならネットワークに出ない)。
// link の結線ファイルがホストに無ければ終了 2 で、 ファイルは変えない (結線は生成しない)。

#include "commands.h"
#include "edit_support.h"
#include "file_io.h"
#include "host_context.h"

#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_manager/init_plan.h"
#include "ergo/package_manager/package_owner.h"

#include <cstdio>

namespace ergo::package_cli {

int run_add(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg add: %s\n", error.c_str());
        return kExitInput;
    }
    const std::string path = join(args.host, pm::kHostDeclarationPath);
    if (!ctx.has_declaration || !ctx.declaration_text) {
        std::fprintf(stderr, "ergo_pkg add: %s has no host declaration (run `ergo_pkg init` first)\n", path.c_str());
        return kExitInput;
    }
    const pm::ErgoManifest* ergo = ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr;

    pm::AddRequest request;
    request.name          = args.names.front();
    request.optional      = args.optional;
    request.features      = args.features;
    request.kind          = args.kind;
    request.code          = args.code;
    request.repository    = args.repository;
    request.rev           = args.rev;
    request.wiring        = args.wiring;
    const std::string wiring = args.wiring.empty() ? pm::default_wiring_path(request.name) : args.wiring;
    request.wiring_exists = read_file(join(args.host, wiring)).has_value();

    const bool new_private = pm::owner_of(request.name, &ctx.declaration, ergo) == pm::PackageOwner::Unknown &&
                             ergo && !ergo->has_name(request.name);
    if (new_private && !args.rev.empty() && !args.repository.empty() && !pm::is_full_sha(args.rev)) {
        pm::RevResolution rev;
        if (const int rc = resolve_with_git("add", args, args.repository, args.rev, rev); rc != kExitOk) return rc;
        request.rev        = rev.rev;
        request.expect_tag = rev.expect_tag;
    }
    return write_edit("add", path, pm::plan_add(*ctx.declaration_text, ergo, request));
}

} // namespace ergo::package_cli
