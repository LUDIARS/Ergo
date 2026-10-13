#include "pipeline_labels.h"

#include <iomanip>
#include <locale>
#include <sstream>

namespace ergo::render_viz {

const char* to_label(PassKind kind) {
    switch (kind) {
    case PassKind::DepthOnly:   return "深度";
    case PassKind::Opaque:      return "不透明";
    case PassKind::Transparent: return "半透明";
    case PassKind::Shadow:      return "影";
    case PassKind::PostProcess: return "ポストプロセス";
    case PassKind::Compute:     return "計算";
    case PassKind::Custom:      return "カスタム";
    }
    return "不明";
}

namespace detail {

std::string format_ms(double ms) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(2) << ms << " ms";
    return out.str();
}

std::string title_label(const PipelineSnapshot& snapshot) {
    std::string title = snapshot.profile_name.empty() ? "描画パイプライン"
                                                      : snapshot.profile_name;
    if (!snapshot.rendering_path.empty()) {
        title += " · " + snapshot.rendering_path;
    }
    if (snapshot.frame_ms) {
        title += " · フレーム " + format_ms(*snapshot.frame_ms);
    }
    if (snapshot.gpu_frame_ms) {
        title += " / GPU " + format_ms(*snapshot.gpu_frame_ms);
    }
    return title;
}

std::string pass_label(const PassInfo& pass, bool heaviest) {
    std::string label = pass.name + " | " + to_label(pass.kind);
    if (pass.host_recorded) label += " · ホスト記録";
    if (pass.gpu_driven)    label += " · GPU駆動";
    label += " | 書く " + std::to_string(pass.targets.size()) +
             " / 読む " + std::to_string(pass.inputs.size());
    if (pass.kind_gpu_ms) {
        label += " | GPU " + format_ms(*pass.kind_gpu_ms) + " (種別合計)";
        if (heaviest) label += " ★最重";
    }
    return label;
}

std::string attachment_label(const AttachmentInfo& attachment, bool declared) {
    std::string label = attachment.name;
    if (!attachment.format.empty()) label += " | " + attachment.format;
    if (attachment.swapchain)       label += " · 表示先";
    if (attachment.depth)           label += " · 深度";
    if (!declared)                  label += " | 未宣言";
    return label;
}

std::string effect_label(const EffectInfo& effect) {
    return effect.enabled ? effect.name : effect.name + " (無効)";
}

std::string stage_label(const StageTiming& stage) {
    return stage.name + " | " + format_ms(stage.cpu_ms);
}

} // namespace detail
} // namespace ergo::render_viz
