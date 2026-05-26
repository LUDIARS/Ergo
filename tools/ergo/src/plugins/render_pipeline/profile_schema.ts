/// Pipeline Profile schema (v1) — the editable side of the render_pipeline
/// plugin. This is the TypeScript mirror of Pictor's `PipelineProfileDef`
/// external configuration.
///
/// Canonical schema:
///   Pictor/spec/pipeline-profile-config.md   (schema version 1)
/// Canonical samples (the files this plugin reads / writes):
///   Pictor/profiles/*.profile.json           (5 presets)
///
/// IMPORTANT — keep this file in sync with the documents above. The C++
/// struct it mirrors is `pictor::PipelineProfileDef`
/// (`include/pictor/pipeline/pipeline_profile.h`).
///
/// Profile-only design (Phase 3, PR #34): the legacy *scanner* snapshot
/// (`render_pipeline.json`) and its Timeline/Scanner UI modes have been
/// removed. `*.profile.json` is now the single source of truth — this
/// schema describes the declarative profile data (系統A) that the single
/// NodeGraph editor reads and writes.

export const PROFILE_SCHEMA_VERSION = 2;

// ---- enums (mirror the C++ enum string tokens) ---------------------------

/** `PipelineProfileDef::rendering_path` */
export const RENDERING_PATHS = ["FORWARD", "FORWARD_PLUS", "DEFERRED", "HYBRID"] as const;
export type RenderingPath = (typeof RENDERING_PATHS)[number];

/** `RenderPassDef::pass_type` */
export const PASS_TYPES = [
    "DEPTH_ONLY", "OPAQUE", "TRANSPARENT", "SHADOW",
    "POST_PROCESS", "COMPUTE", "CUSTOM",
] as const;
export type PassType = (typeof PASS_TYPES)[number];

/** `RenderPassDef::sort_mode` */
export const SORT_MODES = ["FRONT_TO_BACK", "BACK_TO_FRONT", "NONE"] as const;
export type SortMode = (typeof SORT_MODES)[number];

/** `ShadowConfig::filter_mode` / `ShadowMapConfig::filter_mode` */
export const SHADOW_FILTER_MODES = ["NONE", "PCF", "PCSS"] as const;
export type ShadowFilterMode = (typeof SHADOW_FILTER_MODES)[number];

/** `ProfilerConfig::overlay_mode` */
export const OVERLAY_MODES = ["OFF", "MINIMAL", "STANDARD", "DETAILED", "TIMELINE"] as const;
export type OverlayMode = (typeof OVERLAY_MODES)[number];

/** Valid `msaa_samples` values (0 = disabled). */
export const MSAA_SAMPLES = [0, 2, 4, 8] as const;

/**
 * `PostProcessDef::kind` — the effect-type tag (spec §3.4).
 * `Unknown` covers effects with no host-driven implementation in
 * `PostProcessPipeline` (SSAO / TAA / FXAA / VolumetricFog …): they
 * round-trip but the C++ bridge ignores them (系統B phase 2).
 */
export const POST_PROCESS_KINDS = [
    "Unknown", "Bloom", "ToneMapping", "Vignette", "ColorGrading", "DepthOfField",
] as const;
export type PostProcessKind = (typeof POST_PROCESS_KINDS)[number];

/** `ToneMappingConfig::op` (mirror `pictor::ToneMapOperator`). */
export const TONE_MAP_OPERATORS = [
    "ACES_FILMIC", "REINHARD", "REINHARD_EXT", "UNCHARTED2", "LINEAR_CLAMP",
] as const;
export type ToneMapOperator = (typeof TONE_MAP_OPERATORS)[number];

// ---- per-element structs -------------------------------------------------

// ---- Phase 3 (schema v2): attachments + per-pass attachment_ops ----------

/** `AttachmentDef::kind` (Pictor `pictor::AttachmentKind`). */
export const ATTACHMENT_KINDS = ["COLOR", "DEPTH", "SWAPCHAIN_COLOR"] as const;
export type AttachmentKind = (typeof ATTACHMENT_KINDS)[number];

/** `AttachmentDef::sizing`. */
export const ATTACHMENT_SIZINGS = ["SWAPCHAIN_RELATIVE", "ABSOLUTE"] as const;
export type AttachmentSizing = (typeof ATTACHMENT_SIZINGS)[number];

/** `AttachmentDef::format` — subset of Vulkan formats (matches `pictor::AttachmentFormat`). */
export const ATTACHMENT_FORMATS = [
    "UNDEFINED",
    "R8G8B8A8_UNORM", "R8G8B8A8_SRGB", "B8G8R8A8_UNORM", "B8G8R8A8_SRGB",
    "R16G16B16A16_SFLOAT", "R32G32B32A32_SFLOAT", "R11G11B10_UFLOAT",
    "D16_UNORM", "D24_UNORM_S8_UINT", "D32_SFLOAT", "D32_SFLOAT_S8_UINT",
] as const;
export type AttachmentFormat = (typeof ATTACHMENT_FORMATS)[number];

/** `AttachmentUsageBits` — `AttachmentDef::usage` flag names. */
export const ATTACHMENT_USAGE_FLAGS = [
    "TRANSFER_SRC", "TRANSFER_DST", "SAMPLED", "STORAGE",
    "COLOR_ATTACHMENT", "DEPTH_STENCIL_ATTACHMENT",
    "TRANSIENT_ATTACHMENT", "INPUT_ATTACHMENT",
] as const;
export type AttachmentUsageFlag = (typeof ATTACHMENT_USAGE_FLAGS)[number];

/** `AttachmentLoadOp`. */
export const ATTACHMENT_LOAD_OPS = ["LOAD", "CLEAR", "DONT_CARE", "NONE"] as const;
export type AttachmentLoadOp = (typeof ATTACHMENT_LOAD_OPS)[number];

/** `AttachmentStoreOp`. */
export const ATTACHMENT_STORE_OPS = ["STORE", "DONT_CARE", "NONE"] as const;
export type AttachmentStoreOp = (typeof ATTACHMENT_STORE_OPS)[number];

/** `ImageLayout` — subset of VkImageLayout (matches `pictor::ImageLayout`). */
export const IMAGE_LAYOUTS = [
    "UNDEFINED", "GENERAL",
    "COLOR_ATTACHMENT_OPTIMAL",
    "DEPTH_STENCIL_ATTACHMENT_OPTIMAL", "DEPTH_STENCIL_READ_ONLY_OPTIMAL",
    "SHADER_READ_ONLY_OPTIMAL",
    "TRANSFER_SRC_OPTIMAL", "TRANSFER_DST_OPTIMAL",
    "PRESENT_SRC_KHR",
] as const;
export type ImageLayout = (typeof IMAGE_LAYOUTS)[number];

/**
 * Named GPU attachment (Phase 3, schema v2). Mirrors C++
 * `pictor::AttachmentDef`. The reserved name `"swapchain"` always has
 * `kind === "SWAPCHAIN_COLOR"`; the Pictor runtime injects per-image
 * views regardless of `format` (the field is metadata for the editor).
 */
export interface AttachmentDef {
    name:           string;
    kind:           AttachmentKind;
    format:         AttachmentFormat;
    sizing:         AttachmentSizing;
    scale?:         number;      // sizing === "SWAPCHAIN_RELATIVE"
    width?:         number;      // sizing === "ABSOLUTE"
    height?:        number;
    usage:          AttachmentUsageFlag[];
    clear_color?:   [number, number, number, number]; // kind === "COLOR" / "SWAPCHAIN_COLOR"
    clear_depth?:   number;      // kind === "DEPTH"
    clear_stencil?: number;
}

/** Per-pass override of how an attachment is used (`attachment_ops[]`). */
export interface AttachmentOpsDef {
    attachment:     string;       // must match attachments[].name
    load:           AttachmentLoadOp;
    store:          AttachmentStoreOp;
    initial_layout: ImageLayout;
    final_layout:   ImageLayout;
}

/** `RenderPassDef` — one entry of `render_passes[]` (spec §3.2). */
export interface RenderPassDef {
    pass_name:        string;
    pass_type:        PassType;
    /** `"none"` or `"handle:<u32>"`. */
    shader_override:  string;
    render_targets:   string[];
    input_textures:   string[];
    sort_mode:        SortMode;
    filter_mask:      number;
    gpu_driven_pass:  boolean;
    required_streams: string[];
    /** Phase 3: per-attachment load/store overrides, in `render_targets` order.
     *  Empty array = runtime infers defaults (CLEAR/STORE for color,
     *  CLEAR/DONT_CARE for depth, CLEAR/STORE → PRESENT_SRC_KHR for swapchain). */
    attachment_ops:   AttachmentOpsDef[];
}

/** `BloomConfig` — `post_process[].bloom` (spec §3.4). */
export interface BloomParams {
    threshold:      number;
    soft_threshold: number;
    intensity:      number;
    radius:         number;
    mip_levels:     number;
    scatter:        number;
}

/** `ToneMappingConfig` — `post_process[].tone_mapping` (spec §3.4). */
export interface ToneMappingParams {
    op:          ToneMapOperator;
    exposure:    number;
    gamma:       number;
    white_point: number;
    saturation:  number;
}

/** `VignetteConfig` — `post_process[].vignette` (spec §3.4). */
export interface VignetteParams {
    intensity: number;
    radius:    number;
    softness:  number;
    color:     [number, number, number];
}

/** `ColorGradingConfig` — `post_process[].color_grading` (spec §3.4). */
export interface ColorGradingParams {
    lut_path:      string;
    lut_intensity: number;
    lut_size:      number;
}

/** `DepthOfFieldConfig` — `post_process[].depth_of_field` (spec §3.4). */
export interface DepthOfFieldParams {
    focus_distance: number;
    focus_range:    number;
    bokeh_radius:   number;
    near_start:     number;
    near_end:       number;
    far_start:      number;
    far_end:        number;
    sample_count:   number;
}

/**
 * `PostProcessDef` — one entry of `post_process[]` (spec §3.3 / §3.4).
 *
 * Besides `name` / `enabled`, the def carries a typed `kind` and one
 * parameter struct per supported effect. The C++ serializer round-trips the
 * parameter block matching `kind` and `build_post_process_config()` folds
 * the stack into the real `PostProcessConfig`. Only the block matching
 * `kind` is consumed; the others are kept at defaults for the editor.
 *
 * `kind` is normally derived from `name` (case-insensitive); an explicit
 * `kind` overrides the inference. Effects with no host-driven
 * implementation resolve to `Unknown` and carry no parameter block.
 */
export interface PostProcessDef {
    name:            string;
    enabled:         boolean;
    kind:            PostProcessKind;
    bloom:           BloomParams;
    tone_mapping:    ToneMappingParams;
    vignette:        VignetteParams;
    color_grading:   ColorGradingParams;
    depth_of_field:  DepthOfFieldParams;
}

/** `ShadowConfig` — top-level `shadow` object (spec §3.5). */
export interface ShadowConfig {
    cascade_count: number;
    resolution:    number;
    filter_mode:   ShadowFilterMode;
}

/** `ShadowMapConfig` — `gi.shadow` (spec §3.6). */
export interface ShadowMapConfig {
    cascade_count:              number;
    resolution:                 number;
    depth_bias:                 number;
    normal_bias:                number;
    slope_scale_bias:           number;
    cascade_lambda:             number;
    max_shadow_dist:            number;
    cascade_blend_width:        number;
    filter_mode:                ShadowFilterMode;
    shadow_strength:            number;
    pcss_light_size:            number;
    pcss_min_penumbra:          number;
    pcss_max_penumbra:          number;
    pcss_blocker_search_radius: number;
}

/** `SSAOConfig` — `gi.ssao` (spec §3.6). */
export interface SSAOConfig {
    sample_count:  number;
    radius:        number;
    bias:          number;
    intensity:     number;
    falloff_start: number;
    falloff_end:   number;
    blur_enabled:  boolean;
}

/** `GIProbeConfig` — `gi.probes` (spec §3.6). */
export interface GIProbeConfig {
    grid_origin:        [number, number, number];
    grid_spacing:       [number, number, number];
    grid_x:             number;
    grid_y:             number;
    grid_z:             number;
    gi_intensity:       number;
    max_probe_distance: number;
}

/** `GIConfig` — top-level `gi` object (spec §3.6). */
export interface GIConfig {
    shadow_enabled:    boolean;
    ssao_enabled:      boolean;
    gi_probes_enabled: boolean;
    shadow:            ShadowMapConfig;
    ssao:              SSAOConfig;
    probes:            GIProbeConfig;
}

/** `GpuMemoryAllocator::Config` — `memory.gpu` (spec §3.7), bytes. */
export interface GpuMemoryConfig {
    mesh_pool_size:       number;
    ssbo_pool_size:       number;
    instance_buffer_size: number;
    indirect_buffer_size: number;
    staging_buffer_size:  number;
}

/** `MemoryConfig` — top-level `memory` object (spec §3.7), bytes. */
export interface MemoryConfig {
    frame_allocator_size: number;
    flight_count:         number;
    pool_chunk_size:      number;
    use_large_pages:      boolean;
    gpu:                  GpuMemoryConfig;
}

/** `GPUDrivenConfig` — top-level `gpu_driven` object (spec §3.8). */
export interface GPUDrivenConfig {
    max_triangle_count: number;
    min_instance_count: number;
    workgroup_size:     number;
    two_phase_culling:  boolean;
    compute_update:     boolean;
}

/** `UpdateConfig` — top-level `update` object (spec §3.9). */
export interface UpdateConfig {
    chunk_size:         number;
    worker_threads:     number;
    nt_store_enabled:   boolean;
    nt_store_threshold: number;
}

/** `ProfilerConfig` — top-level `profiler` object (spec §3.10). */
export interface ProfilerConfig {
    enabled:      boolean;
    overlay_mode: OverlayMode;
    max_queries:  number;
}

// ---- top-level profile ---------------------------------------------------

/**
 * `PipelineProfileDef` — the whole profile (spec §3). This is the unit the
 * editor reads from / writes to one `<lowercased-name>.profile.json` file.
 */
/** Editor-only UI state. Pictor の serializer は unknown key を skip するので
 *  profile.json に書き戻しても本体ロジックには影響しない。 Ergo の editor
 *  だけが読み書きし、 node 位置などの UI 状態を round-trip させる。 */
export interface ProfileEditorMeta {
    /** GraphView の node 位置 (pass_name → {x, y})。 未指定の pass は
     *  hierarchical layout の自動配置にフォールバック。 */
    nodePositions?: Record<string, { x: number; y: number }>;
}

export interface PipelineProfileDef {
    version:                number;
    profile_name:           string;
    rendering_path:         RenderingPath;
    max_lights:             number;
    msaa_samples:           number;
    gpu_driven_enabled:     boolean;
    compute_update_enabled: boolean;
    /** Phase 3 (schema v2): named attachments. Empty array on v1 profiles —
     *  the loader fills in the built-in defaults (scene_hdr_color / scene_depth
     *  / swapchain) when this array is empty. */
    attachments:            AttachmentDef[];
    render_passes:          RenderPassDef[];
    post_process:           PostProcessDef[];
    shadow:                 ShadowConfig;
    gi:                     GIConfig;
    memory:                 MemoryConfig;
    gpu_driven:             GPUDrivenConfig;
    update:                 UpdateConfig;
    profiler:               ProfilerConfig;
    /** Phase 4 editor extension — 任意の UI 状態。 Pictor 側は無視する。 */
    _editor?:               ProfileEditorMeta;
}

// ---- defaults (mirror PipelineProfileDef C++ defaults, spec §3) ----------

export const DEFAULT_RENDER_PASS: RenderPassDef = {
    pass_name:        "",
    pass_type:        "OPAQUE",
    shader_override:  "none",
    render_targets:   [],
    input_textures:   [],
    sort_mode:        "FRONT_TO_BACK",
    filter_mask:      65535,
    gpu_driven_pass:  false,
    required_streams: [],
    attachment_ops:   [],
};

// Per-effect parameter defaults — mirror the C++ structs in
// `include/pictor/postprocess/postprocess_effect.h`.
export const DEFAULT_BLOOM: BloomParams = {
    threshold:      1.0,
    soft_threshold: 0.5,
    intensity:      0.8,
    radius:         5.0,
    mip_levels:     5,
    scatter:        0.7,
};

export const DEFAULT_TONE_MAPPING: ToneMappingParams = {
    op:          "ACES_FILMIC",
    exposure:    1.0,
    gamma:       2.2,
    white_point: 4.0,
    saturation:  1.0,
};

export const DEFAULT_VIGNETTE: VignetteParams = {
    intensity: 0.35,
    radius:    0.75,
    softness:  0.45,
    color:     [0, 0, 0],
};

export const DEFAULT_COLOR_GRADING: ColorGradingParams = {
    lut_path:      "",
    lut_intensity: 1.0,
    lut_size:      16,
};

export const DEFAULT_DEPTH_OF_FIELD: DepthOfFieldParams = {
    focus_distance: 10.0,
    focus_range:    5.0,
    bokeh_radius:   4.0,
    near_start:     0.0,
    near_end:       3.0,
    far_start:      15.0,
    far_end:        50.0,
    sample_count:   16,
};

export const DEFAULT_POST_PROCESS: PostProcessDef = {
    name:           "",
    enabled:        true,
    kind:           "Unknown",
    bloom:          DEFAULT_BLOOM,
    tone_mapping:   DEFAULT_TONE_MAPPING,
    vignette:       DEFAULT_VIGNETTE,
    color_grading:  DEFAULT_COLOR_GRADING,
    depth_of_field: DEFAULT_DEPTH_OF_FIELD,
};

/**
 * Map an effect name to a `PostProcessKind` (mirrors C++
 * `post_process_kind_from_name()`). Case-insensitive; accepts the canonical
 * spellings plus common aliases. Returns `"Unknown"` for unmapped names.
 */
export function postProcessKindFromName(name: string): PostProcessKind {
    const n = name.trim().toLowerCase();
    if (n === "bloom") return "Bloom";
    if (n === "tonemapping" || n === "tonemap" || n === "tone_mapping") return "ToneMapping";
    if (n === "vignette") return "Vignette";
    if (n === "colorgrading" || n === "color_grading" || n === "lut" || n === "grade") {
        return "ColorGrading";
    }
    if (n === "dof" || n === "depthoffield" || n === "depth_of_field") return "DepthOfField";
    return "Unknown";
}

export const DEFAULT_SHADOW: ShadowConfig = {
    cascade_count: 3,
    resolution:    2048,
    filter_mode:   "PCF",
};

export const DEFAULT_SHADOW_MAP: ShadowMapConfig = {
    cascade_count:              3,
    resolution:                 2048,
    depth_bias:                 0,
    normal_bias:                0,
    slope_scale_bias:           0,
    cascade_lambda:             0.5,
    max_shadow_dist:            150,
    cascade_blend_width:        0,
    filter_mode:                "PCF",
    shadow_strength:            1,
    pcss_light_size:            0.05,
    pcss_min_penumbra:          1,
    pcss_max_penumbra:          16,
    pcss_blocker_search_radius: 8,
};

export const DEFAULT_SSAO: SSAOConfig = {
    sample_count:  32,
    radius:        0.5,
    bias:          0.025,
    intensity:     1,
    falloff_start: 0,
    falloff_end:   1,
    blur_enabled:  true,
};

export const DEFAULT_GI_PROBES: GIProbeConfig = {
    grid_origin:        [0, 0, 0],
    grid_spacing:       [1, 1, 1],
    grid_x:             8,
    grid_y:             8,
    grid_z:             8,
    gi_intensity:       1,
    max_probe_distance: 10,
};

export const DEFAULT_GI: GIConfig = {
    shadow_enabled:    true,
    ssao_enabled:      true,
    gi_probes_enabled: false,
    shadow:            DEFAULT_SHADOW_MAP,
    ssao:              DEFAULT_SSAO,
    probes:            DEFAULT_GI_PROBES,
};

export const DEFAULT_GPU_MEMORY: GpuMemoryConfig = {
    mesh_pool_size:       268435456,
    ssbo_pool_size:       134217728,
    instance_buffer_size: 67108864,
    indirect_buffer_size: 16777216,
    staging_buffer_size:  67108864,
};

export const DEFAULT_MEMORY: MemoryConfig = {
    frame_allocator_size: 16777216,
    flight_count:         3,
    pool_chunk_size:      65536,
    use_large_pages:      false,
    gpu:                  DEFAULT_GPU_MEMORY,
};

export const DEFAULT_GPU_DRIVEN: GPUDrivenConfig = {
    max_triangle_count: 50000,
    min_instance_count: 32,
    workgroup_size:     256,
    two_phase_culling:  true,
    compute_update:     true,
};

export const DEFAULT_UPDATE: UpdateConfig = {
    chunk_size:         16384,
    worker_threads:     0,
    nt_store_enabled:   true,
    nt_store_threshold: 10000,
};

export const DEFAULT_PROFILER: ProfilerConfig = {
    enabled:      true,
    overlay_mode: "STANDARD",
    max_queries:  64,
};

export const DEFAULT_PROFILE: PipelineProfileDef = {
    version:                PROFILE_SCHEMA_VERSION,
    profile_name:           "",
    rendering_path:         "FORWARD_PLUS",
    max_lights:             256,
    msaa_samples:           0,
    gpu_driven_enabled:     true,
    compute_update_enabled: true,
    attachments:            [],   // filled in by normalizeProfile() / defaultAttachments()
    render_passes:          [],
    post_process:           [],
    shadow:                 DEFAULT_SHADOW,
    gi:                     DEFAULT_GI,
    memory:                 DEFAULT_MEMORY,
    gpu_driven:             DEFAULT_GPU_DRIVEN,
    update:                 DEFAULT_UPDATE,
    profiler:               DEFAULT_PROFILER,
};

// ---- helpers -------------------------------------------------------------

function deepClone<T>(v: T): T {
    return JSON.parse(JSON.stringify(v)) as T;
}

function pickEnum<T extends readonly string[]>(
    list: T, raw: unknown, fallback: T[number],
): T[number] {
    return typeof raw === "string" && (list as readonly string[]).includes(raw)
        ? (raw as T[number])
        : fallback;
}

function asNumber(raw: unknown, fallback: number): number {
    return typeof raw === "number" && Number.isFinite(raw) ? raw : fallback;
}

function asBool(raw: unknown, fallback: boolean): boolean {
    return typeof raw === "boolean" ? raw : fallback;
}

function asString(raw: unknown, fallback: string): string {
    return typeof raw === "string" ? raw : fallback;
}

function asStringArray(raw: unknown): string[] {
    return Array.isArray(raw) ? raw.filter((x): x is string => typeof x === "string") : [];
}

function asVec3(raw: unknown, fallback: [number, number, number]): [number, number, number] {
    if (Array.isArray(raw) && raw.length === 3) {
        return [
            asNumber(raw[0], fallback[0]),
            asNumber(raw[1], fallback[1]),
            asNumber(raw[2], fallback[2]),
        ];
    }
    return [...fallback];
}

function normRenderPass(raw: unknown): RenderPassDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    return {
        pass_name:        asString(r.pass_name, DEFAULT_RENDER_PASS.pass_name),
        pass_type:        pickEnum(PASS_TYPES, r.pass_type, DEFAULT_RENDER_PASS.pass_type),
        shader_override:  asString(r.shader_override, DEFAULT_RENDER_PASS.shader_override),
        render_targets:   asStringArray(r.render_targets),
        input_textures:   asStringArray(r.input_textures),
        sort_mode:        pickEnum(SORT_MODES, r.sort_mode, DEFAULT_RENDER_PASS.sort_mode),
        filter_mask:      asNumber(r.filter_mask, DEFAULT_RENDER_PASS.filter_mask),
        gpu_driven_pass:  asBool(r.gpu_driven_pass, DEFAULT_RENDER_PASS.gpu_driven_pass),
        required_streams: asStringArray(r.required_streams),
        attachment_ops:   Array.isArray(r.attachment_ops)
                              ? r.attachment_ops.map(normAttachmentOps)
                              : [],
    };
}

function normAttachmentOps(raw: unknown): AttachmentOpsDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    return {
        attachment:     asString(r.attachment, ""),
        load:           pickEnum(ATTACHMENT_LOAD_OPS,  r.load,           "CLEAR"),
        store:          pickEnum(ATTACHMENT_STORE_OPS, r.store,          "STORE"),
        initial_layout: pickEnum(IMAGE_LAYOUTS,         r.initial_layout, "UNDEFINED"),
        final_layout:   pickEnum(IMAGE_LAYOUTS,         r.final_layout,   "SHADER_READ_ONLY_OPTIMAL"),
    };
}

/** v2 attachments[] normalization — single AttachmentDef element. */
function normAttachment(raw: unknown): AttachmentDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const usageRaw = Array.isArray(r.usage) ? (r.usage as unknown[]) : [];
    const usage: AttachmentUsageFlag[] = usageRaw
        .filter((v): v is string => typeof v === "string")
        .filter((v): v is AttachmentUsageFlag =>
            (ATTACHMENT_USAGE_FLAGS as readonly string[]).includes(v));
    const out: AttachmentDef = {
        name:   asString(r.name, ""),
        kind:   pickEnum(ATTACHMENT_KINDS,   r.kind,   "COLOR"),
        format: pickEnum(ATTACHMENT_FORMATS, r.format, "R16G16B16A16_SFLOAT"),
        sizing: pickEnum(ATTACHMENT_SIZINGS, r.sizing, "SWAPCHAIN_RELATIVE"),
        usage,
    };
    if (out.sizing === "SWAPCHAIN_RELATIVE") {
        out.scale = asNumber(r.scale, 1.0);
    } else {
        out.width  = asNumber(r.width,  0);
        out.height = asNumber(r.height, 0);
    }
    if (out.kind === "DEPTH") {
        out.clear_depth   = asNumber(r.clear_depth, 1.0);
        out.clear_stencil = asNumber(r.clear_stencil, 0);
    } else if (Array.isArray(r.clear_color)) {
        const c = r.clear_color as unknown[];
        out.clear_color = [
            asNumber(c[0], 0), asNumber(c[1], 0),
            asNumber(c[2], 0), asNumber(c[3], 1),
        ];
    } else {
        out.clear_color = [0, 0, 0, 1];
    }
    return out;
}

function normBloom(raw: unknown): BloomParams {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_BLOOM;
    return {
        threshold:      asNumber(r.threshold, d.threshold),
        soft_threshold: asNumber(r.soft_threshold, d.soft_threshold),
        intensity:      asNumber(r.intensity, d.intensity),
        radius:         asNumber(r.radius, d.radius),
        mip_levels:     asNumber(r.mip_levels, d.mip_levels),
        scatter:        asNumber(r.scatter, d.scatter),
    };
}

function normToneMapping(raw: unknown): ToneMappingParams {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_TONE_MAPPING;
    return {
        op:          pickEnum(TONE_MAP_OPERATORS, r.op, d.op),
        exposure:    asNumber(r.exposure, d.exposure),
        gamma:       asNumber(r.gamma, d.gamma),
        white_point: asNumber(r.white_point, d.white_point),
        saturation:  asNumber(r.saturation, d.saturation),
    };
}

function normVignette(raw: unknown): VignetteParams {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_VIGNETTE;
    return {
        intensity: asNumber(r.intensity, d.intensity),
        radius:    asNumber(r.radius, d.radius),
        softness:  asNumber(r.softness, d.softness),
        color:     asVec3(r.color, d.color),
    };
}

function normColorGrading(raw: unknown): ColorGradingParams {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_COLOR_GRADING;
    return {
        lut_path:      asString(r.lut_path, d.lut_path),
        lut_intensity: asNumber(r.lut_intensity, d.lut_intensity),
        lut_size:      asNumber(r.lut_size, d.lut_size),
    };
}

function normDepthOfField(raw: unknown): DepthOfFieldParams {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_DEPTH_OF_FIELD;
    return {
        focus_distance: asNumber(r.focus_distance, d.focus_distance),
        focus_range:    asNumber(r.focus_range, d.focus_range),
        bokeh_radius:   asNumber(r.bokeh_radius, d.bokeh_radius),
        near_start:     asNumber(r.near_start, d.near_start),
        near_end:       asNumber(r.near_end, d.near_end),
        far_start:      asNumber(r.far_start, d.far_start),
        far_end:        asNumber(r.far_end, d.far_end),
        sample_count:   asNumber(r.sample_count, d.sample_count),
    };
}

function normPostProcess(raw: unknown): PostProcessDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const name = asString(r.name, DEFAULT_POST_PROCESS.name);
    // Explicit `kind` wins; otherwise infer from `name` (preset spelling).
    const kind = typeof r.kind === "string"
                     && (POST_PROCESS_KINDS as readonly string[]).includes(r.kind)
                     && r.kind !== "Unknown"
                 ? (r.kind as PostProcessKind)
                 : postProcessKindFromName(name);
    return {
        name,
        enabled:        asBool(r.enabled, DEFAULT_POST_PROCESS.enabled),
        kind,
        bloom:          normBloom(r.bloom),
        tone_mapping:   normToneMapping(r.tone_mapping),
        vignette:       normVignette(r.vignette),
        color_grading:  normColorGrading(r.color_grading),
        depth_of_field: normDepthOfField(r.depth_of_field),
    };
}

function normShadow(raw: unknown): ShadowConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    return {
        cascade_count: asNumber(r.cascade_count, DEFAULT_SHADOW.cascade_count),
        resolution:    asNumber(r.resolution, DEFAULT_SHADOW.resolution),
        filter_mode:   pickEnum(SHADOW_FILTER_MODES, r.filter_mode, DEFAULT_SHADOW.filter_mode),
    };
}

function normShadowMap(raw: unknown): ShadowMapConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_SHADOW_MAP;
    return {
        cascade_count:              asNumber(r.cascade_count, d.cascade_count),
        resolution:                 asNumber(r.resolution, d.resolution),
        depth_bias:                 asNumber(r.depth_bias, d.depth_bias),
        normal_bias:                asNumber(r.normal_bias, d.normal_bias),
        slope_scale_bias:           asNumber(r.slope_scale_bias, d.slope_scale_bias),
        cascade_lambda:             asNumber(r.cascade_lambda, d.cascade_lambda),
        max_shadow_dist:            asNumber(r.max_shadow_dist, d.max_shadow_dist),
        cascade_blend_width:        asNumber(r.cascade_blend_width, d.cascade_blend_width),
        filter_mode:                pickEnum(SHADOW_FILTER_MODES, r.filter_mode, d.filter_mode),
        shadow_strength:            asNumber(r.shadow_strength, d.shadow_strength),
        pcss_light_size:            asNumber(r.pcss_light_size, d.pcss_light_size),
        pcss_min_penumbra:          asNumber(r.pcss_min_penumbra, d.pcss_min_penumbra),
        pcss_max_penumbra:          asNumber(r.pcss_max_penumbra, d.pcss_max_penumbra),
        pcss_blocker_search_radius: asNumber(r.pcss_blocker_search_radius, d.pcss_blocker_search_radius),
    };
}

function normSSAO(raw: unknown): SSAOConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_SSAO;
    return {
        sample_count:  asNumber(r.sample_count, d.sample_count),
        radius:        asNumber(r.radius, d.radius),
        bias:          asNumber(r.bias, d.bias),
        intensity:     asNumber(r.intensity, d.intensity),
        falloff_start: asNumber(r.falloff_start, d.falloff_start),
        falloff_end:   asNumber(r.falloff_end, d.falloff_end),
        blur_enabled:  asBool(r.blur_enabled, d.blur_enabled),
    };
}

function normProbes(raw: unknown): GIProbeConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_GI_PROBES;
    return {
        grid_origin:        asVec3(r.grid_origin, d.grid_origin),
        grid_spacing:       asVec3(r.grid_spacing, d.grid_spacing),
        grid_x:             asNumber(r.grid_x, d.grid_x),
        grid_y:             asNumber(r.grid_y, d.grid_y),
        grid_z:             asNumber(r.grid_z, d.grid_z),
        gi_intensity:       asNumber(r.gi_intensity, d.gi_intensity),
        max_probe_distance: asNumber(r.max_probe_distance, d.max_probe_distance),
    };
}

function normGI(raw: unknown): GIConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    return {
        shadow_enabled:    asBool(r.shadow_enabled, DEFAULT_GI.shadow_enabled),
        ssao_enabled:      asBool(r.ssao_enabled, DEFAULT_GI.ssao_enabled),
        gi_probes_enabled: asBool(r.gi_probes_enabled, DEFAULT_GI.gi_probes_enabled),
        shadow:            normShadowMap(r.shadow),
        ssao:              normSSAO(r.ssao),
        probes:            normProbes(r.probes),
    };
}

function normGpuMemory(raw: unknown): GpuMemoryConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_GPU_MEMORY;
    return {
        mesh_pool_size:       asNumber(r.mesh_pool_size, d.mesh_pool_size),
        ssbo_pool_size:       asNumber(r.ssbo_pool_size, d.ssbo_pool_size),
        instance_buffer_size: asNumber(r.instance_buffer_size, d.instance_buffer_size),
        indirect_buffer_size: asNumber(r.indirect_buffer_size, d.indirect_buffer_size),
        staging_buffer_size:  asNumber(r.staging_buffer_size, d.staging_buffer_size),
    };
}

function normMemory(raw: unknown): MemoryConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_MEMORY;
    return {
        frame_allocator_size: asNumber(r.frame_allocator_size, d.frame_allocator_size),
        flight_count:         asNumber(r.flight_count, d.flight_count),
        pool_chunk_size:      asNumber(r.pool_chunk_size, d.pool_chunk_size),
        use_large_pages:      asBool(r.use_large_pages, d.use_large_pages),
        gpu:                  normGpuMemory(r.gpu),
    };
}

function normGpuDriven(raw: unknown): GPUDrivenConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_GPU_DRIVEN;
    return {
        max_triangle_count: asNumber(r.max_triangle_count, d.max_triangle_count),
        min_instance_count: asNumber(r.min_instance_count, d.min_instance_count),
        workgroup_size:     asNumber(r.workgroup_size, d.workgroup_size),
        two_phase_culling:  asBool(r.two_phase_culling, d.two_phase_culling),
        compute_update:     asBool(r.compute_update, d.compute_update),
    };
}

function normUpdate(raw: unknown): UpdateConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_UPDATE;
    return {
        chunk_size:         asNumber(r.chunk_size, d.chunk_size),
        worker_threads:     asNumber(r.worker_threads, d.worker_threads),
        nt_store_enabled:   asBool(r.nt_store_enabled, d.nt_store_enabled),
        nt_store_threshold: asNumber(r.nt_store_threshold, d.nt_store_threshold),
    };
}

function normProfiler(raw: unknown): ProfilerConfig {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    return {
        enabled:      asBool(r.enabled, DEFAULT_PROFILER.enabled),
        overlay_mode: pickEnum(OVERLAY_MODES, r.overlay_mode, DEFAULT_PROFILER.overlay_mode),
        max_queries:  asNumber(r.max_queries, DEFAULT_PROFILER.max_queries),
    };
}

/**
 * Coerce arbitrary parsed JSON into a fully-populated `PipelineProfileDef`.
 *
 * Mirrors the C++ serializer's forward-compatible behavior (spec §2 / §6):
 * unknown keys are dropped, missing keys fall back to defaults, unknown
 * enum strings fall back to defaults. The only thing that can fail is JSON
 * syntax itself (handled by the caller before this is reached).
 */
/**
 * Built-in attachment defaults — kept bit-equivalent to Pictor's
 * `default_attachments()` in `src/pipeline/attachment_def.cpp`. Used to
 * upgrade v1 profiles (no `attachments[]`) so the editor always shows
 * the canonical three nodes (HDR / depth / swapchain).
 */
export function defaultAttachments(): AttachmentDef[] {
    return [
        {
            name:   "scene_hdr_color",
            kind:   "COLOR",
            format: "R16G16B16A16_SFLOAT",
            sizing: "SWAPCHAIN_RELATIVE",
            scale:  1.0,
            usage:  ["COLOR_ATTACHMENT", "SAMPLED"],
            clear_color: [0.0, 0.0, 0.0, 1.0],
        },
        {
            name:   "scene_depth",
            kind:   "DEPTH",
            format: "D32_SFLOAT",
            sizing: "SWAPCHAIN_RELATIVE",
            scale:  1.0,
            usage:  ["DEPTH_STENCIL_ATTACHMENT"],
            clear_depth:   1.0,
            clear_stencil: 0,
        },
        {
            name:   "swapchain",
            kind:   "SWAPCHAIN_COLOR",
            format: "B8G8R8A8_SRGB",
            sizing: "SWAPCHAIN_RELATIVE",
            scale:  1.0,
            usage:  ["COLOR_ATTACHMENT"],
            clear_color: [0.0, 0.0, 0.0, 1.0],
        },
    ];
}

export function normalizeProfile(raw: unknown): PipelineProfileDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_PROFILE;
    const attachments = Array.isArray(r.attachments)
        ? r.attachments.map(normAttachment)
        : [];
    // v1 compatibility: a profile without attachments[] gets the built-in 3.
    return {
        version:                PROFILE_SCHEMA_VERSION,
        profile_name:           asString(r.profile_name, d.profile_name),
        rendering_path:         pickEnum(RENDERING_PATHS, r.rendering_path, d.rendering_path),
        max_lights:             asNumber(r.max_lights, d.max_lights),
        msaa_samples:           asNumber(r.msaa_samples, d.msaa_samples),
        gpu_driven_enabled:     asBool(r.gpu_driven_enabled, d.gpu_driven_enabled),
        compute_update_enabled: asBool(r.compute_update_enabled, d.compute_update_enabled),
        attachments:            attachments.length ? attachments : defaultAttachments(),
        render_passes:          Array.isArray(r.render_passes)
                                    ? r.render_passes.map(normRenderPass)
                                    : [],
        post_process:           Array.isArray(r.post_process)
                                    ? r.post_process.map(normPostProcess)
                                    : [],
        shadow:                 normShadow(r.shadow),
        gi:                     normGI(r.gi),
        memory:                 normMemory(r.memory),
        gpu_driven:             normGpuDriven(r.gpu_driven),
        update:                 normUpdate(r.update),
        profiler:               normProfiler(r.profiler),
        _editor:                normEditorMeta(r._editor),
    };
}

/** Editor-only `_editor` block の正規化。 不正な値はすべて drop し、 健全な
 *  形に整える。 Pictor 側は本フィールドを skip するため、 round-trip 時に
 *  残せれば充分。 */
function normEditorMeta(raw: unknown): ProfileEditorMeta | undefined {
    if (!raw || typeof raw !== "object") return undefined;
    const r = raw as Record<string, unknown>;
    const out: ProfileEditorMeta = {};
    if (r.nodePositions && typeof r.nodePositions === "object") {
        const pos: Record<string, { x: number; y: number }> = {};
        for (const [k, v] of Object.entries(r.nodePositions as Record<string, unknown>)) {
            if (v && typeof v === "object") {
                const vv = v as Record<string, unknown>;
                const x = Number(vv.x);
                const y = Number(vv.y);
                if (Number.isFinite(x) && Number.isFinite(y)) {
                    pos[k] = { x, y };
                }
            }
        }
        if (Object.keys(pos).length) out.nodePositions = pos;
    }
    return Object.keys(out).length ? out : undefined;
}

/**
 * Serialize one `PostProcessDef` to its on-disk JSON object.
 *
 * Mirrors the C++ encoder (`pipeline_profile_serializer.cpp`): emits
 * `name` / `enabled` / `kind`, then only the parameter block matching
 * `kind`. `Unknown` effects (SSAO / TAA …) carry name/enabled/kind only.
 */
export function serializePostProcess(pp: PostProcessDef): Record<string, unknown> {
    const out: Record<string, unknown> = {
        name:    pp.name,
        enabled: pp.enabled,
        kind:    pp.kind,
    };
    switch (pp.kind) {
        case "Bloom":         out.bloom          = { ...pp.bloom }; break;
        case "ToneMapping":   out.tone_mapping   = { ...pp.tone_mapping }; break;
        case "Vignette":      out.vignette       = { ...pp.vignette, color: [...pp.vignette.color] }; break;
        case "ColorGrading":  out.color_grading  = { ...pp.color_grading }; break;
        case "DepthOfField":  out.depth_of_field = { ...pp.depth_of_field }; break;
        case "Unknown":       break;
    }
    return out;
}

/**
 * Serialize a profile to the JSON text written to `*.profile.json`.
 *
 * Emits the full canonical shape (spec §3) with 2-space indentation, a
 * trailing newline, and `version` first — matching the hand-authored
 * presets in `Pictor/profiles/`. Each `post_process[]` entry carries its
 * typed `kind` + the matching effect parameter block (spec §3.4).
 */
/** v2 attachments[] one element. */
export function serializeAttachment(a: AttachmentDef): Record<string, unknown> {
    const out: Record<string, unknown> = {
        name:   a.name,
        kind:   a.kind,
        format: a.format,
        sizing: a.sizing,
        usage:  [...a.usage],
    };
    if (a.sizing === "SWAPCHAIN_RELATIVE") {
        out.scale = a.scale ?? 1.0;
    } else {
        out.width  = a.width  ?? 0;
        out.height = a.height ?? 0;
    }
    if (a.kind === "DEPTH") {
        out.clear_depth   = a.clear_depth   ?? 1.0;
        out.clear_stencil = a.clear_stencil ?? 0;
    } else if (a.clear_color) {
        out.clear_color = [...a.clear_color];
    }
    return out;
}

/** RenderPassDef one element (includes attachment_ops). */
export function serializeRenderPass(rp: RenderPassDef): Record<string, unknown> {
    const out: Record<string, unknown> = {
        pass_name:        rp.pass_name,
        pass_type:        rp.pass_type,
        shader_override:  rp.shader_override,
        render_targets:   [...rp.render_targets],
        input_textures:   [...rp.input_textures],
        sort_mode:        rp.sort_mode,
        filter_mask:      rp.filter_mask,
        gpu_driven_pass:  rp.gpu_driven_pass,
        required_streams: [...rp.required_streams],
    };
    if (rp.attachment_ops && rp.attachment_ops.length) {
        out.attachment_ops = rp.attachment_ops.map((o) => ({ ...o }));
    }
    return out;
}

export function serializeProfile(p: PipelineProfileDef): string {
    const out: Record<string, unknown> = {
        version:                PROFILE_SCHEMA_VERSION,
        profile_name:           p.profile_name,
        rendering_path:         p.rendering_path,
        max_lights:             p.max_lights,
        msaa_samples:           p.msaa_samples,
        gpu_driven_enabled:     p.gpu_driven_enabled,
        compute_update_enabled: p.compute_update_enabled,
        attachments:            p.attachments.map(serializeAttachment),
        render_passes:          p.render_passes.map(serializeRenderPass),
        post_process:           p.post_process.map(serializePostProcess),
        shadow:                 { ...p.shadow },
        gi:                     {
            shadow_enabled:    p.gi.shadow_enabled,
            ssao_enabled:      p.gi.ssao_enabled,
            gi_probes_enabled: p.gi.gi_probes_enabled,
            shadow:            { ...p.gi.shadow },
            ssao:              { ...p.gi.ssao },
            probes:            { ...p.gi.probes },
        },
        memory:                 { ...p.memory, gpu: { ...p.memory.gpu } },
        gpu_driven:             { ...p.gpu_driven },
        update:                 { ...p.update },
        profiler:               { ...p.profiler },
    };
    // Editor-only meta: 含まれていれば末尾に付ける。 Pictor 側は unknown key を
    // skip するので C++ runtime には影響しない (Phase 4 editor 機能)。
    if (p._editor && Object.keys(p._editor).length) {
        const meta: Record<string, unknown> = {};
        if (p._editor.nodePositions && Object.keys(p._editor.nodePositions).length) {
            meta.nodePositions = { ...p._editor.nodePositions };
        }
        if (Object.keys(meta).length) {
            out._editor = meta;
        }
    }
    return JSON.stringify(out, null, 2) + "\n";
}

/** `<lowercased-profile_name>.profile.json`, the loader's naming rule. */
export function profileFileName(profileName: string): string {
    const slug = profileName.trim().toLowerCase().replace(/[^a-z0-9_-]+/g, "");
    return `${slug || "untitled"}.profile.json`;
}

export { deepClone as cloneProfileValue };
