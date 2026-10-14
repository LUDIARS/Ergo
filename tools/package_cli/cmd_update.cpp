// ergo_pkg update <name> --to <tag|main|sha> — pin を 40 桁 SHA に解決して持ち主のファイルの rev と expect_tag を書く。
// ergo と非公開はホストの宣言、 共有パッケージは --ergo で明示した Ergo のマニフェスト。 取り違えは終了 2。
// コミット・ブランチ・PR はしない (次の操作を表示するだけ)。

#include "commands.h"
#include "edit_support.h"
#include "file_io.h"
#include "host_context.h"

#include "ergo/package_manager/init_plan.h"
#include "ergo/package_manager/package_owner.h"

#include <cstdio>

namespace ergo::package_cli {

namespace {

struct Target {
    std::string path;        ///< 書き換えるファイル
    std::string text;
    std::string repository;  ///< 解決に使う取得元
    std::string where;       ///< 次の操作の表示 (どのリポで PR を出すか)
};

/// 持ち主から書き換えるファイルを決める。 誤りなら終了コード、 決まれば 0。
int pick_target(const Args& args, const HostContext& ctx, pm::PackageOwner owner, const std::string& name,
                Target& out) {
    const char* host_file = pm::kHostDeclarationPath;
    switch (owner) {
        case pm::PackageOwner::HostErgo:
        case pm::PackageOwner::HostPrivate:
            if (!ctx.has_declaration || !ctx.declaration_text) {
                std::fprintf(stderr, "ergo_pkg update: %s is owned by the host, but %s has no host declaration\n",
                             name.c_str(), join(args.host, host_file).c_str());
                return kExitInput;
            }
            out.path  = join(args.host, host_file);
            out.text  = *ctx.declaration_text;
            out.where = "the host repository";
            if (owner == pm::PackageOwner::HostErgo) {
                out.repository = ctx.declaration.ergo.repository;
            } else {
                for (const auto& p : ctx.declaration.packages) {
                    if (p.name == name) out.repository = p.repository;
                }
            }
            return kExitOk;
        case pm::PackageOwner::ErgoShared: {
            if (!args.ergo_explicit) {
                std::fprintf(stderr,
                             "ergo_pkg update: %s is a shared package pinned by the Ergo manifest; pass --ergo <ergo "
                             "checkout> to rewrite it there (the host takes it by raising its Ergo pin)\n",
                             name.c_str());
                return kExitInput;
            }
            const auto* pkg = ctx.ergo_manifest.find(name);
            if (!pkg || pkg->kind != "link" || pkg->repository.empty()) {
                std::fprintf(stderr, "ergo_pkg update: %s has no repository/rev pin (a tool follows its gitlink)\n",
                             name.c_str());
                return kExitInput;
            }
            out.path = join(ctx.ergo_dir, "ergo-packages.json");
            const auto text = read_file(out.path);
            if (!text) {
                std::fprintf(stderr, "ergo_pkg update: cannot read %s\n", out.path.c_str());
                return kExitIo;
            }
            out.text       = *text;
            out.repository = pkg->repository;
            out.where      = "the Ergo repository";
            return kExitOk;
        }
        case pm::PackageOwner::Conflict:
            std::fprintf(stderr, "ergo_pkg update: %s is both private and in the Ergo manifest (PKG-15)\n",
                         name.c_str());
            return kExitInput;
        case pm::PackageOwner::Unknown:
            break;
    }
    std::fprintf(stderr, "ergo_pkg update: %s is neither ergo, a private package of the host nor a package of "
                         "the Ergo manifest\n",
                 name.c_str());
    return kExitInput;
}

} // namespace

int run_update(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg update: %s\n", error.c_str());
        return kExitInput;
    }
    const std::string& name = args.names.front();
    const auto owner = pm::owner_of(name, ctx.has_declaration ? &ctx.declaration : nullptr,
                                    ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr);
    Target target;
    if (const int rc = pick_target(args, ctx, owner, name, target); rc != kExitOk) return rc;

    pm::RevResolution rev;
    if (const int rc = resolve_with_git("update", args, target.repository, args.to, rev); rc != kExitOk) return rc;

    const auto edit = pm::plan_pin(target.text, pm::pin_path(owner, name), rev.rev, rev.expect_tag);
    if (const int rc = write_edit("update", target.path, edit); rc != kExitOk) return rc;

    std::printf("%s -> %s%s (owner: %s)\n", name.c_str(), rev.rev.c_str(),
                rev.expect_tag.empty() ? "" : (" " + rev.expect_tag).c_str(), pm::owner_name(owner));
    std::printf("STRICT: %s\n", rev.expect_tag.empty() || rev.off_main ? "does not pass (untagged or off-main pin)"
                                                                       : "passes the pin rules");
    std::printf("next: in %s, cut a feat branch, commit this change and open a PR (ergo_pkg does not commit)\n",
                target.where.c_str());
    return kExitOk;
}

} // namespace ergo::package_cli
