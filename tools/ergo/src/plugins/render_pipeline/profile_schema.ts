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
/// Distinct from the *scanner* snapshot (`render_pipeline.json`): the
/// scanner describes how Pictor's hard-coded Vulkan code (系統B) actually
/// renders today; this schema describes the declarative profile data
/// (系統A) — i.e. how you *want* it to render. The two are deliberately
/// kept separate in both API and UI.

export const PROFILE_SCHEMA_VERSION = 1;

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

// ---- per-element structs -------------------------------------------------

/** `RenderPassDef` — one entry of `render_passes[]` (spec §3.2). */
export interface RenderPassDef {
    pass_name:        string;
    pass_type:        PassType;
    /** `"none"` or `"handle:<u32>"`. Scheduler does not consume it yet. */
    shader_override:  string;
    render_targets:   string[];
    input_textures:   string[];
    sort_mode:        SortMode;
    filter_mask:      number;
    gpu_driven_pass:  boolean;
    required_streams: string[];
}

/**
 * `PostProcessDef` — one entry of `post_process[]` (spec §3.3 / §3.4).
 *
 * `PostProcessDef` only has `name` / `enabled` in C++. Any extra keys are
 * silently dropped by the C++ serializer (spec §3.4, forward-compat), so
 * `params` is an editor-only round-trip bag that this plugin preserves on
 * disk but the engine ignores.
 */
export interface PostProcessDef {
    name:    string;
    enabled: boolean;
    /** Editor-only effect parameters. Not consumed by C++ (spec §3.4). */
    params?: Record<string, number | string | boolean>;
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
export interface PipelineProfileDef {
    version:                number;
    profile_name:           string;
    rendering_path:         RenderingPath;
    max_lights:             number;
    msaa_samples:           number;
    gpu_driven_enabled:     boolean;
    compute_update_enabled: boolean;
    render_passes:          RenderPassDef[];
    post_process:           PostProcessDef[];
    shadow:                 ShadowConfig;
    gi:                     GIConfig;
    memory:                 MemoryConfig;
    gpu_driven:             GPUDrivenConfig;
    update:                 UpdateConfig;
    profiler:               ProfilerConfig;
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
};

export const DEFAULT_POST_PROCESS: PostProcessDef = {
    name:    "",
    enabled: true,
};

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
    };
}

function normPostProcess(raw: unknown): PostProcessDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const out: PostProcessDef = {
        name:    asString(r.name, DEFAULT_POST_PROCESS.name),
        enabled: asBool(r.enabled, DEFAULT_POST_PROCESS.enabled),
    };
    // Keep any extra keys as editor-only params (spec §3.4 round-trip).
    const params: Record<string, number | string | boolean> = {};
    for (const [k, v] of Object.entries(r)) {
        if (k === "name" || k === "enabled") continue;
        if (typeof v === "number" || typeof v === "string" || typeof v === "boolean") {
            params[k] = v;
        }
    }
    if (Object.keys(params).length) out.params = params;
    return out;
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
export function normalizeProfile(raw: unknown): PipelineProfileDef {
    const r = (raw && typeof raw === "object" ? raw : {}) as Record<string, unknown>;
    const d = DEFAULT_PROFILE;
    return {
        version:                PROFILE_SCHEMA_VERSION,
        profile_name:           asString(r.profile_name, d.profile_name),
        rendering_path:         pickEnum(RENDERING_PATHS, r.rendering_path, d.rendering_path),
        max_lights:             asNumber(r.max_lights, d.max_lights),
        msaa_samples:           asNumber(r.msaa_samples, d.msaa_samples),
        gpu_driven_enabled:     asBool(r.gpu_driven_enabled, d.gpu_driven_enabled),
        compute_update_enabled: asBool(r.compute_update_enabled, d.compute_update_enabled),
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
    };
}

/**
 * Serialize a profile to the JSON text written to `*.profile.json`.
 *
 * Emits the full canonical shape (spec §3) with 2-space indentation, a
 * trailing newline, and `version` first — matching the hand-authored
 * presets in `Pictor/profiles/`. `post_process[].params` is spread back
 * onto each effect object so editor-only params round-trip on disk
 * (C++ drops them on read — spec §3.4).
 */
export function serializeProfile(p: PipelineProfileDef): string {
    const out: Record<string, unknown> = {
        version:                PROFILE_SCHEMA_VERSION,
        profile_name:           p.profile_name,
        rendering_path:         p.rendering_path,
        max_lights:             p.max_lights,
        msaa_samples:           p.msaa_samples,
        gpu_driven_enabled:     p.gpu_driven_enabled,
        compute_update_enabled: p.compute_update_enabled,
        render_passes:          p.render_passes.map((rp) => ({ ...rp })),
        post_process:           p.post_process.map((pp) => {
            const { params, ...rest } = pp;
            return params ? { ...rest, ...params } : { ...rest };
        }),
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
    return JSON.stringify(out, null, 2) + "\n";
}

/** `<lowercased-profile_name>.profile.json`, the loader's naming rule. */
export function profileFileName(profileName: string): string {
    const slug = profileName.trim().toLowerCase().replace(/[^a-z0-9_-]+/g, "");
    return `${slug || "untitled"}.profile.json`;
}

export { deepClone as cloneProfileValue };
