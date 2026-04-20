#include "gtest/gtest.h"

#include <filesystem>

#include "ergo/io/file.h"
#include "ergo/io/path.h"

using namespace ergo::io;
namespace fs = std::filesystem;

namespace {

/// RAII — pick a unique temp dir and nuke it on exit.
struct ScopedTempDir {
    fs::path dir;
    explicit ScopedTempDir(const char* tag) {
        static int counter = 0;
        dir = fs::temp_directory_path() / ("ergo_io_test_" + std::string(tag) +
                                           "_" + std::to_string(++counter));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    ~ScopedTempDir() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    std::string at(const char* sub) const {
        return (dir / sub).string();
    }
};

} // namespace

// -------------------------------------------------------------------------
// read / write round-trip
// -------------------------------------------------------------------------

TEST(IoFile, TextRoundTrip) {
    ScopedTempDir tmp("round");
    const std::string p = tmp.at("a.txt");
    const std::string payload = "hello\nworld\n";
    EXPECT_TRUE(write_file(p, payload));

    std::string got;
    EXPECT_TRUE(read_file(p, got));
    EXPECT_EQ(got, payload);
}

TEST(IoFile, BinaryRoundTripPreservesBytes) {
    ScopedTempDir tmp("binary");
    const std::string p = tmp.at("b.bin");
    std::vector<uint8_t> payload = {0x00, 0x01, 0xFF, 0xFE, 0x0D, 0x0A, 0x00};
    EXPECT_TRUE(write_file_bytes(p, payload));

    std::vector<uint8_t> got;
    EXPECT_TRUE(read_file_bytes(p, got));
    EXPECT_EQ(got.size(), payload.size());
    for (size_t i = 0; i < payload.size(); ++i) EXPECT_EQ(got[i], payload[i]);
}

TEST(IoFile, EmptyContentRoundTrip) {
    ScopedTempDir tmp("empty");
    const std::string p = tmp.at("e.txt");
    EXPECT_TRUE(write_file(p, ""));
    std::string got = "should be cleared";
    EXPECT_TRUE(read_file(p, got));
    EXPECT_TRUE(got.empty());
}

// -------------------------------------------------------------------------
// directory creation
// -------------------------------------------------------------------------

TEST(IoFile, WriteCreatesMissingParents) {
    ScopedTempDir tmp("nested");
    const std::string p = tmp.at("a/b/c/d.txt");
    EXPECT_FALSE(exists(p));
    EXPECT_TRUE (write_file(p, "deep"));
    EXPECT_TRUE (exists(p));

    std::string got;
    EXPECT_TRUE(read_file(p, got));
    EXPECT_EQ(got, "deep");
}

TEST(IoFile, EnsureDirectoryIsIdempotent) {
    ScopedTempDir tmp("ensure");
    const std::string sub = (fs::path(tmp.dir) / "a" / "b").string();
    EXPECT_TRUE(ensure_directory(sub));
    EXPECT_TRUE(ensure_directory(sub));  // second call is fine
    EXPECT_TRUE(is_directory(sub));
}

// -------------------------------------------------------------------------
// queries
// -------------------------------------------------------------------------

TEST(IoFile, MissingFileReadReturnsFalse) {
    ScopedTempDir tmp("missing");
    std::string got;
    EXPECT_FALSE(read_file(tmp.at("nope.txt"), got));
    EXPECT_TRUE(got.empty());
}

TEST(IoFile, IsDirectoryDistinguishesFileFromDir) {
    ScopedTempDir tmp("query");
    const std::string f = tmp.at("file.txt");
    EXPECT_TRUE(write_file(f, "x"));
    EXPECT_FALSE(is_directory(f));
    EXPECT_TRUE (is_directory(tmp.dir.string()));
}

TEST(IoFile, RemoveFileNonExistentSucceeds) {
    ScopedTempDir tmp("rm-miss");
    EXPECT_TRUE(remove_file(tmp.at("nope.txt")));
}

TEST(IoFile, RemoveFileDeletes) {
    ScopedTempDir tmp("rm");
    const std::string p = tmp.at("to-delete.txt");
    EXPECT_TRUE(write_file(p, "bye"));
    EXPECT_TRUE(exists(p));
    EXPECT_TRUE(remove_file(p));
    EXPECT_FALSE(exists(p));
}

// -------------------------------------------------------------------------
// path helpers
// -------------------------------------------------------------------------

TEST(IoPath, ParentOf) {
    EXPECT_EQ(path::parent_of("foo/bar/baz.txt"), "foo/bar");
    EXPECT_EQ(path::parent_of("baz.txt"),         "");
    EXPECT_EQ(path::parent_of(""),                "");
}

TEST(IoPath, JoinAccepts) {
    const std::string j = path::join("foo", "bar.txt");
    EXPECT_TRUE(j == "foo/bar.txt" || j == "foo\\bar.txt");
    EXPECT_FALSE(j.empty());
}

TEST(IoPath, ExtensionAndStem) {
    EXPECT_EQ(path::extension_of("baz.txt"), ".txt");
    EXPECT_EQ(path::extension_of("baz"),     "");
    EXPECT_EQ(path::stem_of("foo/bar.ext"),  "bar");
}
