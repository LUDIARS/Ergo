#include "ergo/resource/catalog.h"

#include <filesystem>
#include <string>

#include "ergo/io/file.h"
#include "gtest/gtest.h"

using ergo::resource::Catalog;

namespace fs = std::filesystem;

namespace {

/// Writes manifest/resource files into a per-test temp dir, removed on exit.
class TempDir {
 public:
    explicit TempDir(const std::string& tag)
        : root_(fs::temp_directory_path() / ("ergo_resource_" + tag)) {
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
    bool ok() const { return all_writes_ok_; }
    std::string path(const std::string& rel) const {
        return (root_ / rel).lexically_normal().string();
    }

 private:
    fs::path root_;
    bool all_writes_ok_ = true;
};

constexpr const char* kManifest = R"({
  "resources": [
    {"id": "tree01", "name": "tree01", "path": "models/tree01.glb",
     "tags": ["tree", "nature"]},
    {"path": "models/rock.glb", "tags": ["rock"]},
    {"path": "C:/abs/boulder.glb"}
  ]
})";

} // namespace

TEST(Catalog, LoadsEntriesWithDefaults) {
    TempDir dir("load");
    const auto manifest = dir.file("resources.json", kManifest);
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    ASSERT_TRUE(catalog.load_manifest(manifest));
    ASSERT_EQ(catalog.entries().size(), 3u);
    EXPECT_TRUE(catalog.last_error().empty());

    // explicit fields kept
    const auto* tree = catalog.find_by_id("tree01");
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->name, "tree01");
    EXPECT_EQ(tree->tags.size(), 2u);
    EXPECT_EQ(tree->path, dir.path("models/tree01.glb"));

    // id defaults to raw path, name to filename stem
    const auto* rock = catalog.find_by_id("models/rock.glb");
    ASSERT_NE(rock, nullptr);
    EXPECT_EQ(rock->name, "rock");

    // absolute paths pass through untouched
    const auto* boulder = catalog.find_by_id("C:/abs/boulder.glb");
    ASSERT_NE(boulder, nullptr);
    EXPECT_EQ(boulder->path, "C:/abs/boulder.glb");
}

TEST(Catalog, VocabularyIsSortedUnique) {
    TempDir dir("vocab");
    const auto manifest = dir.file("resources.json", kManifest);
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    ASSERT_TRUE(catalog.load_manifest(manifest));

    const auto vocab = catalog.vocabulary();
    ASSERT_EQ(vocab.size(), 3u);
    EXPECT_EQ(vocab[0], "nature");
    EXPECT_EQ(vocab[1], "rock");
    EXPECT_EQ(vocab[2], "tree");
}

TEST(Catalog, MissingFileFailsWithReason) {
    Catalog catalog;
    EXPECT_FALSE(catalog.load_manifest("Z:/nope/resources.json"));
    EXPECT_FALSE(catalog.last_error().empty());
    EXPECT_TRUE(catalog.entries().empty());
}

TEST(Catalog, MalformedJsonFailsWithReason) {
    TempDir dir("bad");
    const auto manifest = dir.file("resources.json", "{not json");
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    EXPECT_FALSE(catalog.load_manifest(manifest));
    EXPECT_FALSE(catalog.last_error().empty());
}

TEST(Catalog, EntriesWithoutPathAreSkippedButLoadSucceeds) {
    TempDir dir("skip");
    const auto manifest = dir.file(
        "resources.json",
        R"({"resources": [{"tags": ["orphan"]}, {"path": "a.bin"}]})");
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    ASSERT_TRUE(catalog.load_manifest(manifest));
    EXPECT_EQ(catalog.entries().size(), 1u);
    EXPECT_FALSE(catalog.last_error().empty());  // skip is observable
}

TEST(Catalog, LoadManifestAppendsAcrossCalls) {
    TempDir dir("append");
    const auto first  = dir.file("a.json", R"({"resources":[{"path":"a.bin"}]})");
    const auto second = dir.file("b.json", R"({"resources":[{"path":"b.bin"}]})");
    ASSERT_TRUE(dir.ok());

    Catalog catalog;
    ASSERT_TRUE(catalog.load_manifest(first));
    ASSERT_TRUE(catalog.load_manifest(second));
    EXPECT_EQ(catalog.entries().size(), 2u);

    catalog.clear();
    EXPECT_TRUE(catalog.entries().empty());
}
