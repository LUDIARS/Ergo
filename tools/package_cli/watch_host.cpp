#include "watch_host.h"

#include "file_io.h"
#include "host_context.h"

#include "ergo/package_manager/init_plan.h"

#include <filesystem>

namespace ergo::package_cli {

namespace {

/// 宣言に code が無いときはディレクトリ名 (コードとして書ける形のときだけ)。
std::string code_from_dir(const std::string& dir) {
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(std::filesystem::u8path(dir), ec);
    if (ec) p = std::filesystem::u8path(dir);
    const std::string name = p.filename().u8string().empty() ? p.parent_path().filename().u8string()
                                                             : p.filename().u8string();
    return pm::is_valid_code(name) ? name : std::string();
}

bool load_ergo_checkout(const HostContext& ctx, WatchHost& out, std::string* error) {
    jsonm::JsonValue doc;
    pm::ErgoManifest manifest;
    std::string why;
    if (!jsonm::parse(*ctx.declaration_text, doc) || !pm::read_ergo_manifest(doc, manifest, &why)) {
        if (error) *error = out.dir + ": ergo-packages.json is neither a host declaration nor an Ergo manifest";
        return false;
    }
    out.code         = pw::kErgoHostCode;
    out.dependencies = pw::ergo_dependencies(manifest, &out.notes);
    return true;
}

} // namespace

bool load_watch_host(const Args& args, const std::string& dir, WatchHost& out, std::string* error) {
    Args a = args;
    a.host  = dir;
    out.dir = dir;
    HostContext ctx;
    if (!load_host_context(a, ctx, error)) return false;
    if (!ctx.declaration_text) {
        if (error) *error = dir + ": no ergo-packages.json (run `ergo_pkg init`)";
        return false;
    }
    if (!ctx.has_declaration) return load_ergo_checkout(ctx, out, error);

    for (const auto& n : ctx.notes) out.notes.push_back(n);
    out.code = ctx.declaration.code.empty() ? code_from_dir(dir) : ctx.declaration.code;
    if (!pm::is_valid_code(out.code)) {
        if (error) *error = dir + ": host.code is missing or not a project code";
        return false;
    }
    out.dependencies = pw::host_dependencies(ctx.declaration,
                                             ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr, &out.notes);
    return true;
}

} // namespace ergo::package_cli
