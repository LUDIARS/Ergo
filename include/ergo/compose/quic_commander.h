#pragma once

#include "ergo/compose/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace ergo::compose {

/// QUICコマンダー
/// App版でexeとのQUIC双方向プロセス間通信を管理する
/// 予約済みシステムコマンドとカスタムコマンドの登録・発行を行う
class QuicCommander {
public:
    QuicCommander();
    ~QuicCommander();

    /// 接続を開始
    bool connect(const std::string& host, uint16_t port);

    /// 接続を切断
    void disconnect();

    /// 接続状態を確認
    bool isConnected() const;

    /// コマンドを送信
    bool sendCommand(const QuicCommand& command);

    /// コマンドハンドラを登録
    /// @param commandId コマンドID（SystemCommand:: またはカスタム）
    /// @param handler コールバック
    void registerHandler(uint32_t commandId, CommandHandler handler);

    /// コマンドハンドラを解除
    void unregisterHandler(uint32_t commandId);

    /// カスタムコマンドIDを発行
    /// @return SystemCommand::CustomBase 以降の一意ID
    uint32_t allocateCustomCommandId(const std::string& name);

    /// コマンド名からIDを取得
    uint32_t getCommandId(const std::string& name) const;

    /// IDからコマンド名を取得
    std::string getCommandName(uint32_t commandId) const;

    /// 登録済みコマンド一覧を取得
    std::vector<std::pair<uint32_t, std::string>> getRegisteredCommands() const;

    // ------- 便利メソッド（予約済みコマンドのラッパー） -------

    /// シーン再生
    bool scenePlay(const std::string& sceneName);

    /// シーン停止
    bool sceneStop();

    /// プロファイル開始
    bool profileStart();

    /// プロファイル停止
    bool profileStop();

    /// テスト実行
    bool testRun(const std::string& testName);

    /// ホットリロード通知
    bool notifyHotReload(const std::string& actorName);

    /// 受信ポーリング（受信コマンドをハンドラにディスパッチ）
    void poll();

private:
    bool connected_ = false;
    std::string host_;
    uint16_t port_ = 0;
    uint32_t nextCustomId_ = SystemCommand::CustomBase;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, CommandHandler> handlers_;
    std::unordered_map<std::string, uint32_t> nameToId_;
    std::unordered_map<uint32_t, std::string> idToName_;

    /// システムコマンドの名前を初期登録
    void registerSystemCommands();
};

} // namespace ergo::compose
