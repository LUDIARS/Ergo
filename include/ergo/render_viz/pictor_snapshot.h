#pragma once

/// ergo_render_viz — Pictor のパイプライン定義と計測値からスナップショットを作る。
///
/// `pictor` ターゲットがあるときだけビルドされる (`ERGO_RENDER_VIZ_HAS_PICTOR`)。
/// Pictor の公開型 (`PipelineProfileDef` / `FrameStats`) だけを読み、 GPU API の
/// 型には触れない。
///
/// 使い方 (ホスト側、 例: 1 秒ごとに書き出して Tela で見る):
///   auto snap  = ergo::render_viz::snapshot_from_pictor(
///                    renderer.profile_manager().current_profile(),
///                    &renderer.profiler().get_frame_stats());
///   auto graph = ergo::render_viz::layout_pipeline(snap);
///   ergo::tela_graph::save_tela_graph("pipeline.tela", graph);

#include "ergo/render_viz/pipeline_snapshot.h"

namespace pictor {
struct PipelineProfileDef;
struct FrameStats;
} // namespace pictor

namespace ergo::render_viz {

/// `profile` のパス列・アタッチメント・ポストプロセスを写す。 `stats` を渡すと
/// CPU 段の時間とパス種別ごとの GPU 時間を添える (nullptr なら構造だけ)。
/// `profile.attachments` が空なら Pictor の既定アタッチメント
/// (`pictor::default_attachments()`) を宣言済みとして扱う (Pictor の実行時と同じ)。
PipelineSnapshot snapshot_from_pictor(const pictor::PipelineProfileDef& profile,
                                      const pictor::FrameStats*         stats = nullptr);

} // namespace ergo::render_viz
