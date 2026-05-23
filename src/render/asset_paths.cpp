#include "ergo/render/asset_paths.h"

#include <cstdlib>
#include <filesystem>

namespace ergo::render {

namespace {

namespace fs = std::filesystem;

/// `start` を起点に、 そのディレクトリ自身 + 上方向 `max_levels` 階層から
/// `dir_name` という名前のディレクトリを探す。 最初に見つかった実ディレクトリ
/// の絶対・正規化済み generic パスを返す。 見つからなければ空文字列。
std::string search_upwards(const std::string& dir_name,
                           const std::string& start,
                           int                max_levels) {
    std::error_code ec;
    fs::path base = start.empty() ? fs::current_path(ec) : fs::path(start);
    if (ec) return {};

    fs::path cur = base;
    for (int level = 0; level <= max_levels; ++level) {
        fs::path candidate = cur / dir_name;
        if (fs::is_directory(candidate, ec)) {
            fs::path abs = fs::absolute(candidate, ec);
            if (ec) return candidate.lexically_normal().generic_string();
            return abs.lexically_normal().generic_string();
        }
        fs::path parent = cur.parent_path();
        if (parent.empty() || parent == cur) break;   // ルートに到達
        cur = parent;
    }
    return {};
}

/// 共通解決ロジック: 環境変数 → 上方探索 → フォールバック。
std::string resolve(const char*        env_name,
                    const std::string& dir_name,
                    const std::string& start_dir,
                    int                max_levels) {
    std::error_code ec;

    // 1. 環境変数による明示指定。 CI / 任意環境での上書き用。
    if (const char* env = std::getenv(env_name)) {
        fs::path p(env);
        if (!p.empty()) {
            return p.lexically_normal().generic_string();
        }
    }

    // 2. 起点から上方向へ探索。
    std::string found = search_upwards(dir_name, start_dir, max_levels);
    if (!found.empty()) return found;

    // 3. フォールバック: 起点直下のパス (存在保証は無い)。
    fs::path base = start_dir.empty() ? fs::current_path(ec)
                                      : fs::path(start_dir);
    if (ec) base = fs::path(".");
    return (base / dir_name).lexically_normal().generic_string();
}

} // namespace

std::string resolve_shader_dir(const std::string& dir_name,
                               const std::string& start_dir,
                               int                max_levels) {
    return resolve("ERGO_RENDER_SHADER_DIR", dir_name, start_dir, max_levels);
}

std::string resolve_asset_root(const std::string& dir_name,
                               const std::string& start_dir,
                               int                max_levels) {
    return resolve("ERGO_RENDER_ASSET_ROOT", dir_name, start_dir, max_levels);
}

} // namespace ergo::render
