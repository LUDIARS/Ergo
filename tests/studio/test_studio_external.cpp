#include "ergo/studio/host/external_loader.h"
#include "ergo/studio/host/health_report.h"
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/plugin_abi.h"

#include <gtest/gtest.h>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace ergo::studio;
using namespace ergo::studio::host;
namespace fs = std::filesystem;

namespace {

const LogFn kQuiet = [](const std::string&, const std::string&) {};

/// The built echo pack: ctest passes its path in ERGO_TEST_ECHO_PACK (CMakeLists.txt).
fs::path echo_pack_path() {
#ifdef _WIN32
    std::wstring value(32768, wchar_t{});
    const DWORD n = GetEnvironmentVariableW(L"ERGO_TEST_ECHO_PACK", value.data(), static_cast<DWORD>(value.size()));
    if (n == 0 || n >= value.size()) throw std::runtime_error("ERGO_TEST_ECHO_PACK is not set (run the test through ctest)");
    value.resize(n);
    return fs::path(value);
#else
    const char* value = std::getenv("ERGO_TEST_ECHO_PACK");
    if (value == nullptr) throw std::runtime_error("ERGO_TEST_ECHO_PACK is not set (run the test through ctest)");
    return fs::u8path(value);
#endif
}

/// Fresh pack directory under the build tree's temp dir, removed on exit.
class TempPack {
public:
    explicit TempPack(const std::string& tag) {
        static std::atomic<int> n{0};
        root_ = fs::temp_directory_path() / ("ergo_studio_pack_" + tag + "_" + std::to_string(++n));
        fs::remove_all(root_);
        fs::create_directories(root_);
    }
    ~TempPack() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    const fs::path& root() const { return root_; }
    /// <root>/<name>/ERGO_STUDIO_PLUGIN_FILE, copied from the built echo pack.
    void add_echo(const std::string& name) const {
        fs::create_directories(root_ / name);
        fs::copy_file(echo_pack_path(), root_ / name / ERGO_STUDIO_PLUGIN_FILE);
    }

private:
    fs::path root_;
};

class FakeConn final : public net::IWsConnection {
public:
    uint64_t id() const override { return 7; }
    void     send_text(const std::string& p) override { sent.push_back(p); }
    void     close(uint16_t code) override { closed = code; }
    bool     is_open() const override { return closed == 0; }
    std::vector<std::string> sent;
    uint16_t                 closed = 0;
};

} // namespace

TEST(StudioExternal, SplitsPluginDirList) {
#ifdef _WIN32
    const auto dirs = split_plugin_dirs(" a ;;b; ");
#else
    const auto dirs = split_plugin_dirs(" a ::b: ");
#endif
    ASSERT_EQ(dirs.size(), 2u);
    EXPECT_EQ(dirs[0].u8string(), "a");
    EXPECT_EQ(dirs[1].u8string(), "b");
    EXPECT_TRUE(split_plugin_dirs("").empty());
}

TEST(StudioExternal, FindsEntriesInNameOrderAndIgnoresOthers) {
    TempPack pack("find");
    pack.add_echo("b");
    pack.add_echo("a");
    fs::create_directories(pack.root() / "empty");
    const auto entries = find_pack_entries(pack.root(), kQuiet);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].parent_path().filename().u8string(), "a");
    EXPECT_EQ(entries[1].parent_path().filename().u8string(), "b");
    EXPECT_TRUE(find_pack_entries(pack.root() / "missing", kQuiet).empty());
}

TEST(StudioExternal, CPackServesHttpWsAndHealth) {
    TempPack pack("serve");
    pack.add_echo("echo");
    auto factories = load_external_factories(pack.root().u8string(), kQuiet);
    ASSERT_EQ(factories.size(), 1u);
    PluginHost host(instantiate_plugins({}, factories, kQuiet), kQuiet);
    ASSERT_EQ(host.plugins().size(), 1u);
    EXPECT_EQ(host.plugins()[0]->info().title, "Echo");

    net::HttpRequest req;
    req.method = "POST";
    req.target = "/echo/api/x?k=v";
    req.path   = "/echo/api/x";
    req.body   = "hi";
    const auto res = host.handle(req);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "POST /api/x?k=v hi");
    EXPECT_EQ(res.headers.at("content-type"), "text/plain");

    ASSERT_TRUE(host.accepts("echo"));
    auto conn = std::make_shared<FakeConn>();
    auto h    = host.accept("echo", req, conn);
    ASSERT_TRUE(static_cast<bool>(h.on_text));
    h.on_text("ping");
    ASSERT_EQ(conn->sent.size(), 1u);
    EXPECT_EQ(conn->sent[0], "echo:ping");

    const auto health = plugin_health(*host.plugins()[0]);
    EXPECT_TRUE(health.find("ok")->as_bool());
    EXPECT_EQ(health.find("clients")->as_number(), 1.0);
    h.on_close();
    EXPECT_EQ(plugin_health(*host.plugins()[0]).find("clients")->as_number(), 0.0);
}

TEST(StudioExternal, SecondPackWithSameIdIsRejected) {
    TempPack pack("dup");
    pack.add_echo("one");
    pack.add_echo("two");
    const auto r = instantiate_plugins({}, load_external_factories(pack.root().u8string(), kQuiet), kQuiet);
    ASSERT_EQ(r.plugins.size(), 1u);
    ASSERT_EQ(r.rejected.size(), 1u);
    EXPECT_EQ(r.rejected[0].id, "echo");
    EXPECT_NE(r.rejected[0].source.find("two"), std::string::npos);
}

TEST(StudioExternal, NonLibraryIsSkipped) {
    TempPack pack("bad");
    fs::create_directories(pack.root() / "junk");
    { std::FILE* f = std::fopen((pack.root() / "junk" / ERGO_STUDIO_PLUGIN_FILE).u8string().c_str(), "wb");
      ASSERT_TRUE(f != nullptr);
      std::fputs("not a library", f);
      std::fclose(f); }
    std::vector<std::string> errors;
    const LogFn log = [&](const std::string& lv, const std::string& m) { if (lv == "error") errors.push_back(m); };
    EXPECT_TRUE(load_external_factories(pack.root().u8string(), log).empty());
    EXPECT_EQ(errors.size(), 1u);
}
