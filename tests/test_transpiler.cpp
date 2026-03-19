#include <gtest/gtest.h>
#include "ergo/compose/transpiler.h"

#include <fstream>
#include <filesystem>

namespace ergo::compose::test {

class TranspilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "ergo_compose_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    std::string createTestFile(const std::string& name, const std::string& content) {
        auto path = (testDir_ / name).string();
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }

    std::filesystem::path testDir_;
    Transpiler transpiler_;
};

TEST_F(TranspilerTest, CRC32_Calculation) {
    auto path = createTestFile("test.ts", "const x: number = 42;");
    uint32_t crc = Transpiler::calculateCrc32(path);
    EXPECT_NE(crc, 0u);

    // 同一ファイルは同一CRC
    uint32_t crc2 = Transpiler::calculateCrc32(path);
    EXPECT_EQ(crc, crc2);
}

TEST_F(TranspilerTest, CRC32_DifferentContent) {
    auto path1 = createTestFile("a.ts", "const a = 1;");
    auto path2 = createTestFile("b.ts", "const b = 2;");

    uint32_t crc1 = Transpiler::calculateCrc32(path1);
    uint32_t crc2 = Transpiler::calculateCrc32(path2);
    EXPECT_NE(crc1, crc2);
}

TEST_F(TranspilerTest, CRC32_NonexistentFile) {
    uint32_t crc = Transpiler::calculateCrc32("/nonexistent/file.ts");
    EXPECT_EQ(crc, 0u);
}

TEST_F(TranspilerTest, Transpile_SourceNotFound) {
    TypeScriptSource source;
    source.filePath = "/nonexistent/actor.ts";
    source.actorName = "TestActor";

    auto result = transpiler_.transpile(source);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(source.status, TranspileStatus::Failed);
}

TEST_F(TranspilerTest, Transpile_Success) {
    auto tsPath = createTestFile("player.ts",
        "export class Player {\n"
        "    hp: number = 100;\n"
        "    move(dx: number, dy: number): void {}\n"
        "}\n");

    TypeScriptSource source;
    source.filePath = tsPath;
    source.moduleName = "game";
    source.actorName = "Player";

    auto result = transpiler_.transpile(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(source.status, TranspileStatus::Compiled);
    EXPECT_FALSE(result.outputPath.empty());
    EXPECT_TRUE(std::filesystem::exists(result.outputPath));
}

TEST_F(TranspilerTest, Transpile_UpdatesBustCount) {
    auto tsPath = createTestFile("actor.ts", "export class Actor {}");

    TypeScriptSource source;
    source.filePath = tsPath;
    source.moduleName = "test";
    source.actorName = "Actor";

    transpiler_.transpile(source);
    EXPECT_EQ(source.diffInfo.bustCount, 1u);

    // 再度変換（CRC変更なしなのでスキップ）
    // ソース変更をシミュレート
    createTestFile("actor.ts", "export class Actor { x: number = 0; }");
    source.diffInfo.compiledPath = ""; // コンパイル済みパスをリセット
    transpiler_.transpile(source);
    EXPECT_EQ(source.diffInfo.bustCount, 2u);
}

TEST_F(TranspilerTest, DiffHeader_WriteAndRead) {
    auto cppPath = createTestFile("output.h", "#pragma once\n// content\n");

    FileDiffInfo info;
    info.crc32 = 0xDEADBEEF;
    info.bustCount = 5;
    info.compiledPath = cppPath;

    Transpiler::writeDiffHeader(cppPath, info);

    auto readInfo = Transpiler::readDiffHeader(cppPath);
    ASSERT_TRUE(readInfo.has_value());
    EXPECT_EQ(readInfo->crc32, 0xDEADBEEF);
    EXPECT_EQ(readInfo->bustCount, 5u);
}

TEST_F(TranspilerTest, DiffHeader_ReadInvalidFile) {
    auto path = createTestFile("plain.h", "#pragma once\n");
    auto info = Transpiler::readDiffHeader(path);
    EXPECT_FALSE(info.has_value());
}

TEST_F(TranspilerTest, CheckDiff_DetectsChange) {
    auto tsPath = createTestFile("logic.ts", "const x = 1;");

    FileDiffInfo diffInfo;
    diffInfo.sourcePath = tsPath;
    diffInfo.crc32 = 0; // 初期値と異なるはず

    EXPECT_TRUE(transpiler_.checkDiff(diffInfo));
    // CRCが更新される
    EXPECT_NE(diffInfo.crc32, 0u);
}

TEST_F(TranspilerTest, CheckDiff_NoChange) {
    auto tsPath = createTestFile("logic.ts", "const x = 1;");
    uint32_t crc = Transpiler::calculateCrc32(tsPath);

    FileDiffInfo diffInfo;
    diffInfo.sourcePath = tsPath;
    diffInfo.crc32 = crc;

    EXPECT_FALSE(transpiler_.checkDiff(diffInfo));
}

TEST_F(TranspilerTest, BatchTranspile) {
    auto path1 = createTestFile("a.ts", "export class A {}");
    auto path2 = createTestFile("b.ts", "export class B {}");

    std::vector<TypeScriptSource> sources(2);
    sources[0].filePath = path1;
    sources[0].moduleName = "mod";
    sources[0].actorName = "A";
    sources[1].filePath = path2;
    sources[1].moduleName = "mod";
    sources[1].actorName = "B";

    auto results = transpiler_.transpileBatch(sources);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].success);
    EXPECT_TRUE(results[1].success);
}

} // namespace ergo::compose::test
