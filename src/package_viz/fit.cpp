#include "ergo/package_viz/fit.h"

#include "ergo/graph_layout/layered_layout.h"
#include "ergo/package_viz/collapse_focus.h"
#include "ergo/package_viz/package_view.h"
#include "ergo/package_viz/reduction.h"
#include "ergo/package_viz/split.h"
#include "ergo/package_viz/target_view.h"
#include "ergo/tela_graph/writer.h"

#include <set>

namespace ergo::package_viz {

namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (const auto& p : parts) out += (out.empty() ? "" : sep) + p;
    return out;
}

/// 配置して検査を通れば true。
bool place(const graph_layout::LayoutInput& in, const ViewOptions& o, tela_graph::Graph& out,
           std::string* error) {
    out = graph_layout::layout_layered(in, o.layout).graph;
    return tela_graph::validate_tela_graph(out, error);
}

} // namespace

bool fits(const graph_layout::LayoutInput& g, const Limits& limits) {
    std::set<std::string> groups;
    for (const auto& n : g.nodes) groups.insert(n.group_id);
    return groups.size() <= limits.groups && g.nodes.size() <= limits.nodes &&
           g.edges.size() <= limits.edges;
}

bool fit_view(ViewGraph view, const package_model::PackageModel& model, const ViewOptions& o,
              FitResult& result, std::string* error) {
    result = FitResult{};
    if (view.graph.nodes.empty()) return fail(error, "nothing to draw (no nodes)");
    std::vector<std::string> notes = view.notes;

    // 2. 推移簡約
    if (!fits(view.graph, o.limits)) {
        const std::size_t removed = transitive_reduction(view.graph);
        notes.push_back("推移簡約 " + std::to_string(removed) + " 本");
        result.steps.push_back("reduce");
    }
    // 3. 焦点と畳み込み
    const bool asked = !o.focus.empty() || !o.expand.empty();
    if (asked || !fits(view.graph, o.limits)) {
        if (o.view == View::Targets) {
            const auto collapsed =
                collapse_packages(view, model, expanded_packages(model, o.expand));
            if (!collapsed.empty()) {
                notes.push_back("畳み込み " + join(collapsed, ", "));
                result.steps.push_back("collapse");
            }
        }
        if (!o.focus.empty()) {
            std::size_t dropped = 0;
            if (!apply_focus(view, o.focus, &dropped)) {
                return fail(error, "focus matched nothing: " + join(o.focus, ", "));
            }
            notes.push_back("焦点 " + join(o.focus, ", ") + " (" + std::to_string(dropped) +
                            " ノード省略)");
            result.steps.push_back("focus");
        }
    }
    if (!notes.empty()) view.graph.title += " · " + join(notes, " · ");

    tela_graph::Graph single;
    std::string single_error;
    if (fits(view.graph, o.limits) && place(view.graph, o, single, &single_error)) {
        result.graphs.push_back(std::move(single));
        return true;
    }

    // 4. 分割。 配置後の検査で落ちる枚があれば 1 枚のノード数を半分にして分け直す。
    result.steps.push_back("split");
    for (std::size_t limit = o.limits.nodes; limit >= 1; limit /= 2) {
        const auto split = split_graph(view.graph, o.limits, limit);
        std::vector<tela_graph::Graph> parts;
        bool ok = true;
        std::string part_error;
        for (const auto& part : split.parts) {
            tela_graph::Graph placed;
            if (!fits(part, o.limits) || !place(part, o, placed, &part_error)) {
                ok = false;
                break;
            }
            parts.push_back(std::move(placed));
        }
        tela_graph::Graph index;
        if (ok && place(split.index, o, index, &part_error)) {
            result.graphs = std::move(parts);
            result.index  = std::move(index);
            result.split  = true;
            return true;
        }
        if (limit == 1) {
            return fail(error, "cannot fit the view into Tela limits even when split: " + part_error);
        }
    }
    return fail(error, "cannot fit the view into Tela limits");
}

bool make_view(const package_model::PackageModel& model, const ViewOptions& o, FitResult& result,
               std::string* error) {
    if (o.view == View::Targets && !model.has_codemodel) {
        return fail(error, "the targets view needs the CMake File API reply (codemodel v2)");
    }
    ViewGraph view = o.view == View::Packages ? build_packages_view(model, o)
                                              : build_targets_view(model, o);
    if (o.view == View::Packages && !model.has_codemodel) {
        view.notes.push_back("実リンク未取得");
    }
    return fit_view(std::move(view), model, o, result, error);
}

std::vector<std::pair<std::string, const tela_graph::Graph*>> output_files(
    const FitResult& result, const std::string& out) {
    std::vector<std::pair<std::string, const tela_graph::Graph*>> files;
    if (!result.split) {
        if (!result.graphs.empty()) files.emplace_back(out, &result.graphs.front());
        return files;
    }
    std::string stem = out;
    const std::string ext = ".tela";
    if (stem.size() > ext.size() && stem.compare(stem.size() - ext.size(), ext.size(), ext) == 0) {
        stem.resize(stem.size() - ext.size());
    }
    for (std::size_t i = 0; i < result.graphs.size(); ++i) {
        files.emplace_back(stem + "." + std::to_string(i + 1) + ext, &result.graphs[i]);
    }
    files.emplace_back(stem + ".index" + ext, &result.index);
    return files;
}

} // namespace ergo::package_viz
