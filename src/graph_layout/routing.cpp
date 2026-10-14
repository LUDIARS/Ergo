#include "layout_internal.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace ergo::graph_layout::detail {

namespace {

/// 区間 [top, top + height] を count 等分した j 番目の境 (端は使わない)。
float spread(float top, float height, std::size_t j, std::size_t count) {
    return top + height * static_cast<float>(j + 1) / static_cast<float>(count + 1);
}

/// ノードの左右の辺に並べる端点。 相手の列内の順 (同順は辺の入力順) で上から割り当てる。
struct Ports {
    std::map<std::pair<std::size_t, std::size_t>, float> out;   ///< (枠, 辺) → y (右辺)
    std::map<std::pair<std::size_t, std::size_t>, float> in;    ///< (枠, 辺) → y (左辺)
};

Ports assign_ports(const Chains& c) {
    std::map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>> right, left;
    for (std::size_t e = 0; e < c.by_edge.size(); ++e) {
        const auto& chain = c.by_edge[e];
        const std::size_t first = chain.front(), second = chain[1];
        const std::size_t last = chain.back(), before = chain[chain.size() - 2];
        right[first].emplace_back(c.slots[second].position, e);
        left[last].emplace_back(c.slots[before].position, e);
    }
    Ports ports;
    for (auto& [slot, list] : right) {
        std::sort(list.begin(), list.end());
        const auto& s = c.slots[slot];
        for (std::size_t j = 0; j < list.size(); ++j) {
            ports.out[{slot, list[j].second}] = spread(s.y, s.height, j, list.size());
        }
    }
    for (auto& [slot, list] : left) {
        std::sort(list.begin(), list.end());
        const auto& s = c.slots[slot];
        for (std::size_t j = 0; j < list.size(); ++j) {
            ports.in[{slot, list[j].second}] = spread(s.y, s.height, j, list.size());
        }
    }
    return ports;
}

/// 辺ごとの枠の y (左端は出口、 途中は通り道の中央、 右端は入口)。
std::vector<std::vector<float>> chain_heights(const Chains& c, const Ports& ports) {
    std::vector<std::vector<float>> ys(c.by_edge.size());
    for (std::size_t e = 0; e < c.by_edge.size(); ++e) {
        const auto& chain = c.by_edge[e];
        auto& y = ys[e];
        y.reserve(chain.size());
        for (std::size_t i = 0; i < chain.size(); ++i) {
            const auto& s = c.slots[chain[i]];
            if (i == 0) {
                y.push_back(ports.out.at({chain[i], e}));
            } else if (i + 1 == chain.size()) {
                y.push_back(ports.in.at({chain[i], e}));
            } else {
                y.push_back(s.y + s.height * 0.5f);
            }
        }
    }
    return ys;
}

float quarter(float v) { return std::round(v * 4.0f) / 4.0f; }

} // namespace

std::vector<std::vector<tela_graph::Point>> route_edges(
    const Chains& c, const std::vector<DagEdge>& edges,
    const std::vector<float>& column_x, const LayoutOptions& o) {
    const Ports ports = assign_ports(c);
    const auto  ys    = chain_heights(c, ports);

    // 列間の通路ごとに、 通る区間へ縦の通り筋 (lane) を割り当てる。
    // 並びは (入る高さ, 出る高さ, 辺の入力順) — 交差を減らし、 決定的に保つ。
    std::map<std::size_t, std::vector<std::tuple<float, float, std::size_t>>> gaps;
    for (std::size_t e = 0; e < c.by_edge.size(); ++e) {
        const auto& chain = c.by_edge[e];
        for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
            gaps[c.slots[chain[i]].column].emplace_back(ys[e][i], ys[e][i + 1], e);
        }
    }
    std::map<std::pair<std::size_t, std::size_t>, float> lane_x;   // (通路, 辺) → x
    for (auto& [gap, list] : gaps) {
        std::sort(list.begin(), list.end());
        const float left = column_x[gap] + o.node_width;
        for (std::size_t j = 0; j < list.size(); ++j) {
            lane_x[{gap, std::get<2>(list[j])}] = spread(left, o.column_gap, j, list.size());
        }
    }

    std::vector<std::vector<tela_graph::Point>> routes(c.by_edge.size());
    for (std::size_t e = 0; e < c.by_edge.size(); ++e) {
        const auto& chain = c.by_edge[e];
        auto& pts = routes[e];
        const std::size_t first_col = c.slots[chain.front()].column;
        const std::size_t last_col  = c.slots[chain.back()].column;
        pts.push_back({column_x[first_col] + o.node_width, ys[e].front()});
        for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
            const float x = lane_x.at({c.slots[chain[i]].column, e});
            pts.push_back({x, ys[e][i]});
            pts.push_back({x, ys[e][i + 1]});
        }
        pts.push_back({column_x[last_col], ys[e].back()});
        if (edges[e].reversed) std::reverse(pts.begin(), pts.end());
    }
    return routes;
}

void finish_route(std::vector<tela_graph::Point>& points) {
    std::vector<tela_graph::Point> out;
    out.reserve(points.size());
    for (auto p : points) {
        p.x = quarter(p.x);
        p.y = quarter(p.y);
        if (!out.empty() && out.back().x == p.x && out.back().y == p.y) continue;
        if (out.size() >= 2) {
            const auto& a = out[out.size() - 2];
            const auto& b = out.back();
            const bool same_x = a.x == b.x && b.x == p.x;
            const bool same_y = a.y == b.y && b.y == p.y;
            if (same_x || same_y) {
                out.back() = p;   // 一直線上の中間点を除く
                // 置き換えで直前と重なったら落とす (折り返し)
                if (out.size() >= 2 && out[out.size() - 2].x == p.x &&
                    out[out.size() - 2].y == p.y) {
                    out.pop_back();
                }
                continue;
            }
        }
        out.push_back(p);
    }
    points = std::move(out);
}

} // namespace ergo::graph_layout::detail
