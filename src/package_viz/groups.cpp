#include "ergo/package_viz/groups.h"

namespace ergo::package_viz {

std::vector<graph_layout::LayoutGroup> package_groups() {
    return {
        {kGroupHost,       "host",          {96, 125, 139}},
        {kGroupErgo,       "Ergo",          {66, 133, 244}},
        {kGroupRender,     "描画",          {171, 71, 188}},
        {kGroupViz,        "可視化",        {0, 150, 136}},
        {kGroupTool,       "ツール",        {141, 110, 99}},
        {kGroupThirdParty, "第三者",        {120, 144, 156}},
        {kGroupMusa,       "観測: MUSA",    {245, 166, 35}},
        {kGroupRelated,    "観測: 関連",    {255, 112, 67}},
    };
}

std::vector<graph_layout::LayoutGroup> target_groups() {
    return {
        {kLayerApplication,    "application",    {96, 125, 139}},
        {kLayerPresentation,   "presentation",   {171, 71, 188}},
        {kLayerDomainLogic,    "domain-logic",   {66, 133, 244}},
        {kLayerInfrastructure, "infrastructure", {0, 150, 136}},
        {kLayerShared,         "shared",         {141, 110, 99}},
        {kGroupExternal,       "外部パッケージ", {245, 166, 35}},
        {kGroupThirdParty,     "第三者",         {120, 144, 156}},
        {kGroupCycle,          "循環",           {229, 57, 53}},
    };
}

std::vector<graph_layout::LayoutGroup> usage_groups() {
    return {
        {kGroupErgo,       "Ergo",          {66, 133, 244}},
        {kGroupTool,       "ツール",        {141, 110, 99}},
        {kGroupMusa,       "MUSA",          {245, 166, 35}},
        {kGroupRelated,    "関連",          {255, 112, 67}},
        {kGroupSharedTs,   "共有 TS",       {0, 150, 136}},
        {kGroupUnobserved, "未観測",        {158, 158, 158}},
    };
}

std::vector<graph_layout::LayoutGroup> host_groups() {
    return {
        {kGroupHost,       "host",                {96, 125, 139}},
        {kGroupErgo,       "Ergo",                {66, 133, 244}},
        {kGroupDependency, "依存",                {0, 150, 136}},
        {kGroupUpdate,     "更新: タグ・main",    {245, 166, 35}},
        {kGroupReview,     "更新: 審査中・作業中", {171, 71, 188}},
        {kGroupCheck,      "検査",                {229, 57, 53}},
    };
}

std::vector<graph_layout::LayoutGroup> index_groups() {
    return {{"part", "分割した図", {66, 133, 244}}};
}

} // namespace ergo::package_viz
