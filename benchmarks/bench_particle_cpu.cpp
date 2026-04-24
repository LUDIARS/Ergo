// Benchmark the ergo_gpu_particle CPU-fallback path. Spawns a saturated
// emitter and times one ParticleSystem::update() call at 60 Hz. This is the
// baseline regression target for the CPU fallback's shape sampling and
// per-frame integration cost.

#include "ergo/gpu_particle/particle_system.h"

#include "bench_common.h"

#include <vector>

using namespace ergo::gpu_particle;

namespace {
ParticleSystemConfig cpu_config() {
    ParticleSystemConfig c;
    c.spawn_shader_spirv       = nullptr;
    c.spawn_shader_spirv_size  = 0;
    c.update_shader_spirv      = nullptr;
    c.update_shader_spirv_size = 0;
    c.allow_cpu_fallback       = true;
    return c;
}

void saturate(ParticleSystem& sys, uint32_t capacity) {
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = capacity;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(10.0f);
    d.start_speed    = MinMaxCurve::constant(1.0f);
    d.rate_over_time = MinMaxCurve::constant(static_cast<float>(capacity) * 120.0f);
    d.shape          = EmitterShape::Sphere;
    d.sphere_radius  = 2.0f;
    d.gravity        = {0.0f, -2.0f, 0.0f};
    d.gravity_modifier = 1.0f;
    d.random_seed    = 0xB00B0012u;

    EmitterHandle h = sys.create_emitter(d);
    sys.play(h);
    // Saturate the buffer.
    for (int i = 0; i < 30; ++i) sys.update(1.0f / 60.0f);
}
} // namespace

int main() {
    std::vector<ergo_bench::Result> results;

    for (uint32_t cap : {1024u, 10000u, 50000u}) {
        ParticleSystem sys;
        saturate(sys, cap);
        // 1 frame = 1 update call. Iterate many times to average.
        const uint64_t iters = cap >= 50000u ? 200 : (cap >= 10000u ? 1000 : 5000);
        char label[96];
        std::snprintf(label, sizeof(label), "particle_cpu.update(%u)", cap);
        results.push_back(ergo_bench::run_bench_per_item(
            label, iters, cap, [&] { sys.update(1.0f / 60.0f); }));
    }

    ergo_bench::print_markdown(results);
    return 0;
}
