// ergo_pkg fetch — link パッケージを FetchContent と同じ置き場所へ事前取得する。
// 取得そのものは Ergo の cmake/packages/ErgoPackagePrefetch.cmake (FetchContent) に任せ、 ここは
// 名前と置き場所を決めて起動するだけ (取得エンジンを二重に持たない)。 以後の構成は
// ERGO_PACKAGES_FETCH=ON でもネットワークに出ずに経路 5 (fetch) に当たる。
//
// 置き場所: --cache → --build の CMakeCache.txt の FETCHCONTENT_BASE_DIR (無ければ <build>/_deps)
//           → 環境変数 ERGO_DEPENDENCY_CACHE_DIR。

#include "commands.h"
#include "file_io.h"
#include "host_context.h"
#include "process.h"

#include <cstdio>
#include <cstdlib>

#ifndef ERGO_PKG_CLI_DEFAULT_CMAKE
#define ERGO_PKG_CLI_DEFAULT_CMAKE "cmake"
#endif

namespace ergo::package_cli {

namespace {

/// CMakeCache.txt の 1 項目 (NAME:TYPE=value)。 無ければ ""。
std::string cache_value(const std::string& cache_text, const std::string& name) {
    const std::string head = name + ":";
    std::size_t begin = 0;
    while (begin < cache_text.size()) {
        std::size_t end = cache_text.find('\n', begin);
        if (end == std::string::npos) end = cache_text.size();
        const std::string line = cache_text.substr(begin, end - begin);
        if (line.rfind(head, 0) == 0) {
            const std::size_t eq = line.find('=');
            std::string v = eq == std::string::npos ? std::string() : line.substr(eq + 1);
            while (!v.empty() && v.back() == '\r') v.pop_back();
            return v;
        }
        begin = end + 1;
    }
    return {};
}

std::string base_dir(const Args& args, std::string* error) {
    if (!args.cache.empty()) return args.cache;
    if (!args.build.empty()) {
        const auto cache = read_file(join(args.build, "CMakeCache.txt"));
        if (!cache) {
            *error = args.build + " is not a configured build directory (no CMakeCache.txt)";
            return {};
        }
        const std::string v = cache_value(*cache, "FETCHCONTENT_BASE_DIR");
        return v.empty() ? join(args.build, "_deps") : v;
    }
    if (const char* env = std::getenv("ERGO_DEPENDENCY_CACHE_DIR"); env && *env) return env;
    *error = "where to fetch is unknown: pass --build <dir> or --cache <dir>";
    return {};
}

/// 取得する名前: 引数、 無ければ host.use。 どれも link パッケージでなければ誤り。
bool select_names(const Args& args, const HostContext& ctx, std::vector<std::string>& out,
                  std::string* error) {
    out = args.names.empty() && ctx.has_declaration ? ctx.declaration.use : args.names;
    if (out.empty()) {
        *error = "nothing to fetch: name packages or list them in host.use";
        return false;
    }
    for (const auto& n : out) {
        const pm::ManifestPackage* p = ctx.ergo_manifest.find(n);
        for (const auto& priv : ctx.declaration.packages) {
            if (priv.name == n) p = &priv;
        }
        if (!p || p->kind != "link") {
            *error = "'" + n + "' is not a link package of the Ergo manifest or the host declaration";
            return false;
        }
    }
    return true;
}

} // namespace

int run_fetch(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg fetch: %s\n", error.c_str());
        return kExitInput;
    }
    if (!ctx.has_ergo_manifest) {
        std::fprintf(stderr, "ergo_pkg fetch: Ergo checkout not found (pass --ergo <dir>)\n");
        return kExitInput;
    }
    std::vector<std::string> names;
    const std::string base = base_dir(args, &error);
    if (base.empty() || !select_names(args, ctx, names, &error)) {
        std::fprintf(stderr, "ergo_pkg fetch: %s\n", error.c_str());
        return kExitInput;
    }

    std::string joined;
    for (const auto& n : names) joined += (joined.empty() ? "" : ",") + n;
    std::vector<std::string> argv = {
        args.cmake.empty() ? std::string(ERGO_PKG_CLI_DEFAULT_CMAKE) : args.cmake,
        "-DERGO_PKG_PREFETCH_NAMES=" + joined,
        "-DFETCHCONTENT_BASE_DIR=" + base,
        "-DERGO_PKG_PREFETCH_MANIFEST=" + join(ctx.ergo_dir, "ergo-packages.json"),
    };
    if (ctx.has_declaration) {
        argv.push_back("-DERGO_PKG_PREFETCH_HOST=" + join(args.host, "ergo-packages.json"));
    }
    argv.push_back("-P");
    argv.push_back(join(ctx.ergo_dir, "cmake/packages/ErgoPackagePrefetch.cmake"));
    for (const auto& a : argv) {
        if (!quote_safe(a)) {
            std::fprintf(stderr, "ergo_pkg fetch: refusing an argument with quotes or %%/$: %s\n", a.c_str());
            return kExitInput;
        }
    }
    if (!run(argv)) {
        std::fprintf(stderr, "ergo_pkg fetch: prefetch failed\n");
        return kExitIo;
    }
    std::printf("ergo_pkg fetch: %zu package(s) ready in %s\n", names.size(), base.c_str());
    return kExitOk;
}

} // namespace ergo::package_cli
