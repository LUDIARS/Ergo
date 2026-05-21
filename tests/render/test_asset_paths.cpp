#include "gtest/gtest.h"

#include "ergo/render/asset_paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ergo::render;

namespace {

namespace fs = std::filesystem;

/// テスト用の一意な一時ディレクトリを作る。
fs::path make_temp_dir(const std::string& tag) {
    fs::path base = fs::temp_directory_path() /
                    ("ergo_render_test_" + tag + "_" +
                     std::to_string(static_cast<unsigned long long>(
                         reinterpret_cast<std::uintptr_t>(&tag))));
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base;
}

#ifdef _WIN32
void set_env(const char* k, const char* v) { _putenv_s(k, v ? v : ""); }
void unset_env(const char* k)              { _putenv_s(k, ""); }
#else
void set_env(const char* k, const char* v) { setenv(k, v, 1); }
void unset_env(const char* k)              { unsetenv(k); }
#endif

} // namespace

TEST(AssetPaths, EnvVarTakesPrecedenceForShaderDir) {
    set_env("ERGO_RENDER_SHADER_DIR", "/explicit/shader/path");
    const std::string r = resolve_shader_dir("shaders");
    unset_env("ERGO_RENDER_SHADER_DIR");
    // 環境変数の値が正規化されて返る (区切りは generic の '/')。
    EXPECT_NE(r.find("explicit"), std::string::npos);
}

TEST(AssetPaths, EnvVarTakesPrecedenceForAssetRoot) {
    set_env("ERGO_RENDER_ASSET_ROOT", "/explicit/asset/root");
    const std::string r = resolve_asset_root("KzSUnity");
    unset_env("ERGO_RENDER_ASSET_ROOT");
    EXPECT_NE(r.find("asset"), std::string::npos);
}

TEST(AssetPaths, FindsDirInStartDir) {
    unset_env("ERGO_RENDER_SHADER_DIR");
    fs::path root = make_temp_dir("startdir");
    fs::create_directories(root / "shaders");

    const std::string r = resolve_shader_dir("shaders", root.string());
    // 起点直下の shaders/ を見つける。
    EXPECT_NE(r.find("shaders"), std::string::npos);
    EXPECT_TRUE(fs::is_directory(r));

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(AssetPaths, SearchesUpwards) {
    unset_env("ERGO_RENDER_ASSET_ROOT");
    fs::path root = make_temp_dir("upward");
    fs::create_directories(root / "KzSUnity");
    fs::path deep = root / "a" / "b" / "c";
    fs::create_directories(deep);

    // 深い起点からでも上方向に KzSUnity/ を見つける。
    const std::string r = resolve_asset_root("KzSUnity", deep.string(), 5);
    EXPECT_NE(r.find("KzSUnity"), std::string::npos);
    EXPECT_TRUE(fs::is_directory(r));

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(AssetPaths, FallbackWhenNotFound) {
    unset_env("ERGO_RENDER_SHADER_DIR");
    fs::path root = make_temp_dir("fallback");
    // shaders/ をどこにも作らない。

    const std::string r = resolve_shader_dir("shaders", root.string(), 2);
    // 見つからなくても空文字列ではなく、 起点直下のパスを返す。
    EXPECT_FALSE(r.empty());
    EXPECT_NE(r.find("shaders"), std::string::npos);

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(AssetPaths, ResultHasNoTrailingSlash) {
    unset_env("ERGO_RENDER_SHADER_DIR");
    fs::path root = make_temp_dir("trailing");
    fs::create_directories(root / "shaders");

    const std::string r = resolve_shader_dir("shaders", root.string());
    ASSERT_FALSE(r.empty());
    EXPECT_NE(r.back(), '/');

    std::error_code ec;
    fs::remove_all(root, ec);
}
