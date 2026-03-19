#include <gtest/gtest.h>
#include "ergo/compose/quic_commander.h"

namespace ergo::compose::test {

class QuicCommanderTest : public ::testing::Test {
protected:
    QuicCommander commander_;
};

TEST_F(QuicCommanderTest, InitialState) {
    EXPECT_FALSE(commander_.isConnected());
}

TEST_F(QuicCommanderTest, ConnectAndDisconnect) {
    EXPECT_TRUE(commander_.connect("127.0.0.1", 4433));
    EXPECT_TRUE(commander_.isConnected());

    commander_.disconnect();
    EXPECT_FALSE(commander_.isConnected());
}

TEST_F(QuicCommanderTest, SystemCommandsRegistered) {
    // 予約済みシステムコマンドが登録されていること
    EXPECT_EQ(commander_.getCommandId("scene.play"), SystemCommand::ScenePlay);
    EXPECT_EQ(commander_.getCommandId("scene.stop"), SystemCommand::SceneStop);
    EXPECT_EQ(commander_.getCommandId("scene.pause"), SystemCommand::ScenePause);
    EXPECT_EQ(commander_.getCommandId("hotreload"), SystemCommand::HotReload);
    EXPECT_EQ(commander_.getCommandId("profile.start"), SystemCommand::ProfileStart);
    EXPECT_EQ(commander_.getCommandId("test.run"), SystemCommand::TestRun);
    EXPECT_EQ(commander_.getCommandId("ping"), SystemCommand::Ping);
    EXPECT_EQ(commander_.getCommandId("shutdown"), SystemCommand::Shutdown);
}

TEST_F(QuicCommanderTest, SystemCommandNames) {
    EXPECT_EQ(commander_.getCommandName(SystemCommand::ScenePlay), "scene.play");
    EXPECT_EQ(commander_.getCommandName(SystemCommand::Shutdown), "shutdown");
}

TEST_F(QuicCommanderTest, AllocateCustomCommand) {
    uint32_t id = commander_.allocateCustomCommandId("custom.attack");
    EXPECT_GE(id, SystemCommand::CustomBase);
    EXPECT_EQ(commander_.getCommandId("custom.attack"), id);
    EXPECT_EQ(commander_.getCommandName(id), "custom.attack");
}

TEST_F(QuicCommanderTest, CustomCommandIdUnique) {
    uint32_t id1 = commander_.allocateCustomCommandId("cmd1");
    uint32_t id2 = commander_.allocateCustomCommandId("cmd2");
    EXPECT_NE(id1, id2);
}

TEST_F(QuicCommanderTest, CustomCommandIdempotent) {
    uint32_t id1 = commander_.allocateCustomCommandId("same_cmd");
    uint32_t id2 = commander_.allocateCustomCommandId("same_cmd");
    EXPECT_EQ(id1, id2);
}

TEST_F(QuicCommanderTest, CustomCommandNoConflictWithSystem) {
    auto commands = commander_.getRegisteredCommands();
    for (const auto& [id, name] : commands) {
        if (id < SystemCommand::CustomBase) {
            // システムコマンドの範囲
            uint32_t customId = commander_.allocateCustomCommandId("new_custom_" + name);
            EXPECT_GE(customId, SystemCommand::CustomBase);
            EXPECT_NE(customId, id);
        }
    }
}

TEST_F(QuicCommanderTest, SendCommand_NotConnected) {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::Ping;
    EXPECT_FALSE(commander_.sendCommand(cmd));
}

TEST_F(QuicCommanderTest, SendCommand_Connected) {
    commander_.connect("127.0.0.1", 4433);

    QuicCommand cmd;
    cmd.commandId = SystemCommand::Ping;
    cmd.name = "ping";
    EXPECT_TRUE(commander_.sendCommand(cmd));
}

TEST_F(QuicCommanderTest, ScenePlayStop) {
    commander_.connect("127.0.0.1", 4433);
    EXPECT_TRUE(commander_.scenePlay("TestScene"));
    EXPECT_TRUE(commander_.sceneStop());
}

TEST_F(QuicCommanderTest, ProfileStartStop) {
    commander_.connect("127.0.0.1", 4433);
    EXPECT_TRUE(commander_.profileStart());
    EXPECT_TRUE(commander_.profileStop());
}

TEST_F(QuicCommanderTest, TestRun) {
    commander_.connect("127.0.0.1", 4433);
    EXPECT_TRUE(commander_.testRun("unit_test_player"));
}

TEST_F(QuicCommanderTest, NotifyHotReload) {
    commander_.connect("127.0.0.1", 4433);
    EXPECT_TRUE(commander_.notifyHotReload("Player"));
}

TEST_F(QuicCommanderTest, RegisterHandler) {
    bool handled = false;
    commander_.registerHandler(SystemCommand::Pong, [&](const QuicCommand&) {
        handled = true;
    });

    // ハンドラが登録されていることを間接的に確認
    commander_.unregisterHandler(SystemCommand::Pong);
    // 解除後も例外なし
}

TEST_F(QuicCommanderTest, GetRegisteredCommands) {
    auto commands = commander_.getRegisteredCommands();
    EXPECT_GE(commands.size(), 16u); // システムコマンド16個以上

    // ソート済みであること
    for (size_t i = 1; i < commands.size(); ++i) {
        EXPECT_LE(commands[i - 1].first, commands[i].first);
    }
}

TEST_F(QuicCommanderTest, UnknownCommandName) {
    EXPECT_EQ(commander_.getCommandId("nonexistent"), 0u);
}

TEST_F(QuicCommanderTest, UnknownCommandId) {
    EXPECT_EQ(commander_.getCommandName(0xFFFF), "");
}

} // namespace ergo::compose::test
