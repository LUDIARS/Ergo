#include "ergo/gpu_particle/particle_system.h"

#include "ergo/gpu_particle/particle_state.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// Per-emitter UBO layout shared with the compute shaders. The offsets
// here must match `shaders/particle_*.comp`'s `layout(std140) uniform
// EmitterBlock` definition.
//
// The per-frame block is small (curves are packed into dedicated 1D
// textures), but we keep the spawn/shape parameters here so the shader
// only binds one UBO per dispatch.
// ---------------------------------------------------------------------------
struct EmitterUBO {
    float    delta_time;
    float    emitter_time;      // seconds since play
    float    simulation_speed;
    float    _pad0;

    uint32_t to_spawn;          // number of particles to emit this frame
    uint32_t max_particles;
    uint32_t simulation_space;  // 0 = world, 1 = local
    uint32_t random_seed;

    float    emitter_world[16]; // 4x4 row-major (translation in row 3)

    // Main
    float    start_color[4];

    // Shape
    uint32_t shape;
    float    cone_angle_rad;
    float    cone_radius;
    float    cone_radius_thickness;
    float    sphere_radius;
    float    _pad1[3];
    float    box_extents[3];
    float    _pad2;
    float    shape_position[3];
    float    _pad3;

    // Physics
    float    gravity[3];
    float    gravity_modifier;
    float    linear_damping;
    float    wind_direction[3];
    float    wind_strength;
    float    wind_turbulence;
    float    _pad4[2];

    // Start ranges (already pre-sampled to min/max on CPU for speed)
    float    start_lifetime_min;
    float    start_lifetime_max;
    float    start_speed_min;
    float    start_speed_max;
    float    start_size_min;
    float    start_size_max;
    float    start_rotation_min;
    float    start_rotation_max;
};

// Forward-declared; implementation below.
static float random01(uint32_t& state);

// ---------------------------------------------------------------------------
// Per-emitter runtime state.
// ---------------------------------------------------------------------------
struct EmitterRuntime {
    EmitterDescriptor desc;
    EmitterState      state = EmitterState::Stopped;
    Float4x4          world;
    Vec3f             last_world_position{0.f, 0.f, 0.f};
    float             time_since_play  = 0.0f;
    float             spawn_accumulator = 0.0f;
    float             distance_accumulator = 0.0f;
    uint32_t          random_state = 0xA7E5D2F1u;

    // GPU-side resources (only used when a backend is active).
    BufferHandle particle_buffer;
    BufferHandle counters_buffer;
    BufferHandle instance_buffer;
    BufferHandle ubo_buffer;
    TextureHandle curve_tex_size;
    TextureHandle curve_tex_rotation;
    TextureHandle curve_tex_color_r;
    TextureHandle curve_tex_color_g;
    TextureHandle curve_tex_color_b;
    TextureHandle curve_tex_color_a;
    TextureHandle curve_tex_atlas_frame;
    TextureHandle curve_tex_velocity_x;
    TextureHandle curve_tex_velocity_y;
    TextureHandle curve_tex_velocity_z;

    // CPU-fallback state (used when no backend is attached).
    std::vector<Particle>    cpu_particles;
    std::vector<InstanceData> cpu_instances;
    uint32_t                 cpu_alive = 0;
};

// ---------------------------------------------------------------------------
// ParticleSystem::Impl
// ---------------------------------------------------------------------------
struct ParticleSystem::Impl {
    IGpuBackend*             backend  = nullptr;
    ParticleSystemConfig     cfg;
    bool                     initialized = false;
    bool                     gpu_mode    = false;

    ShaderHandle spawn_shader;
    ShaderHandle update_shader;

    EmitterHandle next_handle = 1;
    std::unordered_map<EmitterHandle, EmitterRuntime> emitters;

    // Cached RNG for auto-seeding emitters.
    std::mt19937 seeder{std::random_device{}()};

    bool create_gpu_resources(EmitterRuntime& rt);
    void destroy_gpu_resources(EmitterRuntime& rt);
    void bake_curves(EmitterRuntime& rt);

    uint32_t compute_spawn_count(EmitterRuntime& rt, float dt);

    void dispatch_frame(EmitterRuntime& rt, float dt);
    void cpu_simulate   (EmitterRuntime& rt, float dt);
};

// ---------------------------------------------------------------------------
// Constructors / shutdown
// ---------------------------------------------------------------------------

ParticleSystem::ParticleSystem()  : impl_(std::make_unique<Impl>()) {}
ParticleSystem::~ParticleSystem() { shutdown(); }

bool ParticleSystem::initialize(IGpuBackend* backend, const ParticleSystemConfig& cfg) {
    if (impl_->initialized) return true;
    impl_->backend = backend;
    impl_->cfg     = cfg;

    impl_->gpu_mode = false;
    if (backend != nullptr &&
        cfg.spawn_shader_spirv && cfg.spawn_shader_spirv_size > 0 &&
        cfg.update_shader_spirv && cfg.update_shader_spirv_size > 0)
    {
        impl_->spawn_shader = backend->create_compute_shader(
            cfg.spawn_shader_spirv, cfg.spawn_shader_spirv_size, "main", "particle_spawn");
        impl_->update_shader = backend->create_compute_shader(
            cfg.update_shader_spirv, cfg.update_shader_spirv_size, "main", "particle_update");
        impl_->gpu_mode = static_cast<bool>(impl_->spawn_shader) && static_cast<bool>(impl_->update_shader);
    }

    if (!impl_->gpu_mode && !cfg.allow_cpu_fallback) {
        return false;
    }

    impl_->initialized = true;
    return true;
}

void ParticleSystem::shutdown() {
    if (!impl_) return;
    if (impl_->initialized) {
        for (auto& [_, rt] : impl_->emitters) {
            impl_->destroy_gpu_resources(rt);
        }
        impl_->emitters.clear();
        if (impl_->backend) {
            if (impl_->spawn_shader)  impl_->backend->destroy_shader(impl_->spawn_shader);
            if (impl_->update_shader) impl_->backend->destroy_shader(impl_->update_shader);
        }
        impl_->spawn_shader  = {};
        impl_->update_shader = {};
        impl_->initialized = false;
    }
}

bool ParticleSystem::is_initialized() const {
    return impl_ && impl_->initialized;
}

// ---------------------------------------------------------------------------
// Emitter registration
// ---------------------------------------------------------------------------

EmitterHandle ParticleSystem::create_emitter(const EmitterDescriptor& desc,
                                             std::string* error)
{
    if (!impl_->initialized) {
        if (error) *error = "ParticleSystem not initialized";
        return INVALID_EMITTER;
    }
    if (!desc.validate(error)) return INVALID_EMITTER;

    EmitterRuntime rt;
    rt.desc  = desc;
    rt.state = EmitterState::Stopped;
    rt.world = Float4x4::identity();
    rt.random_state = desc.random_seed != 0
                      ? desc.random_seed
                      : static_cast<uint32_t>(impl_->seeder());

    if (impl_->gpu_mode) {
        if (!impl_->create_gpu_resources(rt)) {
            if (error) *error = "GPU resource allocation failed";
            return INVALID_EMITTER;
        }
        impl_->bake_curves(rt);
    } else {
        rt.cpu_particles.resize(desc.max_particles);
        rt.cpu_instances.resize(desc.max_particles);
    }

    const EmitterHandle handle = impl_->next_handle++;
    impl_->emitters.emplace(handle, std::move(rt));
    return handle;
}

void ParticleSystem::destroy_emitter(EmitterHandle h) {
    auto it = impl_->emitters.find(h);
    if (it == impl_->emitters.end()) return;
    impl_->destroy_gpu_resources(it->second);
    impl_->emitters.erase(it);
}

// ---------------------------------------------------------------------------
// Runtime control
// ---------------------------------------------------------------------------

// Free-function lookups require access to the private Impl type; we keep
// the logic inline at each call site instead.

void ParticleSystem::play(EmitterHandle h) {
    if (auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) {
        rt->state = EmitterState::Playing;
        rt->time_since_play = 0.0f;
        rt->spawn_accumulator = 0.0f;
        rt->distance_accumulator = 0.0f;
    }
}

void ParticleSystem::pause(EmitterHandle h) {
    if (auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) {
        if (rt->state == EmitterState::Playing) rt->state = EmitterState::Paused;
    }
}

void ParticleSystem::stop(EmitterHandle h, bool clear_particles) {
    auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })();
    if (!rt) return;
    rt->state = EmitterState::Stopped;
    if (clear_particles) {
        if (impl_->gpu_mode) {
            if (impl_->backend && rt->particle_buffer) {
                impl_->backend->zero_buffer(rt->particle_buffer);
            }
            if (impl_->backend && rt->counters_buffer) {
                impl_->backend->zero_buffer(rt->counters_buffer);
            }
        } else {
            rt->cpu_alive = 0;
            for (auto& p : rt->cpu_particles) p.lifetime_remaining = 0.0f;
        }
    }
}

void ParticleSystem::set_world_transform(EmitterHandle h, const Float4x4& world) {
    if (auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) {
        rt->last_world_position = {rt->world.m[3][0], rt->world.m[3][1], rt->world.m[3][2]};
        rt->world = world;
    }
}

void ParticleSystem::set_emitter_position(EmitterHandle h, Vec3f p) {
    if (auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) {
        rt->last_world_position = {rt->world.m[3][0], rt->world.m[3][1], rt->world.m[3][2]};
        rt->world.set_translation(p.x, p.y, p.z);
    }
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void ParticleSystem::update(float delta_time) {
    if (!impl_->initialized) return;
    for (auto& [_, rt] : impl_->emitters) {
        if (rt.state != EmitterState::Playing) continue;
        const float dt = delta_time * rt.desc.simulation_speed;
        rt.time_since_play += dt;

        // Loop / duration handling.
        if (rt.desc.duration > 0.0f && !rt.desc.loop) {
            if (rt.time_since_play >= rt.desc.duration) {
                rt.state = EmitterState::Stopped;
                continue;
            }
        }

        if (impl_->gpu_mode) {
            impl_->dispatch_frame(rt, dt);
        } else {
            impl_->cpu_simulate(rt, dt);
        }
    }
}

// ---------------------------------------------------------------------------
// Read-back API
// ---------------------------------------------------------------------------

BufferHandle ParticleSystem::get_instance_buffer(EmitterHandle h) const {
    if (const auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) return rt->instance_buffer;
    return {};
}

uint32_t ParticleSystem::get_live_particle_count(EmitterHandle h) const {
    const auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })();
    if (!rt) return 0;
    if (impl_->gpu_mode && impl_->backend && rt->counters_buffer) {
        ParticleCounters c{};
        impl_->backend->readback_buffer(rt->counters_buffer, 0, &c, sizeof(c));
        return c.alive_count;
    }
    return rt->cpu_alive;
}

BufferHandle ParticleSystem::get_counters_buffer(EmitterHandle h) const {
    if (const auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) return rt->counters_buffer;
    return {};
}

const EmitterDescriptor* ParticleSystem::get_descriptor(EmitterHandle h) const {
    if (const auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) return &rt->desc;
    return nullptr;
}

EmitterState ParticleSystem::get_state(EmitterHandle h) const {
    if (const auto* rt = ([this, h]() -> auto* { auto it = impl_->emitters.find(h); return it == impl_->emitters.end() ? nullptr : &it->second; })()) return rt->state;
    return EmitterState::Stopped;
}

// ---------------------------------------------------------------------------
// Impl helpers
// ---------------------------------------------------------------------------

bool ParticleSystem::Impl::create_gpu_resources(EmitterRuntime& rt) {
    if (!backend) return false;
    const uint32_t cap = rt.desc.max_particles;
    rt.particle_buffer  = backend->create_buffer(cap * sizeof(Particle),     BufferUsage::Storage, "particles");
    rt.instance_buffer  = backend->create_buffer(cap * sizeof(InstanceData), BufferUsage::Storage, "instances");
    rt.counters_buffer  = backend->create_buffer(sizeof(ParticleCounters),   BufferUsage::Storage, "counters");
    rt.ubo_buffer       = backend->create_buffer(sizeof(EmitterUBO),         BufferUsage::Uniform, "emitter_ubo");
    if (!rt.particle_buffer || !rt.instance_buffer || !rt.counters_buffer || !rt.ubo_buffer) {
        destroy_gpu_resources(rt);
        return false;
    }
    backend->zero_buffer(rt.particle_buffer);
    backend->zero_buffer(rt.counters_buffer);
    return true;
}

void ParticleSystem::Impl::destroy_gpu_resources(EmitterRuntime& rt) {
    if (!backend) return;
    auto kill = [&](BufferHandle& b) { if (b) { backend->destroy_buffer(b); b = {}; } };
    kill(rt.particle_buffer);
    kill(rt.instance_buffer);
    kill(rt.counters_buffer);
    kill(rt.ubo_buffer);
    auto kill_tex = [&](TextureHandle& t) { if (t) { backend->destroy_texture(t); t = {}; } };
    kill_tex(rt.curve_tex_size);
    kill_tex(rt.curve_tex_rotation);
    kill_tex(rt.curve_tex_color_r);
    kill_tex(rt.curve_tex_color_g);
    kill_tex(rt.curve_tex_color_b);
    kill_tex(rt.curve_tex_color_a);
    kill_tex(rt.curve_tex_atlas_frame);
    kill_tex(rt.curve_tex_velocity_x);
    kill_tex(rt.curve_tex_velocity_y);
    kill_tex(rt.curve_tex_velocity_z);
}

void ParticleSystem::Impl::bake_curves(EmitterRuntime& rt) {
    if (!backend) return;
    auto upload = [&](const Curve& curve, TextureHandle& out, const char* name) {
        Curve::BakedArray samples{};
        if (!curve.empty()) curve.bake(samples);
        else                samples.fill(1.0f);
        out = backend->create_texture_1d(Curve::kSampleCount, TextureFormat::R32F, name);
        backend->upload_texture_1d(out, samples.data(), samples.size() * sizeof(float));
    };
    auto bake_minmax = [&](const MinMaxCurve& mm, TextureHandle& out, const char* name) {
        Curve c;
        if (mm.mode == MinMaxCurve::Mode::Curve || mm.mode == MinMaxCurve::Mode::TwoCurves) {
            c = mm.curve_min;
        } else {
            c = Curve::constant(mm.constant_min);
        }
        upload(c, out, name);
    };
    bake_minmax(rt.desc.size_over_lifetime,     rt.curve_tex_size,          "curve_size");
    bake_minmax(rt.desc.rotation_over_lifetime, rt.curve_tex_rotation,      "curve_rotation");
    upload(rt.desc.color_r_over_lifetime,       rt.curve_tex_color_r,       "curve_color_r");
    upload(rt.desc.color_g_over_lifetime,       rt.curve_tex_color_g,       "curve_color_g");
    upload(rt.desc.color_b_over_lifetime,       rt.curve_tex_color_b,       "curve_color_b");
    upload(rt.desc.color_a_over_lifetime,       rt.curve_tex_color_a,       "curve_color_a");
    upload(rt.desc.atlas_frame_over_lifetime,   rt.curve_tex_atlas_frame,   "curve_atlas");
    upload(rt.desc.velocity_over_lifetime_x,    rt.curve_tex_velocity_x,    "curve_vel_x");
    upload(rt.desc.velocity_over_lifetime_y,    rt.curve_tex_velocity_y,    "curve_vel_y");
    upload(rt.desc.velocity_over_lifetime_z,    rt.curve_tex_velocity_z,    "curve_vel_z");
}

uint32_t ParticleSystem::Impl::compute_spawn_count(EmitterRuntime& rt, float dt) {
    const float t_norm = rt.desc.duration > 0.0f
        ? std::fmod(rt.time_since_play, rt.desc.duration) / rt.desc.duration
        : 0.0f;

    // Rate over time.
    const float rand_a = random01(rt.random_state);
    const float rate = rt.desc.rate_over_time.evaluate(t_norm, rand_a);
    rt.spawn_accumulator += rate * dt;

    // Rate over distance.
    const Vec3f curr_pos{rt.world.m[3][0], rt.world.m[3][1], rt.world.m[3][2]};
    const float dx = curr_pos.x - rt.last_world_position.x;
    const float dy = curr_pos.y - rt.last_world_position.y;
    const float dz = curr_pos.z - rt.last_world_position.z;
    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    const float rand_b = random01(rt.random_state);
    const float rate_d = rt.desc.rate_over_distance.evaluate(t_norm, rand_b);
    rt.distance_accumulator += rate_d * dist;
    rt.last_world_position = curr_pos;

    uint32_t count = static_cast<uint32_t>(rt.spawn_accumulator) +
                     static_cast<uint32_t>(rt.distance_accumulator);
    rt.spawn_accumulator    -= static_cast<float>(static_cast<uint32_t>(rt.spawn_accumulator));
    rt.distance_accumulator -= static_cast<float>(static_cast<uint32_t>(rt.distance_accumulator));

    // Bursts: simple one-shot support (cycles/interval TODO).
    for (const Burst& b : rt.desc.bursts) {
        const float prev = rt.time_since_play - dt;
        if (b.time > prev && b.time <= rt.time_since_play) {
            const uint32_t span = b.count_max - b.count_min;
            const uint32_t burst = span == 0 ? b.count_min
                : b.count_min + static_cast<uint32_t>(random01(rt.random_state) * (span + 1));
            count += burst;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// GPU dispatch path — records bindings and asks the backend to run the
// spawn then update compute kernels. The actual GLSL lives in
// shaders/particle_spawn.comp and shaders/particle_update.comp.
// ---------------------------------------------------------------------------
void ParticleSystem::Impl::dispatch_frame(EmitterRuntime& rt, float dt) {
    if (!backend) return;

    const uint32_t to_spawn = std::min(rt.desc.max_particles,
                                       compute_spawn_count(rt, dt));

    // Pack per-frame UBO.
    EmitterUBO ubo{};
    ubo.delta_time       = dt;
    ubo.emitter_time     = rt.time_since_play;
    ubo.simulation_speed = rt.desc.simulation_speed;
    ubo.to_spawn         = to_spawn;
    ubo.max_particles    = rt.desc.max_particles;
    ubo.simulation_space = static_cast<uint32_t>(rt.desc.simulation_space);
    ubo.random_seed      = rt.random_state;
    std::memcpy(ubo.emitter_world, &rt.world.m[0][0], sizeof(ubo.emitter_world));
    ubo.start_color[0] = rt.desc.start_color.x;
    ubo.start_color[1] = rt.desc.start_color.y;
    ubo.start_color[2] = rt.desc.start_color.z;
    ubo.start_color[3] = rt.desc.start_color.w;
    ubo.shape          = static_cast<uint32_t>(rt.desc.shape);
    ubo.cone_angle_rad = rt.desc.cone_angle_deg * (3.14159265f / 180.f);
    ubo.cone_radius    = rt.desc.cone_radius;
    ubo.cone_radius_thickness = rt.desc.cone_radius_thickness;
    ubo.sphere_radius  = rt.desc.sphere_radius;
    ubo.box_extents[0] = rt.desc.box_extents.x;
    ubo.box_extents[1] = rt.desc.box_extents.y;
    ubo.box_extents[2] = rt.desc.box_extents.z;
    ubo.shape_position[0] = rt.desc.shape_position.x;
    ubo.shape_position[1] = rt.desc.shape_position.y;
    ubo.shape_position[2] = rt.desc.shape_position.z;
    ubo.gravity[0] = rt.desc.gravity.x;
    ubo.gravity[1] = rt.desc.gravity.y;
    ubo.gravity[2] = rt.desc.gravity.z;
    ubo.gravity_modifier = rt.desc.gravity_modifier;
    ubo.linear_damping   = rt.desc.linear_damping;
    ubo.wind_direction[0] = rt.desc.wind.direction.x;
    ubo.wind_direction[1] = rt.desc.wind.direction.y;
    ubo.wind_direction[2] = rt.desc.wind.direction.z;
    ubo.wind_strength     = rt.desc.wind.enabled ? rt.desc.wind.main_strength : 0.0f;
    ubo.wind_turbulence   = rt.desc.wind.turbulence;
    ubo.start_lifetime_min = rt.desc.start_lifetime.constant_min;
    ubo.start_lifetime_max = rt.desc.start_lifetime.constant_max;
    ubo.start_speed_min    = rt.desc.start_speed.constant_min;
    ubo.start_speed_max    = rt.desc.start_speed.constant_max;
    ubo.start_size_min     = rt.desc.start_size.constant_min;
    ubo.start_size_max     = rt.desc.start_size.constant_max;
    ubo.start_rotation_min = rt.desc.start_rotation.constant_min;
    ubo.start_rotation_max = rt.desc.start_rotation.constant_max;
    backend->upload_buffer(rt.ubo_buffer, 0, &ubo, sizeof(ubo));

    // Bindings shared by spawn + update:
    //   0: Particle SSBO (rw)
    //   1: Counters SSBO (rw)
    //   2: Instance SSBO (w)
    //   3: EmitterUBO
    //   4..: curve textures (size / rotation / color_rgba / atlas / vel_xyz)
    ResourceBinding bindings[] = {
        {0, ResourceKind::StorageBuffer, rt.particle_buffer, {}, 0, 0},
        {1, ResourceKind::StorageBuffer, rt.counters_buffer, {}, 0, 0},
        {2, ResourceKind::StorageBuffer, rt.instance_buffer, {}, 0, 0},
        {3, ResourceKind::UniformBuffer, rt.ubo_buffer,      {}, 0, 0},
        {4, ResourceKind::SampledTexture1D, {}, rt.curve_tex_size,        0, 0},
        {5, ResourceKind::SampledTexture1D, {}, rt.curve_tex_rotation,    0, 0},
        {6, ResourceKind::SampledTexture1D, {}, rt.curve_tex_color_r,     0, 0},
        {7, ResourceKind::SampledTexture1D, {}, rt.curve_tex_color_g,     0, 0},
        {8, ResourceKind::SampledTexture1D, {}, rt.curve_tex_color_b,     0, 0},
        {9, ResourceKind::SampledTexture1D, {}, rt.curve_tex_color_a,     0, 0},
        {10, ResourceKind::SampledTexture1D, {}, rt.curve_tex_atlas_frame, 0, 0},
        {11, ResourceKind::SampledTexture1D, {}, rt.curve_tex_velocity_x,  0, 0},
        {12, ResourceKind::SampledTexture1D, {}, rt.curve_tex_velocity_y,  0, 0},
        {13, ResourceKind::SampledTexture1D, {}, rt.curve_tex_velocity_z,  0, 0},
    };
    constexpr uint32_t binding_count = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));

    const uint32_t wg = cfg.workgroup_size > 0 ? cfg.workgroup_size : 64;

    // Spawn dispatch — workgroups cover `to_spawn` particles.
    if (to_spawn > 0) {
        const uint32_t groups_x = (to_spawn + wg - 1) / wg;
        backend->dispatch_compute(spawn_shader, {groups_x, 1, 1}, bindings, binding_count);
        backend->barrier_buffer(rt.particle_buffer);
        backend->barrier_buffer(rt.counters_buffer);
    }

    // Update dispatch — cover the whole particle capacity; dead particles
    // short-circuit in the shader.
    {
        const uint32_t groups_x = (rt.desc.max_particles + wg - 1) / wg;
        backend->dispatch_compute(update_shader, {groups_x, 1, 1}, bindings, binding_count);
        backend->barrier_buffer(rt.instance_buffer);
        backend->barrier_buffer(rt.counters_buffer);
    }
}

// ---------------------------------------------------------------------------
// CPU fallback — a small, correctness-first simulation used when no GPU
// backend is attached (e.g. in the test suite). Feature coverage is
// deliberately limited to what's needed to exercise the descriptor
// validation + state machine.
// ---------------------------------------------------------------------------

static float random01(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
}

static void spawn_one_cpu(EmitterRuntime& rt, Particle& p) {
    const float r_life = random01(rt.random_state);
    const float r_spd  = random01(rt.random_state);
    const float r_sz   = random01(rt.random_state);
    const float r_rot  = random01(rt.random_state);

    const float life  = rt.desc.start_lifetime.evaluate(0.0f, r_life);
    const float speed = rt.desc.start_speed   .evaluate(0.0f, r_spd);
    const float size  = rt.desc.start_size    .evaluate(0.0f, r_sz);
    const float rot   = rt.desc.start_rotation.evaluate(0.0f, r_rot);

    // Origin at emitter world position (shape-specific variants TODO).
    p.position = {rt.world.m[3][0], rt.world.m[3][1], rt.world.m[3][2]};
    p.velocity = {0.0f, speed, 0.0f};   // cone approximation: along +Y
    p.color    = rt.desc.start_color;
    p.size     = size;
    p.rotation = rot;
    p.atlas_frame = 0.0f;
    p.seed = random01(rt.random_state);
    p.lifetime_initial = life;
    p.lifetime_remaining = life;
}

void ParticleSystem::Impl::cpu_simulate(EmitterRuntime& rt, float dt) {
    // Integrate live particles.
    uint32_t alive = 0;
    for (uint32_t i = 0; i < rt.desc.max_particles; ++i) {
        Particle& p = rt.cpu_particles[i];
        if (p.lifetime_remaining <= 0.0f) continue;

        // Gravity + damping.
        p.velocity.x += rt.desc.gravity.x * rt.desc.gravity_modifier * dt;
        p.velocity.y += rt.desc.gravity.y * rt.desc.gravity_modifier * dt;
        p.velocity.z += rt.desc.gravity.z * rt.desc.gravity_modifier * dt;
        const float damp = 1.0f - std::min(rt.desc.linear_damping * dt, 1.0f);
        p.velocity.x *= damp; p.velocity.y *= damp; p.velocity.z *= damp;

        // Position integrate.
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;

        p.lifetime_remaining -= dt;
        if (p.lifetime_remaining <= 0.0f) continue;

        rt.cpu_instances[alive] = InstanceData{
            p.position, p.size, p.color, p.rotation, p.atlas_frame, 0.f, 0.f};
        ++alive;
    }
    rt.cpu_alive = alive;

    // Spawn new particles.
    uint32_t to_spawn = compute_spawn_count(rt, dt);
    for (uint32_t i = 0; i < rt.desc.max_particles && to_spawn > 0; ++i) {
        Particle& p = rt.cpu_particles[i];
        if (p.lifetime_remaining > 0.0f) continue;
        spawn_one_cpu(rt, p);
        --to_spawn;
    }
}

} // namespace ergo::gpu_particle
