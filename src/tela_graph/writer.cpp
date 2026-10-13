#include "ergo/tela_graph/writer.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace ergo::tela_graph {

namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool valid_id(const std::string& id) {
    return !id.empty() && id.size() <= kTelaMaxIdBytes;
}

bool valid_coord(float v) {
    return std::isfinite(v) && std::fabs(v) <= kTelaMaxCoord;
}

bool valid_size(float v) {
    return valid_coord(v) && v > 0.0f;
}

/// Tela の遷移ファイルと同じ引用符エスケープ ( " と \ の前に \ )。
/// 制御文字は 1 行 1 レコードを壊すので空白にする。
std::string quote(const std::string& value) {
    std::string out = "\"";
    for (const char c : value) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (u < 0x20 || u == 0x7f) {
            out += ' ';
        } else {
            out += c;
        }
    }
    out += '"';
    return out;
}

/// 小数 2 桁まで。 末尾の 0 と小数点を落とす (ロケールに依存しない)。
std::string number(float v) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(2) << static_cast<double>(v);
    std::string s = out.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

} // namespace

bool validate_tela_graph(const Graph& g, std::string* error) {
    if (!valid_size(g.width) || !valid_size(g.height)) {
        return fail(error, "graph size must be positive and finite");
    }
    if (g.groups.empty())                return fail(error, "no groups");
    if (g.nodes.empty())                 return fail(error, "no nodes");
    if (g.groups.size() > kTelaMaxGroups) return fail(error, "too many groups (max 8)");
    if (g.nodes.size()  > kTelaMaxNodes)  return fail(error, "too many nodes (max 128)");
    if (g.edges.size()  > kTelaMaxEdges)  return fail(error, "too many edges (max 256)");

    std::unordered_set<std::string> groups;
    for (const auto& gr : g.groups) {
        if (!valid_id(gr.id))            return fail(error, "invalid group id: " + gr.id);
        if (!groups.insert(gr.id).second) return fail(error, "duplicate group id: " + gr.id);
    }
    std::unordered_set<std::string> nodes;
    for (const auto& n : g.nodes) {
        if (!valid_id(n.id))              return fail(error, "invalid node id: " + n.id);
        if (!nodes.insert(n.id).second)   return fail(error, "duplicate node id: " + n.id);
        if (!groups.count(n.group_id))    return fail(error, "node in unknown group: " + n.id);
        if (!valid_coord(n.x) || !valid_coord(n.y) ||
            !valid_size(n.width) || !valid_size(n.height)) {
            return fail(error, "invalid node bounds: " + n.id);
        }
    }
    std::unordered_set<std::string> edges;
    std::size_t points = 0;
    for (const auto& e : g.edges) {
        if (!valid_id(e.id))              return fail(error, "invalid edge id: " + e.id);
        if (!edges.insert(e.id).second)   return fail(error, "duplicate edge id: " + e.id);
        if (!nodes.count(e.from) || !nodes.count(e.to)) {
            return fail(error, "edge with unknown end: " + e.id);
        }
        if (e.points.size() < 2)          return fail(error, "edge needs two points: " + e.id);
        for (const auto& p : e.points) {
            if (!valid_coord(p.x) || !valid_coord(p.y)) {
                return fail(error, "invalid route point: " + e.id);
            }
        }
        points += e.points.size();
    }
    if (points > kTelaMaxPoints) return fail(error, "too many route points (max 4096)");
    return true;
}

std::string to_tela_graph_text(const Graph& g, std::string* error) {
    if (!validate_tela_graph(g, error)) return {};

    std::string out = "TELA_GRAPH 1\n";
    out += "graph " + quote(g.project) + " " + quote(g.title) + " " +
           number(g.width) + " " + number(g.height) + "\n";
    for (const auto& gr : g.groups) {
        out += "group " + quote(gr.id) + " " + quote(gr.name) + " " +
               (gr.visible ? "1" : "0") + " " + std::to_string(gr.color.r) + " " +
               std::to_string(gr.color.g) + " " + std::to_string(gr.color.b) + "\n";
    }
    for (const auto& n : g.nodes) {
        out += "node " + quote(n.group_id) + " " + quote(n.id) + " " + quote(n.label) + " " +
               number(n.x) + " " + number(n.y) + " " + number(n.width) + " " +
               number(n.height) + "\n";
    }
    for (const auto& e : g.edges) {
        out += "edge " + quote(e.id) + " " + quote(e.from) + " " + quote(e.to) + " " +
               (e.dashed ? "1" : "0") + "\n";
        for (const auto& p : e.points) {
            out += "point " + quote(e.id) + " " + number(p.x) + " " + number(p.y) + "\n";
        }
    }
    if (out.size() > kTelaMaxBytes) {
        fail(error, "file exceeds 4 MiB");
        return {};
    }
    return out;
}

bool save_tela_graph(const std::string& path, const Graph& graph, std::string* error) {
    const std::string text = to_tela_graph_text(graph, error);
    if (text.empty()) return false;

    // 途中で失敗しても既存ファイルを壊さないよう、 隣の一時ファイルへ書いてから置き換える。
    const std::filesystem::path target = std::filesystem::u8path(path);
    std::filesystem::path temp = target;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) return fail(error, "cannot open " + path + ".tmp");
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!file) return fail(error, "cannot write " + path + ".tmp");
    }
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return fail(error, "cannot replace " + path);
    }
    return true;
}

} // namespace ergo::tela_graph
