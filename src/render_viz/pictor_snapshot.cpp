#include "ergo/render_viz/pictor_snapshot.h"

#include "pictor/pipeline/attachment_def.h"
#include "pictor/pipeline/pipeline_profile.h"
#include "pictor/profiler/profiler.h"

#include <optional>

// Windows の wingdi.h は OPAQUE / TRANSPARENT をマクロ定義する。 Pictor のヘッダ経由で
// 取り込まれると pictor::PassType の列挙子名が置換されるので、 ここで外す。
#ifdef OPAQUE
#undef OPAQUE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace ergo::render_viz {

namespace {

PassKind to_kind(pictor::PassType type) {
    switch (type) {
    case pictor::PassType::DEPTH_ONLY:   return PassKind::DepthOnly;
    case pictor::PassType::OPAQUE:       return PassKind::Opaque;
    case pictor::PassType::TRANSPARENT:  return PassKind::Transparent;
    case pictor::PassType::SHADOW:       return PassKind::Shadow;
    case pictor::PassType::POST_PROCESS: return PassKind::PostProcess;
    case pictor::PassType::COMPUTE:      return PassKind::Compute;
    case pictor::PassType::CUSTOM:       return PassKind::Custom;
    }
    return PassKind::Custom;
}

const char* to_label(pictor::RenderingPath path) {
    switch (path) {
    case pictor::RenderingPath::FORWARD:      return "Forward";
    case pictor::RenderingPath::FORWARD_PLUS: return "Forward+";
    case pictor::RenderingPath::DEFERRED:     return "Deferred";
    case pictor::RenderingPath::HYBRID:       return "Hybrid";
    }
    return "";
}

/// Pictor の FrameStats は GPU 時間を種別単位でしか持たない。
std::optional<double> kind_gpu_ms(PassKind kind, const pictor::FrameStats& s) {
    switch (kind) {
    case PassKind::Shadow:      return s.shadow_gpu_ms;
    case PassKind::DepthOnly:   return s.depth_prepass_gpu_ms;
    case PassKind::Opaque:      return s.opaque_gpu_ms;
    case PassKind::Transparent: return s.transparent_gpu_ms;
    case PassKind::PostProcess: return s.post_process_gpu_ms;
    case PassKind::Compute:     return s.compute_update_gpu_ms;
    case PassKind::Custom:      return std::nullopt;   // 計測の受け皿が無い
    }
    return std::nullopt;
}

AttachmentInfo to_info(const pictor::AttachmentDef& def) {
    AttachmentInfo info;
    info.name      = def.name;
    info.format    = pictor::to_string(def.format);
    info.swapchain = def.kind == pictor::AttachmentKind::SWAPCHAIN_COLOR;
    info.depth     = def.kind == pictor::AttachmentKind::DEPTH ||
                     pictor::is_depth_format(def.format);
    if (info.swapchain) info.format.clear();   // 形式は実行時の swapchain 次第
    return info;
}

} // namespace

PipelineSnapshot snapshot_from_pictor(const pictor::PipelineProfileDef& profile,
                                      const pictor::FrameStats*         stats) {
    PipelineSnapshot snap;
    snap.profile_name   = profile.profile_name;
    snap.rendering_path = to_label(profile.rendering_path);

    if (profile.attachments.empty()) {
        for (const auto& def : pictor::default_attachments()) snap.attachments.push_back(to_info(def));
    } else {
        for (const auto& def : profile.attachments) snap.attachments.push_back(to_info(def));
    }

    for (const auto& def : profile.render_passes) {
        PassInfo pass;
        pass.name          = def.pass_name;
        pass.kind          = to_kind(def.pass_type);
        pass.targets       = def.render_targets;
        pass.inputs        = def.input_textures;
        pass.host_recorded = def.host_recorded;
        pass.gpu_driven    = def.gpu_driven_pass;
        if (stats) pass.kind_gpu_ms = kind_gpu_ms(pass.kind, *stats);
        snap.passes.push_back(std::move(pass));
    }

    for (const auto& fx : profile.post_process_stack) {
        snap.effects.push_back({fx.name, fx.enabled});
    }

    if (stats) {
        snap.cpu_stages = {
            {"DataUpdate",    stats->data_update_ms},
            {"Culling",       stats->culling_ms},
            {"Sort",          stats->sort_ms},
            {"BatchBuild",    stats->batch_build_ms},
            {"CommandEncode", stats->command_encode_ms},
        };
        snap.frame_ms     = stats->frame_time_ms;
        snap.gpu_frame_ms = stats->gpu_frame_time_ms;
    }
    return snap;
}

} // namespace ergo::render_viz
