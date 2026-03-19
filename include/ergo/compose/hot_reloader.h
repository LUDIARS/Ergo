#pragma once

#include "ergo/compose/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>

namespace ergo::compose {

/// ホットリローダー
/// ファイル変更を監視し、変更されたロジックの再結合をトリガーする
/// 投機的C++変換もここで管理する
class HotReloader {
public:
    HotReloader();
    ~HotReloader();

    /// 監視を開始
    void start(const std::string& watchPath);

    /// 監視を停止
    void stop();

    /// 監視中かどうか
    bool isRunning() const;

    /// 監視対象ファイルを追加
    void addWatchTarget(const std::string& filePath, const std::string& actorName);

    /// 監視対象ファイルを除外
    void removeWatchTarget(const std::string& filePath);

    /// ホットリロードコールバックを設定
    void setCallback(HotReloadCallback callback);

    /// 現在の状態を取得
    HotReloadState getState() const;

    /// 変更されたファイル一覧を取得（前回チェック以降）
    std::vector<std::string> getChangedFiles();

    // ------- 投機的コンパイル -------

    /// 投機的コンパイル設定を適用
    void setSpeculativeConfig(const SpeculativeCompileConfig& config);

    /// 投機的コンパイル対象を取得
    /// アイドル時間閾値を超え、編集頻度が低いTSファイルを返す
    std::vector<TypeScriptSource> getSpeculativeCandidates() const;

    /// 投機的コンパイルを実行
    void runSpeculativeCompile();

    /// ファイルの編集頻度を記録
    void recordEdit(const std::string& filePath);

private:
    std::atomic<bool> running_{false};
    std::atomic<HotReloadState> state_{HotReloadState::Idle};
    std::string watchPath_;
    HotReloadCallback callback_;
    SpeculativeCompileConfig specConfig_;

    mutable std::mutex mutex_;
    std::thread watchThread_;

    /// ファイルパス → (アクター名, 差分情報)
    std::unordered_map<std::string, std::pair<std::string, FileDiffInfo>> watchTargets_;

    /// ファイルパス → 編集回数
    std::unordered_map<std::string, uint32_t> editCounts_;

    /// 変更検出済みファイル
    std::vector<std::string> changedFiles_;

    /// ファイル監視ループ（内部スレッド）
    void watchLoop();
};

} // namespace ergo::compose
