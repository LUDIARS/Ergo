#pragma once

/// ergo_package_viz — 図のグループ (Tela の表示切り替え単位)。 どの図も 8 以下。
///
/// P1 packages: Ergo / 描画 / 可視化 / ツール / 第三者 / 観測: MUSA / 観測: 関連 / host
/// P2 targets : `.anatomia/layers.json` の 5 層 + 外部パッケージ + 第三者 + 循環
/// P3 usage   : Ergo / ツール / MUSA / 関連 / 共有 TS / 未観測

#include "ergo/graph_layout/layered_layout.h"

#include <vector>

namespace ergo::package_viz {

// P1
inline constexpr const char* kGroupErgo       = "ergo";
inline constexpr const char* kGroupRender     = "render";
inline constexpr const char* kGroupViz        = "viz";
inline constexpr const char* kGroupTool       = "tool";
inline constexpr const char* kGroupThirdParty = "third_party";
inline constexpr const char* kGroupMusa       = "musa";
inline constexpr const char* kGroupRelated    = "related";
inline constexpr const char* kGroupHost       = "host";

// P2 (層の ID は layers.json の値そのもの)
inline constexpr const char* kLayerPresentation   = "presentation";
inline constexpr const char* kLayerDomainLogic    = "domain-logic";
inline constexpr const char* kLayerApplication    = "application";
inline constexpr const char* kLayerInfrastructure = "infrastructure";
inline constexpr const char* kLayerShared         = "shared";
inline constexpr const char* kGroupExternal       = "external";
inline constexpr const char* kGroupCycle          = "cycle";

// P3 (Ergo / ツール / MUSA / 関連は P1 と同じ ID)
inline constexpr const char* kGroupSharedTs   = "shared_ts";
inline constexpr const char* kGroupUnobserved = "unobserved";

/// P1 のグループ (列順)。
std::vector<graph_layout::LayoutGroup> package_groups();

/// P2 のグループ (列順)。 第三者は P1 と同じ ID。
std::vector<graph_layout::LayoutGroup> target_groups();

/// P3 のグループ (列順)。
std::vector<graph_layout::LayoutGroup> usage_groups();

/// 分割の索引図のグループ。
std::vector<graph_layout::LayoutGroup> index_groups();

} // namespace ergo::package_viz
