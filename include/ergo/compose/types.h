#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace ergo::compose {

// =====================================================================
// ビルドモード
// =====================================================================
enum class BuildMode : uint8_t {
    Web,    // WebGL + Wasm + TS/JS
    App     // exe + QUIC IPC
};

// =====================================================================
// ビルド単位
// =====================================================================
enum class BuildUnit : uint8_t {
    Actor,  // アクター単位ビルド
    Scene   // シーン単位ビルド（プレイ確認用）
};

// =====================================================================
// トランスパイル状態
// =====================================================================
enum class TranspileStatus : uint8_t {
    Pending,        // 未変換
    TypeScript,     // TSのまま実行中
    Compiling,      // C++変換中
    Compiled,       // C++変換済み
    Failed          // 変換失敗
};

// =====================================================================
// ホットリロード状態
// =====================================================================
enum class HotReloadState : uint8_t {
    Idle,           // 監視中・変更なし
    Detected,       // 変更検出
    Reloading,      // リロード中
    Ready           // リロード完了
};

// =====================================================================
// QUICコマンドID（予約済みシステムコマンド）
// =====================================================================
namespace SystemCommand {
    constexpr uint32_t ScenePlay        = 0x0001;
    constexpr uint32_t SceneStop        = 0x0002;
    constexpr uint32_t ScenePause       = 0x0003;
    constexpr uint32_t SceneResume      = 0x0004;
    constexpr uint32_t ActorSpawn       = 0x0010;
    constexpr uint32_t ActorDestroy     = 0x0011;
    constexpr uint32_t ActorInspect     = 0x0012;
    constexpr uint32_t HotReload        = 0x0020;
    constexpr uint32_t ProfileStart     = 0x0030;
    constexpr uint32_t ProfileStop      = 0x0031;
    constexpr uint32_t ProfileSnapshot  = 0x0032;
    constexpr uint32_t TestRun          = 0x0040;
    constexpr uint32_t TestResult       = 0x0041;
    constexpr uint32_t Ping             = 0x00F0;
    constexpr uint32_t Pong             = 0x00F1;
    constexpr uint32_t Shutdown         = 0x00FF;

    // カスタムコマンドIDの開始値
    constexpr uint32_t CustomBase       = 0x1000;
}

// =====================================================================
// モジュール結合領域
// =====================================================================
enum class BindingDomain : uint8_t {
    Logic,      // ゲームロジック（Ars/TS由来）
    Input,      // 入力モジュール（Ergo）
    Physics,    // 物理モジュール（Ergo）
    Audio,      // 音声モジュール（Ergo）
    Render,     // 描画（Pictor）
    Network,    // ネットワーク（Ergo）
    Custom      // ユーザー定義
};

// =====================================================================
// 構造体定義
// =====================================================================

/// CRC/バスト数によるファイル差分情報
struct FileDiffInfo {
    uint32_t    crc32       = 0;
    uint32_t    bustCount   = 0;    // 変更回数
    std::string sourcePath;
    std::string compiledPath;
    std::chrono::steady_clock::time_point lastModified;
    std::chrono::steady_clock::time_point lastCompiled;
};

/// TypeScript ソースファイル情報
struct TypeScriptSource {
    std::string filePath;
    std::string moduleName;
    std::string actorName;
    TranspileStatus status = TranspileStatus::Pending;
    FileDiffInfo diffInfo;
};

/// モジュールバインディング（結合情報）
struct ModuleBinding {
    std::string         moduleName;
    BindingDomain       domain;
    std::string         headerPath;
    std::string         libraryPath;
    std::vector<std::string> symbols;   // エクスポートされるシンボル
};

/// Pictor描画バインディング
struct RenderBinding {
    std::string     meshId;
    std::string     materialId;
    uint16_t        flags = 0;          // ObjectDescriptor flags
    uint8_t         lodLevel = 0;
};

/// アクター結合定義
struct ActorComposition {
    std::string                     actorName;
    std::vector<TypeScriptSource>   logicSources;       // TSロジック
    std::vector<ModuleBinding>      moduleBindings;     // Ergoモジュール
    RenderBinding                   renderBinding;      // Pictor描画
    std::vector<std::string>        dependencies;       // 他アクターへの依存
};

/// シーン定義
struct SceneDefinition {
    std::string                     sceneName;
    std::vector<std::string>        actorNames;     // 含まれるアクター名
    BuildMode                       targetMode;
};

/// QUICコマンド
struct QuicCommand {
    uint32_t    commandId;
    std::string name;
    std::vector<uint8_t> payload;
};

/// ビルド結果
struct BuildResult {
    bool        success = false;
    std::string outputPath;
    std::string errorMessage;
    std::vector<std::string> warnings;
    std::chrono::milliseconds buildTime{0};
};

/// 投機的コンパイル設定
struct SpeculativeCompileConfig {
    std::chrono::seconds    idleThreshold{300};     // アイドル判定閾値（デフォルト5分）
    uint32_t                editFrequencyThreshold = 3; // この回数以下の編集頻度なら対象
    bool                    enabled = true;
};

/// Composer設定
struct ComposeConfig {
    BuildMode                   defaultMode = BuildMode::App;
    bool                        hotReloadEnabled = true;
    SpeculativeCompileConfig    speculativeCompile;
    std::string                 typeDefPath;            // .d.ts ディレクトリ
    std::string                 outputPath;             // 出力ディレクトリ
    uint16_t                    quicPort = 4433;        // QUIC通信ポート
};

/// コマンドハンドラ型
using CommandHandler = std::function<void(const QuicCommand&)>;

/// ホットリロードコールバック型
using HotReloadCallback = std::function<void(const std::string& actorName, HotReloadState state)>;

} // namespace ergo::compose
