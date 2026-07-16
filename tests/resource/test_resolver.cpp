#include "ergo/resource/resolver.h"

#include <filesystem>
#include <string>

#include "ergo/io/file.h"
#include "ergo/resource/loader.h"
#include "gtest/gtest.h"

using ergo::resource::Catalog;
using ergo::resource::Entry;
using ergo::resource::LoadedResource;

namespace fs = std::filesystem;

namespace {

Entry make_entry(std::string id, std::string name,
                 std::vector<std::string> tags, std::string path = "") {
    Entry e;
    e.id   = std::move(id);
    e.name = std::move(name);
    e.tags = std::move(tags);
    e.path = std::move(path);
    return e;
}

Catalog make_catalog() {
    Catalog c;
    c.add(make_entry("tree01", "tree01", {"tree", "nature"}));
    c.add(make_entry("tree02", "tree02", {"tree"}));
    c.add(make_entry("rock01", "rock01", {"rock", "nature"}));
    return c;
}

} // namespace

TEST(Resolver, RanksByTagOverlap) {
    const Catalog catalog = make_catalog();

    const auto candidates =
        ergo::resource::resolve_candidates(catalog, {"tree", "nature"}, "");
    ASSERT_EQ(candidates.size(), 3u);  // all share at least one tag
    EXPECT_EQ(candidates[0].entry->id, "tree01");
    EXPECT_GT(candidates[0].score, candidates[1].score);
}

TEST(Resolver, NameHintBreaksTagTies) {
    const Catalog catalog = make_catalog();

    const auto* best =
        ergo::resource::resolve_best(catalog, {"tree"}, "tree02");
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->id, "tree02");
}

TEST(Resolver, NoMatchYieldsEmpty) {
    const Catalog catalog = make_catalog();

    EXPECT_EQ(ergo::resource::resolve_best(catalog, {"spaceship"}, ""),
              nullptr);
    EXPECT_TRUE(
        ergo::resource::resolve_candidates(catalog, {"spaceship"}, "")
            .empty());
}

TEST(Resolver, MaxCandidatesTruncates) {
    const Catalog catalog = make_catalog();

    const auto candidates = ergo::resource::resolve_candidates(
        catalog, {"nature"}, "", /*max_candidates=*/1);
    EXPECT_EQ(candidates.size(), 1u);
}

TEST(Loader, ReadsResolvedFileBytes) {
    const fs::path root =
        fs::temp_directory_path() / "ergo_resource_loader";
    fs::create_directories(root);
    const std::string payload = "mesh-bytes";
    const std::string file    = (root / "tree01.bin").string();
    ASSERT_TRUE(ergo::io::write_file(file, payload));

    Catalog catalog;
    catalog.add(make_entry("tree01", "tree01", {"tree"}, file));

    LoadedResource loaded;
    ASSERT_TRUE(
        ergo::resource::resolve_and_load(catalog, {"tree"}, "", loaded));
    EXPECT_EQ(loaded.entry.id, "tree01");
    ASSERT_EQ(loaded.bytes.size(), payload.size());
    EXPECT_EQ(std::string(loaded.bytes.begin(), loaded.bytes.end()), payload);

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(Loader, MissingFileFails) {
    Catalog catalog;
    catalog.add(make_entry("ghost", "ghost", {"tree"}, "Z:/nope/ghost.bin"));

    LoadedResource loaded;
    EXPECT_FALSE(
        ergo::resource::resolve_and_load(catalog, {"tree"}, "", loaded));
    EXPECT_TRUE(loaded.bytes.empty());
}

TEST(Loader, NoMatchFails) {
    const Catalog catalog = make_catalog();

    LoadedResource loaded;
    EXPECT_FALSE(
        ergo::resource::resolve_and_load(catalog, {"spaceship"}, "", loaded));
}
