#include "ergo/render_viz/pipeline_layout.h"

#include "pipeline_labels.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ergo::render_viz {

const char* group_of(PassKind kind) {
    switch (kind) {
    case PassKind::DepthOnly:
    case PassKind::Shadow:      return kGroupGeometry;
    case PassKind::Opaque:
    case PassKind::Transparent: return kGroupShading;
    case PassKind::PostProcess: return kGroupPost;
    case PassKind::Compute:
    case PassKind::Custom:      return kGroupCompute;
    }
    return kGroupCompute;
}

namespace {

struct GroupDef {
    const char* id;
    const char* name;
    tela_graph::Color    color;
};

// 図の列順 (Tela の切り替えボタンの並び)。
const GroupDef kGroups[] = {
    {kGroupCpu,        "CPU 段",         {96, 125, 139}},
    {kGroupGeometry,   "影・深度",       {141, 110, 99}},
    {kGroupShading,    "不透明・半透明", {66, 133, 244}},
    {kGroupPost,       "ポストプロセス", {171, 71, 188}},
    {kGroupCompute,    "計算・カスタム", {0, 150, 136}},
    {kGroupAttachment, "アタッチメント", {245, 166, 35}},
};

/// 同名の要素が並んでも ID が衝突しないよう、 2 回目以降に "#n" を付ける。
class IdAllocator {
public:
    std::string make(const std::string& prefix, const std::string& name) {
        std::string id = prefix + name;
        const int n = ++seen_[id];
        return n == 1 ? id : id + "#" + std::to_string(n);
    }

private:
    std::unordered_map<std::string, int> seen_;
};

/// 参照されたアタッチメントの並び (初出順) と宣言の有無。
struct AttachmentSlot {
    AttachmentInfo info;
    bool           declared = false;
    std::string    node_id;
    float          x = 0.0f;
    std::size_t    writers = 0, readers = 0;   // 辺の端点を散らす総数
    std::size_t    next_writer = 0, next_reader = 0;
};

std::vector<AttachmentSlot> collect_attachments(const PipelineSnapshot& s) {
    std::unordered_map<std::string, const AttachmentInfo*> declared;
    for (const auto& a : s.attachments) declared.emplace(a.name, &a);

    std::vector<AttachmentSlot> slots;
    std::unordered_map<std::string, std::size_t> index;
    auto touch = [&](const std::string& name) -> AttachmentSlot& {
        auto it = index.find(name);
        if (it != index.end()) return slots[it->second];
        AttachmentSlot slot;
        auto d = declared.find(name);
        if (d != declared.end()) {
            slot.info     = *d->second;
            slot.declared = true;
        } else {
            slot.info.name = name;
        }
        index.emplace(name, slots.size());
        slots.push_back(std::move(slot));
        return slots.back();
    };

    for (const auto& pass : s.passes) {
        for (const auto& t : pass.targets) ++touch(t).writers;
        for (const auto& i : pass.inputs)  ++touch(i).readers;
    }
    // どのパスからも参照されない宣言も図に残す (使われていないことが見える)。
    for (const auto& a : s.attachments) touch(a.name);
    return slots;
}

/// 0..1 の区間 [lo, lo+span] を count 等分した j 番目の中心。
float spread(float lo, float span, std::size_t j, std::size_t count) {
    const float n = static_cast<float>(count == 0 ? 1 : count);
    return lo + span * (static_cast<float>(j) + 0.5f) / n;
}

void push_point(tela_graph::Edge& edge, float x, float y) {
    if (!edge.points.empty()) {
        const auto& last = edge.points.back();
        if (last.x == x && last.y == y) return;   // 長さ 0 の区間を作らない
    }
    edge.points.push_back({x, y});
}

class Builder {
public:
    Builder(const PipelineSnapshot& s, const LayoutOptions& o) : s_(s), o_(o) {
        pitch_ = o_.node_width + o_.column_gap;
        left_  = o_.margin + o_.lane_width;
    }

    tela_graph::Graph build() {
        graph_.project = o_.project;
        graph_.title   = detail::title_label(s_);

        auto slots = collect_attachments(s_);
        const bool has_cpu = !s_.cpu_stages.empty();
        const bool has_att = !slots.empty();

        float y = o_.margin;
        y_cpu_  = y;
        if (has_cpu) y += o_.node_height + o_.row_gap;
        y_att_  = y;
        if (has_att) y += o_.node_height + o_.row_gap;
        y_pass_ = y;
        y_fx_   = y_pass_ + o_.node_height + o_.row_gap;

        place_cpu_stages();
        place_attachments(slots);
        place_passes(slots);
        place_effects();
        finish_groups();
        return std::move(graph_);
    }

private:
    float column_x(std::size_t i) const { return left_ + pitch_ * static_cast<float>(i); }

    void add_node(const char* group, std::string id, std::string label, float x, float y) {
        graph_.nodes.push_back({group, std::move(id), std::move(label),
                                x, y, o_.node_width, o_.node_height});
        used_groups_.insert(group);
        extend(x + o_.node_width, y + o_.node_height);
    }

    tela_graph::Edge& add_edge(const std::string& from, const std::string& to, bool dashed) {
        tela_graph::Edge edge;
        edge.id     = "e" + std::to_string(graph_.edges.size());
        edge.from   = from;
        edge.to     = to;
        edge.dashed = dashed;
        graph_.edges.push_back(std::move(edge));
        return graph_.edges.back();
    }

    void extend(float right, float bottom) {
        graph_.width  = std::max(graph_.width,  right  + o_.margin);
        graph_.height = std::max(graph_.height, bottom + o_.margin);
    }

    // アタッチメント行とパス行の間の水平通路。 辺ごとに段をずらして重なりを減らす。
    float channel_y(std::size_t edge_ordinal) const {
        const float top    = y_att_ + o_.node_height + 8.0f;
        const float height = std::max(8.0f, o_.row_gap - 16.0f);
        const std::size_t lanes = std::max<std::size_t>(1, static_cast<std::size_t>(height / 10.0f));
        return spread(top, height, edge_ordinal % lanes, lanes);
    }

    void place_cpu_stages() {
        for (std::size_t i = 0; i < s_.cpu_stages.size(); ++i) {
            const auto& stage = s_.cpu_stages[i];
            std::string id = ids_.make("cpu:", stage.name);
            add_node(kGroupCpu, id, detail::stage_label(stage), column_x(i), y_cpu_);
            if (i > 0) {
                const float cy = y_cpu_ + o_.node_height * 0.5f;
                auto& e = add_edge(cpu_ids_.back(), id, false);
                push_point(e, column_x(i - 1) + o_.node_width, cy);
                push_point(e, column_x(i), cy);
            }
            cpu_ids_.push_back(std::move(id));
        }
    }

    void place_attachments(std::vector<AttachmentSlot>& slots) {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            auto& slot   = slots[i];
            slot.x       = column_x(i);
            slot.node_id = ids_.make("att:", slot.info.name);
            add_node(kGroupAttachment, slot.node_id,
                     detail::attachment_label(slot.info, slot.declared), slot.x, y_att_);
            att_index_.emplace(slot.info.name, i);
        }
    }

    double heaviest_kind_ms() const {
        double best = 0.0;
        for (const auto& p : s_.passes) {
            if (p.kind_gpu_ms) best = std::max(best, *p.kind_gpu_ms);
        }
        return best;
    }

    void place_passes(std::vector<AttachmentSlot>& slots) {
        const double heaviest = heaviest_kind_ms();
        const float  w        = o_.node_width;
        const float  att_bottom = y_att_ + o_.node_height;

        for (std::size_t i = 0; i < s_.passes.size(); ++i) {
            const auto& pass = s_.passes[i];
            const float px   = column_x(i);
            const bool  hot  = heaviest > 0.0 && pass.kind_gpu_ms && *pass.kind_gpu_ms == heaviest;
            std::string id = ids_.make("pass:", pass.name);
            add_node(group_of(pass.kind), id, detail::pass_label(pass, hot), px, y_pass_);

            // 書き込み: パス上辺の左半分 → アタッチメント下辺の左半分 (実線)。
            for (std::size_t j = 0; j < pass.targets.size(); ++j) {
                auto& slot = slots[att_index_.at(pass.targets[j])];
                const float sx = px + w * spread(0.08f, 0.36f, j, pass.targets.size());
                const float tx = slot.x + w * spread(0.08f, 0.36f, slot.next_writer++, slot.writers);
                const float cy = channel_y(graph_.edges.size());
                auto& e = add_edge(id, slot.node_id, false);
                push_point(e, sx, y_pass_);
                push_point(e, sx, cy);
                push_point(e, tx, cy);
                push_point(e, tx, att_bottom);
            }
            // 読み取り: アタッチメント下辺の右半分 → パス上辺の右半分 (破線)。
            for (std::size_t j = 0; j < pass.inputs.size(); ++j) {
                auto& slot = slots[att_index_.at(pass.inputs[j])];
                const float sx = slot.x + w * spread(0.56f, 0.36f, slot.next_reader++, slot.readers);
                const float tx = px + w * spread(0.56f, 0.36f, j, pass.inputs.size());
                const float cy = channel_y(graph_.edges.size());
                auto& e = add_edge(slot.node_id, id, true);
                push_point(e, sx, att_bottom);
                push_point(e, sx, cy);
                push_point(e, tx, cy);
                push_point(e, tx, y_pass_);
            }
            // 実行順: 直前のパスの右辺 → このパスの左辺 (実線)。 書き込み先を宣言しない
            // パス (組み込みプリセットに多い) でも流れが読めるようにする。
            if (!pass_ids_.empty()) {
                const float cy = y_pass_ + o_.node_height * 0.5f;
                auto& e = add_edge(pass_ids_.back(), id, false);
                push_point(e, px - o_.column_gap, cy);
                push_point(e, px, cy);
            }
            if (pass.kind == PassKind::PostProcess && first_post_.empty()) {
                first_post_   = id;
                first_post_x_ = px;
            }
            pass_ids_.push_back(std::move(id));
        }

        // CPU 段の最後 → 最初のパス。 左の通路を降りてパスの左辺へ入る。
        if (!cpu_ids_.empty() && !pass_ids_.empty()) {
            const float last_x = column_x(cpu_ids_.size() - 1) + w * 0.5f;
            const float lane_x = o_.margin + o_.lane_width * 0.5f;
            const float turn_y = y_cpu_ + o_.node_height + o_.row_gap * 0.5f;
            const float mid_y  = y_pass_ + o_.node_height * 0.5f;
            auto& e = add_edge(cpu_ids_.back(), pass_ids_.front(), false);
            push_point(e, last_x, y_cpu_ + o_.node_height);
            push_point(e, last_x, turn_y);
            push_point(e, lane_x, turn_y);
            push_point(e, lane_x, mid_y);
            push_point(e, left_, mid_y);
        }
    }

    void place_effects() {
        if (s_.effects.empty()) return;
        const float x0 = first_post_.empty() ? left_ : first_post_x_;
        const float cy = y_fx_ + o_.node_height * 0.5f;
        std::string prev;
        for (std::size_t k = 0; k < s_.effects.size(); ++k) {
            const auto& fx = s_.effects[k];
            const float x  = x0 + pitch_ * static_cast<float>(k);
            std::string id = ids_.make("fx:", fx.name);
            add_node(kGroupPost, id, detail::effect_label(fx), x, y_fx_);
            if (k == 0 && !first_post_.empty()) {
                const float cx = x + o_.node_width * 0.5f;
                auto& e = add_edge(first_post_, id, false);
                push_point(e, cx, y_pass_ + o_.node_height);
                push_point(e, cx, y_fx_);
            } else if (k > 0) {
                auto& e = add_edge(prev, id, false);
                push_point(e, x - o_.column_gap, cy);
                push_point(e, x, cy);
            }
            prev = std::move(id);
        }
    }

    void finish_groups() {
        for (const auto& g : kGroups) {
            if (used_groups_.count(g.id)) {
                graph_.groups.push_back({g.id, g.name, true, g.color});
            }
        }
    }

    const PipelineSnapshot& s_;
    const LayoutOptions&    o_;
    tela_graph::Graph                graph_;
    IdAllocator             ids_;
    std::unordered_set<std::string> used_groups_;
    std::unordered_map<std::string, std::size_t> att_index_;
    std::vector<std::string> cpu_ids_;
    std::vector<std::string> pass_ids_;
    std::string first_post_;
    float first_post_x_ = 0.0f;
    float pitch_ = 0.0f, left_ = 0.0f;
    float y_cpu_ = 0.0f, y_att_ = 0.0f, y_pass_ = 0.0f, y_fx_ = 0.0f;
};

} // namespace

tela_graph::Graph layout_pipeline(const PipelineSnapshot& snapshot, const LayoutOptions& options) {
    return Builder(snapshot, options).build();
}

} // namespace ergo::render_viz
