#pragma once

/// ergo_render_viz — 描画パイプラインの読み取り専用スナップショット。
///
/// Pictor の `PipelineProfileDef` (パス列・アタッチメント・ポストプロセス) と
/// `Profiler::FrameStats` (段ごとの CPU / GPU 時間) を、 可視化に必要な分だけ
/// 写し取った値型。 Pictor の型を含まないので、 Pictor が無い構成でも
/// レイアウトとファイル書き出し (ergo_tela_graph) はビルド・テストできる。
///
/// 組み立ては `pictor_snapshot.h` の `snapshot_from_pictor()` (pictor ターゲットが
/// あるときだけビルド)、 または呼び出し側が直接詰める。

#include <optional>
#include <string>
#include <vector>

namespace ergo::render_viz {

/// Pictor `PassType` と 1:1 の種別。 可視化での色分け (グループ) に使う。
enum class PassKind {
    DepthOnly,
    Opaque,
    Transparent,
    Shadow,
    PostProcess,
    Compute,
    Custom,
};

/// 種別の表示名 (日本語)。
const char* to_label(PassKind kind);

/// 名前付きアタッチメント (描画先 / 入力テクスチャ)。
struct AttachmentInfo {
    std::string name;
    std::string format;     ///< 表示用の形式名 (例 "R16G16B16A16_SFLOAT")。空なら出さない。
    bool        swapchain = false;
    bool        depth     = false;
};

/// 1 本の render pass。 `targets` に書き、 `inputs` から読む。
struct PassInfo {
    std::string              name;
    PassKind                 kind = PassKind::Opaque;
    std::vector<std::string> targets;
    std::vector<std::string> inputs;
    bool                     host_recorded = false;  ///< Begin/End をホストが記録する
    bool                     gpu_driven    = false;  ///< GPU 駆動パス
    /// このパスの種別に属する全パスの GPU 時間の合計 (ms)。 Pictor の
    /// FrameStats は種別単位でしか測らないので、 パス単体の値ではない。
    std::optional<double>    kind_gpu_ms;
};

/// ポストプロセス・スタックの 1 エフェクト (実行順)。
struct EffectInfo {
    std::string name;
    bool        enabled = true;
};

/// フレームの CPU 段 (DataUpdate → Culling → Sort → BatchBuild → CommandEncode)。
struct StageTiming {
    std::string name;
    double      cpu_ms = 0.0;
};

/// 可視化 1 回分の入力。
struct PipelineSnapshot {
    std::string                 profile_name;
    std::string                 rendering_path;   ///< "Forward+" など。空なら出さない。
    std::vector<AttachmentInfo> attachments;      ///< 宣言済みアタッチメント (宣言順)
    std::vector<PassInfo>       passes;           ///< 実行順
    std::vector<EffectInfo>     effects;          ///< ポストプロセス (実行順)
    std::vector<StageTiming>    cpu_stages;       ///< 空なら CPU 段の行を描かない
    std::optional<double>       frame_ms;
    std::optional<double>       gpu_frame_ms;
};

} // namespace ergo::render_viz
