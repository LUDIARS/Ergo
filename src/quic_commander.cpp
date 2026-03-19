#include "ergo/compose/quic_commander.h"

#include <algorithm>

namespace ergo::compose {

QuicCommander::QuicCommander() {
    registerSystemCommands();
}

QuicCommander::~QuicCommander() {
    disconnect();
}

void QuicCommander::registerSystemCommands() {
    nameToId_["scene.play"]         = SystemCommand::ScenePlay;
    nameToId_["scene.stop"]         = SystemCommand::SceneStop;
    nameToId_["scene.pause"]        = SystemCommand::ScenePause;
    nameToId_["scene.resume"]       = SystemCommand::SceneResume;
    nameToId_["actor.spawn"]        = SystemCommand::ActorSpawn;
    nameToId_["actor.destroy"]      = SystemCommand::ActorDestroy;
    nameToId_["actor.inspect"]      = SystemCommand::ActorInspect;
    nameToId_["hotreload"]          = SystemCommand::HotReload;
    nameToId_["profile.start"]      = SystemCommand::ProfileStart;
    nameToId_["profile.stop"]       = SystemCommand::ProfileStop;
    nameToId_["profile.snapshot"]   = SystemCommand::ProfileSnapshot;
    nameToId_["test.run"]           = SystemCommand::TestRun;
    nameToId_["test.result"]        = SystemCommand::TestResult;
    nameToId_["ping"]               = SystemCommand::Ping;
    nameToId_["pong"]               = SystemCommand::Pong;
    nameToId_["shutdown"]           = SystemCommand::Shutdown;

    // 逆引きマップ
    for (const auto& [name, id] : nameToId_) {
        idToName_[id] = name;
    }
}

bool QuicCommander::connect(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);

    // QUIC接続の確立（スタブ実装）
    // 実際のQUIC実装はサブモジュールが担当
    host_ = host;
    port_ = port;
    connected_ = true;

    return true;
}

void QuicCommander::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
}

bool QuicCommander::isConnected() const {
    return connected_;
}

bool QuicCommander::sendCommand(const QuicCommand& command) {
    if (!connected_) return false;

    // QUICストリーム経由でコマンドを送信（スタブ）
    // 実際の送信はQUICサブモジュールが担当
    return true;
}

void QuicCommander::registerHandler(uint32_t commandId, CommandHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[commandId] = std::move(handler);
}

void QuicCommander::unregisterHandler(uint32_t commandId) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.erase(commandId);
}

uint32_t QuicCommander::allocateCustomCommandId(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 既に登録済みならそのIDを返す
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        return it->second;
    }

    uint32_t id = nextCustomId_++;
    nameToId_[name] = id;
    idToName_[id] = name;
    return id;
}

uint32_t QuicCommander::getCommandId(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToId_.find(name);
    return (it != nameToId_.end()) ? it->second : 0;
}

std::string QuicCommander::getCommandName(uint32_t commandId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = idToName_.find(commandId);
    return (it != idToName_.end()) ? it->second : "";
}

std::vector<std::pair<uint32_t, std::string>> QuicCommander::getRegisteredCommands() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<uint32_t, std::string>> commands;
    commands.reserve(idToName_.size());
    for (const auto& [id, name] : idToName_) {
        commands.emplace_back(id, name);
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

bool QuicCommander::scenePlay(const std::string& sceneName) {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::ScenePlay;
    cmd.name = "scene.play";
    cmd.payload.assign(sceneName.begin(), sceneName.end());
    return sendCommand(cmd);
}

bool QuicCommander::sceneStop() {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::SceneStop;
    cmd.name = "scene.stop";
    return sendCommand(cmd);
}

bool QuicCommander::profileStart() {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::ProfileStart;
    cmd.name = "profile.start";
    return sendCommand(cmd);
}

bool QuicCommander::profileStop() {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::ProfileStop;
    cmd.name = "profile.stop";
    return sendCommand(cmd);
}

bool QuicCommander::testRun(const std::string& testName) {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::TestRun;
    cmd.name = "test.run";
    cmd.payload.assign(testName.begin(), testName.end());
    return sendCommand(cmd);
}

bool QuicCommander::notifyHotReload(const std::string& actorName) {
    QuicCommand cmd;
    cmd.commandId = SystemCommand::HotReload;
    cmd.name = "hotreload";
    cmd.payload.assign(actorName.begin(), actorName.end());
    return sendCommand(cmd);
}

void QuicCommander::poll() {
    // 受信キューからコマンドを取り出しハンドラにディスパッチ（スタブ）
    // 実際のQUIC受信はサブモジュールが担当
}

} // namespace ergo::compose
