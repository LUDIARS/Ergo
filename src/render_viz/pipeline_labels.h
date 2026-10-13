#pragma once

// ergo_render_viz 内部 — ノードと図の見出しの文言を組み立てる。
// 配置 (pipeline_layout.cpp) と分け、 文言の変更が座標計算に波及しないようにする。

#include "ergo/render_viz/pipeline_snapshot.h"

#include <cstddef>
#include <string>

namespace ergo::render_viz::detail {

/// ms を小数 2 桁で書く (ロケールに依存しない)。
std::string format_ms(double ms);

/// 図の見出し: プロファイル名・描画方式・フレーム時間。
std::string title_label(const PipelineSnapshot& snapshot);

/// パスの文言。 `heaviest` はこのパスの種別が GPU 時間最大の種別であること。
std::string pass_label(const PassInfo& pass, bool heaviest);

/// アタッチメントの文言。 `declared` が false なら未宣言と添える。
std::string attachment_label(const AttachmentInfo& attachment, bool declared);

std::string effect_label(const EffectInfo& effect);

std::string stage_label(const StageTiming& stage);

} // namespace ergo::render_viz::detail
