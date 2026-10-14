// ergo_pkg init — ホストの宣言・ブートストラップ・.gitignore を用意する (--upm は .ergo/host.json だけ)。
// 2 回目は何も書かない。 ネットワークに出ない。

#include "commands.h"
#include "file_io.h"
#include "host_context.h"
#include "process.h"

#include "ergo/package_manager/bootstrap.h"
#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_manager/init_plan.h"

#include <cstdio>

namespace ergo::package_cli {

int run_init(const Args& args) {
    pm::InitOptions options;
    options.code = args.code;
    options.upm  = args.upm;

    pm::ExistingFiles existing;
    if (options.upm) {
        existing.upm_host = read_file(join(args.host, pm::kUpmHostPath));
    } else {
        const std::string ergo = locate_ergo(args);
        if (ergo.empty()) {
            std::fprintf(stderr, "ergo_pkg init: Ergo checkout not found (pass --ergo <dir>)\n");
            return kExitInput;
        }
        options.ergo_rev = git_head(ergo);
        if (!pm::is_full_sha(options.ergo_rev)) {
            std::fprintf(stderr, "ergo_pkg init: cannot read the Ergo commit (git rev-parse HEAD)\n");
            return kExitIo;
        }
        options.bootstrap_template =
            read_file(join(ergo, "cmake/packages/ergo-bootstrap.cmake.in")).value_or(std::string());
        existing.declaration = read_file(join(args.host, pm::kHostDeclarationPath));
        existing.bootstrap   = read_file(join(args.host, pm::kBootstrapPath));
        existing.gitignore   = read_file(join(args.host, pm::kGitignorePath));
    }

    const auto plan = pm::plan_init(options, existing);
    if (!plan.ok) {
        std::fprintf(stderr, "ergo_pkg init: %s\n", plan.error.c_str());
        return kExitInput;
    }
    for (const auto& w : plan.writes) {
        if (!write_file(join(args.host, w.path), w.content)) {
            std::fprintf(stderr, "ergo_pkg init: cannot write %s\n", w.path.c_str());
            return kExitIo;
        }
        std::printf("wrote %s\n", w.path.c_str());
    }
    for (const auto& n : plan.notes) std::printf("%s\n", n.c_str());
    if (plan.writes.empty()) std::printf("ergo_pkg init: nothing to change\n");
    return kExitOk;
}

} // namespace ergo::package_cli
