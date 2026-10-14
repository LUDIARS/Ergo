#include "ergo/graph_layout/layered_layout.h"

#include "layout_internal.h"

#include <algorithm>
#include <map>
#include <set>

namespace ergo::graph_layout {

std::size_t label_line_count(const std::string& label) {
    std::size_t lines = 1;
    for (std::size_t pos = label.find(" | "); pos != std::string::npos;
         pos = label.find(" | ", pos + 3)) {
        ++lines;
    }
    return lines;
}

float node_height_for(const std::string& label, const LayoutOptions& o) {
    return o.node_padding * 2.0f + o.line_height * static_cast<float>(label_line_count(label));
}

namespace {

/// 余白込みの最大寸法がこれを超えたら全体を縮める (Tela の上限 ±100000 に余裕を持たせる)。
constexpr float kMaxExtent = 99000.0f;

void scale_graph(tela_graph::Graph& g, std::vector<std::vector<tela_graph::Point>>& routes,
                 float factor) {
    for (auto& n : g.nodes) {
        n.x *= factor;
        n.y *= factor;
        n.width *= factor;
        n.height *= factor;
    }
    for (auto& r : routes) {
        for (auto& p : r) {
            p.x *= factor;
            p.y *= factor;
        }
    }
    g.width *= factor;
    g.height *= factor;
}

} // namespace

LayoutResult layout_layered(const LayoutInput& in, const LayoutOptions& o) {
    LayoutResult result;
    auto& g   = result.graph;
    g.project = in.project;
    g.title   = in.title;

    // ノード添字 (同じ ID が重なれば最初のもの。 重複は tela_graph の検査が拒む)。
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < in.nodes.size(); ++i) index.emplace(in.nodes[i].id, i);

    std::vector<detail::DagEdge> edges;
    edges.reserve(in.edges.size());
    for (const auto& e : in.edges) {
        const auto f = index.find(e.from);
        const auto t = index.find(e.to);
        if (f == index.end() || t == index.end() || f->second == t->second) {
            ++result.dropped_edges;
            continue;
        }
        detail::DagEdge d;
        d.from   = f->second;
        d.to     = t->second;
        d.dashed = e.dashed;
        edges.push_back(d);
    }

    const auto rank = detail::id_ranks(in.nodes);
    detail::orient_edges(in.nodes.size(), rank, edges);
    const auto column = detail::longest_path_columns(in.nodes.size(), rank, edges);
    std::size_t columns = 0;
    for (const auto c : column) columns = std::max(columns, c + 1);
    result.columns = columns;

    auto chains = detail::order_columns(column, rank, edges, columns);

    // 列の x と、 列内の順に積んだ y。
    std::vector<float> column_x(columns);
    for (std::size_t c = 0; c < columns; ++c) {
        column_x[c] = o.margin + static_cast<float>(c) * (o.node_width + o.column_gap);
    }
    float bottom = o.margin;
    for (const auto& list : chains.by_column) {
        float y = o.margin;
        bool  prev_dummy = false;
        for (std::size_t i = 0; i < list.size(); ++i) {
            auto& s = chains.slots[list[i]];
            if (i > 0) y += (prev_dummy || s.dummy) ? o.row_gap * 0.5f : o.row_gap;
            s.height = s.dummy ? o.dummy_height : node_height_for(in.nodes[s.node].label, o);
            s.y      = y;
            y += s.height;
            prev_dummy = s.dummy;
        }
        bottom = std::max(bottom, y);
    }

    std::set<std::string> used_groups;
    g.nodes.reserve(in.nodes.size());
    for (std::size_t n = 0; n < in.nodes.size(); ++n) {
        const auto& s = chains.slots[n];   // ノードの枠は添字 = ノード添字
        g.nodes.push_back({in.nodes[n].group_id, in.nodes[n].id, in.nodes[n].label,
                           column_x[s.column], s.y, o.node_width, s.height});
        used_groups.insert(in.nodes[n].group_id);
    }
    for (const auto& gr : in.groups) {
        if (used_groups.count(gr.id)) g.groups.push_back({gr.id, gr.name, gr.visible, gr.color});
    }
    g.width  = (columns == 0 ? o.margin : column_x[columns - 1] + o.node_width) + o.margin;
    g.height = bottom + o.margin;

    auto routes = detail::route_edges(chains, edges, column_x, o);
    const float extent = std::max(g.width, g.height);
    if (extent > kMaxExtent) scale_graph(g, routes, kMaxExtent / extent);

    g.edges.reserve(edges.size());
    for (std::size_t e = 0; e < edges.size(); ++e) {
        tela_graph::Edge out;
        out.id     = "e" + std::to_string(e);
        out.from   = in.nodes[edges[e].from].id;
        out.to     = in.nodes[edges[e].to].id;
        out.dashed = edges[e].dashed;
        out.points = std::move(routes[e]);
        detail::finish_route(out.points);
        g.edges.push_back(std::move(out));
    }
    return result;
}

} // namespace ergo::graph_layout
