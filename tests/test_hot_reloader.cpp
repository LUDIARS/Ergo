#include <gtest/gtest.h>
#include "ergo/compose/hot_reloader.h"

#include <filesystem>
#include <fstream>
#include <thread>

namespace ergo::compose::test {

class HotReloaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "ergo_hotreload_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        reloader_.stop();
        std::filesystem::remove_all(testDir_);
    }

    std::string createTsFile(const std::string& name, const std::string& content) {
        auto path = (testDir_ / name).string();
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

    std::filesystem::path testDir_;
    HotReloader reloader_;
};

TEST_F(HotReloaderTest, InitialState) {
    EXPECT_FALSE(reloader_.isRunning());
    EXPECT_EQ(reloader_.getState(), HotReloadState::Idle);
}

TEST_F(HotReloaderTest, StartStop) {
    reloader_.start(testDir_.string());
    EXPECT_TRUE(reloader_.isRunning());

    reloader_.stop();
    EXPECT_FALSE(reloader_.isRunning());
}

TEST_F(HotReloaderTest, AddRemoveWatchTarget) {
    auto path = createTsFile("player.ts", "export class Player {}");

    reloader_.addWatchTarget(path, "Player");
    reloader_.removeWatchTarget(path);
    // 例外なく完了すること
}

TEST_F(HotReloaderTest, RecordEdit) {
    auto path = createTsFile("logic.ts", "// logic");

    reloader_.addWatchTarget(path, "Actor");
    reloader_.recordEdit(path);
    reloader_.recordEdit(path);
    reloader_.recordEdit(path);
    // 編集回数が記録されること（投機的コンパイル判定に使用）
}

TEST_F(HotReloaderTest, SpeculativeConfig) {
    SpeculativeCompileConfig config;
    config.idleThreshold = std::chrono::seconds(10);
    config.editFrequencyThreshold = 2;
    config.enabled = true;

    reloader_.setSpeculativeConfig(config);
    // 設定が例外なく適用されること
}

TEST_F(HotReloaderTest, SpeculativeCandidates_Empty) {
    // 監視対象なし
    auto candidates = reloader_.getSpeculativeCandidates();
    EXPECT_TRUE(candidates.empty());
}

TEST_F(HotReloaderTest, SpeculativeCandidates_FrequentEditExcluded) {
    auto path = createTsFile("frequent.ts", "// edited often");

    SpeculativeCompileConfig config;
    config.idleThreshold = std::chrono::seconds(0); // 即座にアイドル判定
    config.editFrequencyThreshold = 2;
    config.enabled = true;
    reloader_.setSpeculativeConfig(config);

    reloader_.addWatchTarget(path, "FrequentActor");

    // 閾値を超える回数の編集
    reloader_.recordEdit(path);
    reloader_.recordEdit(path);
    reloader_.recordEdit(path);

    auto candidates = reloader_.getSpeculativeCandidates();
    // 編集頻度が高いので対象外
    EXPECT_TRUE(candidates.empty());
}

TEST_F(HotReloaderTest, GetChangedFiles_InitiallyEmpty) {
    auto changed = reloader_.getChangedFiles();
    EXPECT_TRUE(changed.empty());
}

TEST_F(HotReloaderTest, Callback_Set) {
    bool callbackCalled = false;
    reloader_.setCallback([&](const std::string&, HotReloadState) {
        callbackCalled = true;
    });
    // コールバック設定が例外なく完了すること
}

TEST_F(HotReloaderTest, DetectFileChange) {
    auto path = createTsFile("watch_target.ts", "// original");

    reloader_.addWatchTarget(path, "TestActor");

    bool changeDetected = false;
    reloader_.setCallback([&](const std::string& actorName, HotReloadState state) {
        if (state == HotReloadState::Detected && actorName == "TestActor") {
            changeDetected = true;
        }
    });

    reloader_.start(testDir_.string());

    // ファイルを変更
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    {
        std::ofstream ofs(path);
        ofs << "// modified content";
    }

    // 変更検出を待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    reloader_.stop();

    EXPECT_TRUE(changeDetected);
}

TEST_F(HotReloaderTest, RunSpeculativeCompile_DisabledConfig) {
    SpeculativeCompileConfig config;
    config.enabled = false;
    reloader_.setSpeculativeConfig(config);

    // 無効時は何もしない
    reloader_.runSpeculativeCompile();
}

} // namespace ergo::compose::test
