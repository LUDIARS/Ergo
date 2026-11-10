#include "profile_fields.h"

namespace ergo::studio::plugins::render_pipeline {
namespace {

Field num(const char* k, double d) {
    Field f;
    f.key  = k;
    f.type = FieldType::Number;
    f.num  = d;
    return f;
}
Field boolean(const char* k, bool d) {
    Field f;
    f.key  = k;
    f.type = FieldType::Bool;
    f.flag = d;
    return f;
}
Field str(const char* k, const char* d) {
    Field f;
    f.key  = k;
    f.type = FieldType::String;
    f.text = d;
    return f;
}
Field choice(const char* k, const Choices& c, const char* d) {
    Field f;
    f.key     = k;
    f.type    = FieldType::Enum;
    f.choices = &c;
    f.text    = d;
    return f;
}
Field vec3(const char* k, double x, double y, double z) {
    Field f;
    f.key  = k;
    f.type = FieldType::Vec3;
    f.vec  = {x, y, z};
    return f;
}
Field strings(const char* k) {
    Field f;
    f.key  = k;
    f.type = FieldType::StringArray;
    return f;
}
Field sub(const char* k, const Section& s) {
    Field f;
    f.key     = k;
    f.type    = FieldType::Section;
    f.section = &s;
    return f;
}

} // namespace

#define ERGO_CHOICES(name, ...)                     \
    const Choices& name() {                          \
        static const Choices c{__VA_ARGS__};         \
        return c;                                    \
    }

ERGO_CHOICES(rendering_paths, "FORWARD", "FORWARD_PLUS", "DEFERRED", "HYBRID")
ERGO_CHOICES(pass_types, "DEPTH_ONLY", "OPAQUE", "TRANSPARENT", "SHADOW", "POST_PROCESS", "COMPUTE", "CUSTOM")
ERGO_CHOICES(sort_modes, "FRONT_TO_BACK", "BACK_TO_FRONT", "NONE")
ERGO_CHOICES(shadow_filter_modes, "NONE", "PCF", "PCSS")
ERGO_CHOICES(overlay_modes, "OFF", "MINIMAL", "STANDARD", "DETAILED", "TIMELINE")
ERGO_CHOICES(post_process_kinds, "Unknown", "Bloom", "ToneMapping", "Vignette", "ColorGrading", "DepthOfField")
ERGO_CHOICES(tone_map_operators, "ACES_FILMIC", "REINHARD", "REINHARD_EXT", "UNCHARTED2", "LINEAR_CLAMP")
ERGO_CHOICES(attachment_kinds, "COLOR", "DEPTH", "SWAPCHAIN_COLOR")
ERGO_CHOICES(attachment_sizings, "SWAPCHAIN_RELATIVE", "ABSOLUTE")
ERGO_CHOICES(attachment_formats, "UNDEFINED", "R8G8B8A8_UNORM", "R8G8B8A8_SRGB", "B8G8R8A8_UNORM", "B8G8R8A8_SRGB",
             "R16G16B16A16_SFLOAT", "R32G32B32A32_SFLOAT", "R11G11B10_UFLOAT", "D16_UNORM", "D24_UNORM_S8_UINT",
             "D32_SFLOAT", "D32_SFLOAT_S8_UINT")
ERGO_CHOICES(attachment_usage_flags, "TRANSFER_SRC", "TRANSFER_DST", "SAMPLED", "STORAGE", "COLOR_ATTACHMENT",
             "DEPTH_STENCIL_ATTACHMENT", "TRANSIENT_ATTACHMENT", "INPUT_ATTACHMENT")
ERGO_CHOICES(attachment_load_ops, "LOAD", "CLEAR", "DONT_CARE", "NONE")
ERGO_CHOICES(attachment_store_ops, "STORE", "DONT_CARE", "NONE")
ERGO_CHOICES(image_layouts, "UNDEFINED", "GENERAL", "COLOR_ATTACHMENT_OPTIMAL", "DEPTH_STENCIL_ATTACHMENT_OPTIMAL",
             "DEPTH_STENCIL_READ_ONLY_OPTIMAL", "SHADER_READ_ONLY_OPTIMAL", "TRANSFER_SRC_OPTIMAL",
             "TRANSFER_DST_OPTIMAL", "PRESENT_SRC_KHR")

#undef ERGO_CHOICES

const Section& profile_scalars() {
    static const Section s{{
        str("profile_name", ""),
        choice("rendering_path", rendering_paths(), "FORWARD_PLUS"),
        num("max_lights", 256),
        num("msaa_samples", 0),
        boolean("gpu_driven_enabled", true),
        boolean("compute_update_enabled", true),
    }};
    return s;
}

const Section& render_pass_section() {
    static const Section s{{
        str("pass_name", ""),
        choice("pass_type", pass_types(), "OPAQUE"),
        str("shader_override", "none"),
        strings("render_targets"),
        strings("input_textures"),
        choice("sort_mode", sort_modes(), "FRONT_TO_BACK"),
        num("filter_mask", 65535),
        boolean("gpu_driven_pass", false),
        strings("required_streams"),
    }};
    return s;
}

const Section& attachment_ops_section() {
    static const Section s{{
        str("attachment", ""),
        choice("load", attachment_load_ops(), "CLEAR"),
        choice("store", attachment_store_ops(), "STORE"),
        choice("initial_layout", image_layouts(), "UNDEFINED"),
        choice("final_layout", image_layouts(), "SHADER_READ_ONLY_OPTIMAL"),
    }};
    return s;
}

const Section& bloom_section() {
    static const Section s{{num("threshold", 1.0), num("soft_threshold", 0.5), num("intensity", 0.8), num("radius", 5.0),
                            num("mip_levels", 5), num("scatter", 0.7)}};
    return s;
}

const Section& tone_mapping_section() {
    static const Section s{{choice("op", tone_map_operators(), "ACES_FILMIC"), num("exposure", 1.0), num("gamma", 2.2),
                            num("white_point", 4.0), num("saturation", 1.0)}};
    return s;
}

const Section& vignette_section() {
    static const Section s{{num("intensity", 0.35), num("radius", 0.75), num("softness", 0.45), vec3("color", 0, 0, 0)}};
    return s;
}

const Section& color_grading_section() {
    static const Section s{{str("lut_path", ""), num("lut_intensity", 1.0), num("lut_size", 16)}};
    return s;
}

const Section& depth_of_field_section() {
    static const Section s{{num("focus_distance", 10.0), num("focus_range", 5.0), num("bokeh_radius", 4.0),
                            num("near_start", 0.0), num("near_end", 3.0), num("far_start", 15.0), num("far_end", 50.0),
                            num("sample_count", 16)}};
    return s;
}

const Section& shadow_section() {
    static const Section s{{num("cascade_count", 3), num("resolution", 2048),
                            choice("filter_mode", shadow_filter_modes(), "PCF")}};
    return s;
}

namespace {
const Section& shadow_map_section() {
    static const Section s{{num("cascade_count", 3), num("resolution", 2048), num("depth_bias", 0), num("normal_bias", 0),
                            num("slope_scale_bias", 0), num("cascade_lambda", 0.5), num("max_shadow_dist", 150),
                            num("cascade_blend_width", 0), choice("filter_mode", shadow_filter_modes(), "PCF"),
                            num("shadow_strength", 1), num("pcss_light_size", 0.05), num("pcss_min_penumbra", 1),
                            num("pcss_max_penumbra", 16), num("pcss_blocker_search_radius", 8)}};
    return s;
}
const Section& ssao_section() {
    static const Section s{{num("sample_count", 32), num("radius", 0.5), num("bias", 0.025), num("intensity", 1),
                            num("falloff_start", 0), num("falloff_end", 1), boolean("blur_enabled", true)}};
    return s;
}
const Section& probes_section() {
    static const Section s{{vec3("grid_origin", 0, 0, 0), vec3("grid_spacing", 1, 1, 1), num("grid_x", 8), num("grid_y", 8),
                            num("grid_z", 8), num("gi_intensity", 1), num("max_probe_distance", 10)}};
    return s;
}
const Section& gpu_memory_section() {
    static const Section s{{num("mesh_pool_size", 268435456), num("ssbo_pool_size", 134217728),
                            num("instance_buffer_size", 67108864), num("indirect_buffer_size", 16777216),
                            num("staging_buffer_size", 67108864)}};
    return s;
}
} // namespace

const Section& gi_section() {
    static const Section s{{boolean("shadow_enabled", true), boolean("ssao_enabled", true),
                            boolean("gi_probes_enabled", false), sub("shadow", shadow_map_section()),
                            sub("ssao", ssao_section()), sub("probes", probes_section())}};
    return s;
}

const Section& memory_section() {
    static const Section s{{num("frame_allocator_size", 16777216), num("flight_count", 3), num("pool_chunk_size", 65536),
                            boolean("use_large_pages", false), sub("gpu", gpu_memory_section())}};
    return s;
}

const Section& gpu_driven_section() {
    static const Section s{{num("max_triangle_count", 50000), num("min_instance_count", 32), num("workgroup_size", 256),
                            boolean("two_phase_culling", true), boolean("compute_update", true)}};
    return s;
}

const Section& update_section() {
    static const Section s{{num("chunk_size", 16384), num("worker_threads", 0), boolean("nt_store_enabled", true),
                            num("nt_store_threshold", 10000)}};
    return s;
}

const Section& profiler_section() {
    static const Section s{{boolean("enabled", true), choice("overlay_mode", overlay_modes(), "STANDARD"),
                            num("max_queries", 64)}};
    return s;
}

const std::vector<NamedSection>& tail_sections() {
    static const std::vector<NamedSection> v{{"shadow", &shadow_section()},         {"gi", &gi_section()},
                                             {"memory", &memory_section()},         {"gpu_driven", &gpu_driven_section()},
                                             {"update", &update_section()},         {"profiler", &profiler_section()}};
    return v;
}

} // namespace ergo::studio::plugins::render_pipeline
