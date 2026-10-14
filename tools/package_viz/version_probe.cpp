#include "version_probe.h"

#include "ergo/package_model/file_api_reader.h"
#include "ergo/package_model/version_label.h"

#include <cstdio>
#include <string>

namespace ergo::package_viz::cli {

namespace {

#if defined(_WIN32)
constexpr const char* kNullRedirect = " 2>nul";
#define ERGO_POPEN _popen
#define ERGO_PCLOSE _pclose
#else
constexpr const char* kNullRedirect = " 2>/dev/null";
#define ERGO_POPEN popen
#define ERGO_PCLOSE pclose
#endif

/// コマンドの標準出力。 終了コードが 0 でなければ false。
bool run(const std::string& command, std::string& out) {
#if defined(_WIN32)
    // cmd /c は先頭と末尾の引用符を剥がすので全体をもう 1 組で包む。
    const std::string line = "\"" + command + kNullRedirect + "\"";
#else
    const std::string line = command + kNullRedirect;
#endif
    FILE* pipe = ERGO_POPEN(line.c_str(), "r");
    if (!pipe) return false;
    out.clear();
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    return ERGO_PCLOSE(pipe) == 0;
}

/// 引用符で包んでも壊れないパスだけを渡す。
bool safe_path(const std::string& dir) {
    if (dir.empty()) return false;
    for (const char c : dir) {
        if (c == '"' || c == '\n' || c == '\r' || c == '%' || c == '`' || c == '$') return false;
    }
    return true;
}

package_model::VersionLabel probe(const std::string& dir) {
    std::string out;
    if (run("revisor version show --repo \"" + dir + "\"", out)) {
        const std::string v = package_model::version_from_revisor(out);
        if (!v.empty()) return {v, "revisor"};
    }
    if (run("git -C \"" + dir + "\" describe --tags --long", out)) {
        const std::string v = package_model::version_from_describe(out);
        if (!v.empty()) return {v, "git"};
    }
    return {};
}

} // namespace

void probe_versions(package_model::PackageModel& model) {
    for (auto& p : model.packages) {
        std::string dir;
        if (p.kind == package_model::PackageKind::Link) {
            dir = p.source_dir;
        } else if (p.kind == package_model::PackageKind::Tool && !model.ergo_root.empty() &&
                   !p.path.empty() && p.initialized) {
            dir = package_model::normalize_path(p.path, model.ergo_root);
        }
        if (!safe_path(dir)) continue;
        const auto v = probe(dir);
        if (!v.text.empty()) p.version = v;
    }
}

} // namespace ergo::package_viz::cli
