#include "ergo/resource/loader.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ergo/io/file.h"
#include "ergo/resource/catalog.h"
#include "gtest/gtest.h"

using ergo::resource::Catalog;
using ergo::resource::Entry;
using ergo::resource::LoadedResource;

namespace fs = std::filesystem;

namespace {

/// Writes files into a per-test temp dir, removed on exit. Paths come from
/// std::filesystem's own temp directory (not a hardcoded drive path), so
/// this is meaningful on every OS the tests run on.
class TempDir {
 public:
    explicit TempDir(const std::string& tag)
        : root_(fs::temp_directory_path() / ("ergo_resource_loader_" + tag)) {
        fs::create_directories(root_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    // NOTE: mini-gtest の EXPECT/ASSERT は非 void 関数内で使えない
    //       (`return;` を展開するため)。失敗は ok() に集約して TEST 側で確認する。
    std::string file(const std::string& rel, const std::string& content) {
        const std::string path = (root_ / rel).string();
        if (!ergo::io::write_file(path, content)) all_writes_ok_ = false;
        return path;
    }
    std::string file(const std::string& rel, const std::vector<uint8_t>& bytes) {
        const std::string path = (root_ / rel).string();
        if (!ergo::io::write_file_bytes(path, bytes)) all_writes_ok_ = false;
        return path;
    }
    bool ok() const { return all_writes_ok_; }
    /// A path under this dir that is never written — guaranteed absent.
    std::string missing_path(const std::string& rel) const {
        return (root_ / rel).string();
    }

 private:
    fs::path root_;
    bool all_writes_ok_ = true;
};

Entry make_entry(std::string id, std::string path,
                 std::vector<std::string> tags = {}) {
    Entry e;
    e.id   = id;
    e.name = id;
    e.path = std::move(path);
    e.tags = std::move(tags);
    return e;
}

} // namespace

// ---- load() -------------------------------------------------------------

TEST(Loader, LoadReadsFileBytesAndCopiesEntry) {
    TempDir dir("basic");
    const std::string payload = "mesh-bytes";
    const std::string file = dir.file("mesh.bin", payload);
    ASSERT_TRUE(dir.ok());

    const Entry entry = make_entry("tree01", file, {"tree"});
    LoadedResource out;
    ASSERT_TRUE(ergo::resource::load(entry, out));
    EXPECT_EQ(out.entry.id, "tree01");
    ASSERT_EQ(out.bytes.size(), payload.size());
    EXPECT_EQ(std::string(out.bytes.begin(), out.bytes.end()), payload);
}

TEST(Loader, LoadPreservesBinaryContentIncludingEmbeddedNulls) {
    TempDir dir("binary");
    const std::vector<uint8_t> payload = {0x00, 0x01, 0xFF, 0x00, 0x7F};
    const std::string file = dir.file("mesh.bin", payload);
    ASSERT_TRUE(dir.ok());

    const Entry entry = make_entry("bin01", file);
    LoadedResource out;
    ASSERT_TRUE(ergo::resource::load(entry, out));
    EXPECT_EQ(out.bytes, payload);
}

TEST(Loader, LoadFailsAndClearsOutWhenFileIsMissing) {
    TempDir dir("missing");
    const std::string missing = dir.missing_path("does_not_exist.bin");

    const Entry entry = make_entry("ghost", missing);
    LoadedResource out;
    out.bytes = {1, 2, 3};  // pre-existing garbage must be cleared on failure
    EXPECT_FALSE(ergo::resource::load(entry, out));
    EXPECT_TRUE(out.bytes.empty());
    EXPECT_TRUE(out.entry.id.empty());
}

TEST(Loader, LoadFailsForEmptyPath) {
    const Entry entry = make_entry("no-path", "");
    LoadedResource out;
    EXPECT_FALSE(ergo::resource::load(entry, out));
    EXPECT_TRUE(out.bytes.empty());
}

// ---- resolve_and_load() ---------------------------------------------------

TEST(Loader, ResolveAndLoadFindsBestMatchAndReadsIt) {
    TempDir dir("resolve");
    const std::string payload = "tree-mesh";
    const std::string file = dir.file("tree01.bin", payload);
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    catalog.add(make_entry("tree01", file, {"tree", "nature"}));
    catalog.add(make_entry("rock01", dir.missing_path("rock01.bin"), {"rock"}));

    LoadedResource out;
    ASSERT_TRUE(ergo::resource::resolve_and_load(catalog, {"tree"}, "", out));
    EXPECT_EQ(out.entry.id, "tree01");
    EXPECT_EQ(std::string(out.bytes.begin(), out.bytes.end()), payload);
}

TEST(Loader, ResolveAndLoadFailsWhenNothingMatchesTags) {
    Catalog catalog;
    catalog.add(make_entry("tree01", "unused", {"tree"}));

    LoadedResource out;
    EXPECT_FALSE(
        ergo::resource::resolve_and_load(catalog, {"spaceship"}, "", out));
    EXPECT_TRUE(out.bytes.empty());
}

TEST(Loader, ResolveAndLoadFailsWhenMatchedFileIsUnreadable) {
    TempDir dir("resolve-unreadable");
    Catalog catalog;
    catalog.add(make_entry("ghost", dir.missing_path("ghost.bin"), {"tree"}));

    LoadedResource out;
    out.bytes = {9, 9, 9};
    EXPECT_FALSE(ergo::resource::resolve_and_load(catalog, {"tree"}, "", out));
    EXPECT_TRUE(out.bytes.empty());
}