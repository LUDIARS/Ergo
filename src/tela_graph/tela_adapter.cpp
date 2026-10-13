#include "ergo/tela_graph/tela_adapter.h"

#include "ergo/tela_graph/writer.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ergo::tela_graph {

tela::Graph to_tela_graph(const Graph& g) {
    std::string error;
    if (!validate_tela_graph(g, &error)) {
        throw std::invalid_argument("ergo_tela_graph: " + error);
    }

    std::vector<tela::GraphGroup> groups;
    groups.reserve(g.groups.size());
    for (const auto& gr : g.groups) {
        groups.push_back({gr.id, gr.name, gr.visible,
                          tela::Color{gr.color.r, gr.color.g, gr.color.b, 255}});
    }
    std::vector<tela::GraphNode> nodes;
    nodes.reserve(g.nodes.size());
    for (const auto& n : g.nodes) {
        nodes.push_back({n.group_id, n.id, n.label, tela::Rect{n.x, n.y, n.width, n.height}});
    }
    std::vector<tela::GraphEdge> edges;
    edges.reserve(g.edges.size());
    for (const auto& e : g.edges) {
        tela::GraphEdge edge{e.id, e.from, e.to, e.dashed, {}};
        edge.points.reserve(e.points.size());
        for (const auto& p : e.points) edge.points.push_back({p.x, p.y});
        edges.push_back(std::move(edge));
    }
    return tela::Graph(tela::GraphInfo{g.project, g.title, g.width, g.height},
                       std::move(groups), std::move(nodes), std::move(edges));
}

} // namespace ergo::tela_graph
