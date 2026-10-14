#include "edit_support.h"

#include "commands.h"
#include "file_io.h"
#include "git_rev_resolver.h"

#include <cstdio>

namespace ergo::package_cli {

namespace pm = ergo::package_manager;

int resolve_with_git(const char* command, const Args& args, const std::string& repository,
                     const std::string& to, pm::RevResolution& out) {
    const std::string root = mirror_root(args.cache);
    if (root.empty()) {
        std::fprintf(stderr, "ergo_pkg %s: no place for the mirror (pass --cache <dir>)\n", command);
        return kExitIo;
    }
    GitRevResolver resolver(repository, root);
    std::string error;
    if (!resolver.prepare(error)) {
        std::fprintf(stderr, "ergo_pkg %s: %s\n", command, error.c_str());
        return kExitIo;
    }
    pm::RevRequest request;
    request.to             = to;
    request.allow_off_main = args.allow_off_main;
    out = pm::resolve_rev(request, resolver);
    if (out.status != pm::ResolveStatus::Ok) {
        std::fprintf(stderr, "ergo_pkg %s: %s\n", command, out.error.c_str());
        return out.status == pm::ResolveStatus::IoError ? kExitIo : kExitInput;
    }
    for (const auto& w : out.warnings) std::fprintf(stderr, "warning: %s\n", w.c_str());
    return kExitOk;
}

int write_edit(const char* command, const std::string& path, const pm::EditResult& edit) {
    if (!edit.ok) {
        std::fprintf(stderr, "ergo_pkg %s: %s\n", command, edit.error.c_str());
        return kExitInput;
    }
    if (edit.changed && !write_file(path, edit.text)) {
        std::fprintf(stderr, "ergo_pkg %s: cannot write %s\n", command, path.c_str());
        return kExitIo;
    }
    for (const auto& n : edit.notes) std::printf("%s\n", n.c_str());
    if (edit.changed) std::printf("wrote %s\n", path.c_str());
    else std::printf("ergo_pkg %s: nothing to change\n", command);
    return kExitOk;
}

} // namespace ergo::package_cli
