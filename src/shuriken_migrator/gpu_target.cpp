/// gpu_target.cpp
///
/// ShurikenSource → ergo::gpu_particle::EmitterDescriptor 変換、
/// および EmitterDescriptor の JSON シリアライズ。
/// 2D path (migrator.cpp の ConvertToErgo / MigrateFileToJson) と並列に
/// 存在し、 同じ Shuriken 抽出結果からより広い Shuriken 機能を gpu_particle
/// にマップする。

#include "ergo/shuriken_migrator/migrator.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>

namespace ergo::shuriken_migrator {

namespace {

using GpuMinMax = ergo::gpu_particle::MinMaxCurve;

GpuMinMax to_gpu_curve(const MinMaxCurve& c) {
    float lo, hi; c.Range(lo, hi);
    if (std::fabs(hi - lo) < 1e-6f) return GpuMinMax::constant(lo);
    return GpuMinMax::two_constants(lo, hi);
}

ergo::gpu_particle::EmitterShape to_gpu_shape(int shape_type) {
    using S = ergo::gpu_particle::EmitterShape;
    switch (shape_type) {
        case 0: return S::Sphere;
        case 4: return S::Hemisphere;
        case 5: return S::Cone;
        case 6: return S::Box;
        case 7: return S::Circle;
        case 8: return S::Edge;
        default: return S::Cone;
    }
}

ergo::gpu_particle::Curve linear_curve(float v0, float v1) {
    return ergo::gpu_particle::Curve::linear(v0, v1);
}

uint32_t curve_count_to_u32(float v) {
    if (v <= 0.0f) return 0u;
    return static_cast<uint32_t>(std::lround(v));
}

} // namespace

ergo::gpu_particle::EmitterDescriptor
ConvertToGpuEmitter(const ShurikenSource& src, MigrationReport& report) {
    ergo::gpu_particle::EmitterDescriptor d{};
    d.name           = src.name;
    d.duration       = src.main.duration;
    d.loop           = src.main.looping;
    d.start_lifetime = to_gpu_curve(src.main.startLifetime);
    d.start_speed    = to_gpu_curve(src.main.startSpeed);
    d.start_size     = to_gpu_curve(src.main.startSize);
    d.start_color    = ergo::gpu_particle::Vec4f{
        src.main.startColor[0], src.main.startColor[1],
        src.main.startColor[2], src.main.startColor[3]};
    d.max_particles  = static_cast<uint32_t>(src.main.maxNumParticles);
    d.simulation_space = (src.main.simulationSpace == 1)
        ? ergo::gpu_particle::SimulationSpace::World
        : ergo::gpu_particle::SimulationSpace::Local;
    {
        float lo, hi;
        src.main.gravityModifier.Range(lo, hi);
        d.gravity_modifier = (lo + hi) * 0.5f;
    }

    d.rate_over_time = to_gpu_curve(src.emission.rateOverTime);
    for (const auto& sb : src.emission.bursts) {
        float lo = 0.0f, hi = 0.0f;
        sb.count.Range(lo, hi);
        if (hi < lo) std::swap(lo, hi);
        ergo::gpu_particle::Burst b;
        b.time        = std::max(0.0f, sb.time);
        b.count_min   = curve_count_to_u32(lo);
        b.count_max   = curve_count_to_u32(hi);
        b.cycles      = sb.cycles == 0 ? 1u : sb.cycles;
        b.interval    = sb.interval <= 0.0f ? 0.01f : sb.interval;
        b.probability = std::clamp(sb.probability, 0.0f, 1.0f);
        if (b.count_max > 0) d.bursts.push_back(b);
    }

    d.shape          = to_gpu_shape(src.shape.shapeType);
    d.sphere_radius  = src.shape.radius;
    d.cone_radius    = src.shape.radius;
    d.cone_angle_deg = src.shape.angle;
    d.shape_scale    = ergo::gpu_particle::Vec3f{
        src.shape.scale[0], src.shape.scale[1], src.shape.scale[2]};

    if (src.velocityOverLifetime.enabled) {
        float lo, hi;
        src.velocityOverLifetime.x.Range(lo, hi);
        d.velocity_over_lifetime_x = linear_curve(lo, hi);
        src.velocityOverLifetime.y.Range(lo, hi);
        d.velocity_over_lifetime_y = linear_curve(lo, hi);
        src.velocityOverLifetime.z.Range(lo, hi);
        d.velocity_over_lifetime_z = linear_curve(lo, hi);
    }

    if (src.sizeOverLifetime.enabled) {
        d.size_over_lifetime = to_gpu_curve(src.sizeOverLifetime.size);
    }

    if (src.colorOverLifetime.enabled) {
        const auto& g = src.colorOverLifetime.gradient;
        d.color_r_over_lifetime = linear_curve(g.startColor[0], g.endColor[0]);
        d.color_g_over_lifetime = linear_curve(g.startColor[1], g.endColor[1]);
        d.color_b_over_lifetime = linear_curve(g.startColor[2], g.endColor[2]);
        d.color_a_over_lifetime = linear_curve(g.startColor[3], g.endColor[3]);
    }

    if (src.limitVelocityOverLifetime.enabled) {
        d.limit_velocity.enabled     = true;
        d.limit_velocity.speed_limit = to_gpu_curve(src.limitVelocityOverLifetime.speed);
        d.limit_velocity.dampen      = src.limitVelocityOverLifetime.dampen;
        d.limit_velocity.drag        = src.limitVelocityOverLifetime.drag;
    }

    if (src.noise.enabled) {
        d.noise.enabled           = true;
        d.noise.strength          = src.noise.strength;
        d.noise.frequency         = src.noise.frequency;
        d.noise.octaves           = static_cast<uint32_t>(std::max(1, src.noise.octaves));
        d.noise.octave_multiplier = src.noise.octave_multiplier;
        d.noise.octave_scale      = src.noise.octave_scale;
        d.noise.separate_axes     = src.noise.separate_axes;
        d.noise.scroll_speed      = src.noise.scroll_speed;
    }

    if (src.trails.enabled) {
        d.trail.enabled        = true;
        d.trail.trail_lifetime = GpuMinMax::constant(src.trails.lifetime);
        d.trail.trail_width    = GpuMinMax::constant(src.trails.width);
        d.trail.color_start    = ergo::gpu_particle::Vec4f{
            src.trails.color_start[0], src.trails.color_start[1],
            src.trails.color_start[2], src.trails.color_start[3]};
        d.trail.color_end      = ergo::gpu_particle::Vec4f{
            src.trails.color_end[0], src.trails.color_end[1],
            src.trails.color_end[2], src.trails.color_end[3]};
        d.trail.min_vertex_distance     = src.trails.min_vertex_distance;
        d.trail.inherit_particle_color  = src.trails.inherit_particle_color;
        d.trail.die_with_particle       = src.trails.die_with_particle;
    }

    for (const auto& se : src.subEmitters) {
        ergo::gpu_particle::SubEmitter sub;
        sub.event = (se.event == 0) ? ergo::gpu_particle::SubEmitterEvent::Birth
                   : (se.event == 2) ? ergo::gpu_particle::SubEmitterEvent::Collision
                                     : ergo::gpu_particle::SubEmitterEvent::Death;
        sub.emitter_index = static_cast<uint32_t>(se.emitter_index);
        sub.probability   = se.probability;
        d.sub_emitters.push_back(sub);
    }

    if (src.textureSheet.enabled) {
        d.atlas_cols = static_cast<uint32_t>(std::max(1, src.textureSheet.num_tiles_x));
        d.atlas_rows = static_cast<uint32_t>(std::max(1, src.textureSheet.num_tiles_y));
        d.texture_sheet.enabled    = true;
        d.texture_sheet.fps        = src.textureSheet.fps;
        d.texture_sheet.row_count  = d.atlas_rows;
        d.texture_sheet.random_row = src.textureSheet.random_row;
        d.texture_sheet.cycles     = static_cast<uint32_t>(std::max(1, src.textureSheet.cycles));
        switch (src.textureSheet.time_mode) {
            case 1: d.texture_sheet.time_mode = ergo::gpu_particle::AtlasTimeMode::Speed; break;
            case 2: d.texture_sheet.time_mode = ergo::gpu_particle::AtlasTimeMode::FPS;   break;
            default: d.texture_sheet.time_mode = ergo::gpu_particle::AtlasTimeMode::Lifetime;
        }
    }

    std::string err;
    if (!d.validate(&err)) report.Warn("emitter validation: " + err);
    return d;
}

bool ConvertTreeToGpuEmitters(const ShurikenNode& root,
                              std::vector<GpuEmitterNode>& out,
                              MigrationReport& report) {
    std::function<void(const ShurikenNode&, int)> rec =
        [&](const ShurikenNode& n, int parent) {
            const int myIdx = static_cast<int>(out.size());
            GpuEmitterNode g;
            g.name = n.name;
            for (int i = 0; i < 3; ++i) {
                g.local_position[i] = n.transform.local_position[i];
                g.local_scale[i]    = n.transform.local_scale[i];
            }
            for (int i = 0; i < 4; ++i)
                g.local_rotation[i] = n.transform.local_rotation[i];
            g.parent_index = parent;
            g.has_emitter  = n.has_emitter;
            if (n.has_emitter) {
                g.emitter = ConvertToGpuEmitter(n.emitter, report);
                ++report.convertedSystems;
            }
            out.push_back(std::move(g));
            for (const auto& c : n.children) rec(c, myIdx);
        };
    rec(root, -1);
    return true;
}

namespace {
void append_kv(std::ostringstream& os, const char* key, float v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":" << v;
}
void append_kv(std::ostringstream& os, const char* key, int v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":" << v;
}
void append_kv(std::ostringstream& os, const char* key, bool v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":" << (v ? "true" : "false");
}
void append_kv_str(std::ostringstream& os, const char* key, const std::string& v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":\"" << v << "\"";
}
void append_minmax(std::ostringstream& os, const char* key, const GpuMinMax& v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":{";
    os << "\"mode\":" << static_cast<int>(v.mode)
       << ",\"min\":" << v.constant_min
       << ",\"max\":" << v.constant_max << "}";
}
void append_vec3(std::ostringstream& os, const char* key,
                 const ergo::gpu_particle::Vec3f& v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":[" << v.x << "," << v.y << "," << v.z << "]";
}
void append_vec4(std::ostringstream& os, const char* key,
                 const ergo::gpu_particle::Vec4f& v, bool& first) {
    if (!first) os << ","; first = false;
    os << "\"" << key << "\":[" << v.x << "," << v.y << "," << v.z << "," << v.w << "]";
}
} // namespace

std::string EmitterDescriptorToJson(const ergo::gpu_particle::EmitterDescriptor& d) {
    std::ostringstream os;
    os << "{";
    bool f = true;
    append_kv_str(os, "name", d.name, f);
    append_kv(os, "duration", d.duration, f);
    append_kv(os, "loop", d.loop, f);
    append_minmax(os, "start_lifetime", d.start_lifetime, f);
    append_minmax(os, "start_speed", d.start_speed, f);
    append_minmax(os, "start_size", d.start_size, f);
    append_vec4(os, "start_color", d.start_color, f);
    append_kv(os, "max_particles", static_cast<int>(d.max_particles), f);
    append_kv(os, "simulation_space", static_cast<int>(d.simulation_space), f);
    append_kv(os, "gravity_modifier", d.gravity_modifier, f);

    append_minmax(os, "rate_over_time", d.rate_over_time, f);
    if (!d.bursts.empty()) {
        if (!f) os << ","; f = false;
        os << "\"bursts\":[";
        for (size_t i = 0; i < d.bursts.size(); ++i) {
            if (i) os << ",";
            const auto& b = d.bursts[i];
            os << "{\"time\":" << b.time
               << ",\"count_min\":" << b.count_min
               << ",\"count_max\":" << b.count_max
               << ",\"cycles\":" << b.cycles
               << ",\"interval\":" << b.interval
               << ",\"probability\":" << b.probability
               << "}";
        }
        os << "]";
    }
    append_kv(os, "shape", static_cast<int>(d.shape), f);
    append_kv(os, "sphere_radius", d.sphere_radius, f);
    append_kv(os, "cone_radius", d.cone_radius, f);
    append_kv(os, "cone_angle_deg", d.cone_angle_deg, f);
    append_vec3(os, "shape_scale", d.shape_scale, f);
    append_minmax(os, "size_over_lifetime", d.size_over_lifetime, f);

    if (d.limit_velocity.enabled) {
        if (!f) os << ","; f = false;
        os << "\"limit_velocity\":{\"enabled\":true,\"dampen\":" << d.limit_velocity.dampen
           << ",\"drag\":" << d.limit_velocity.drag << "}";
    }
    if (d.noise.enabled) {
        if (!f) os << ","; f = false;
        os << "\"noise\":{\"enabled\":true,\"strength\":" << d.noise.strength
           << ",\"frequency\":" << d.noise.frequency
           << ",\"octaves\":" << d.noise.octaves
           << ",\"scroll_speed\":" << d.noise.scroll_speed << "}";
    }
    if (d.trail.enabled) {
        if (!f) os << ","; f = false;
        os << "\"trail\":{\"enabled\":true";
        bool tf = false;
        append_minmax(os, "lifetime", d.trail.trail_lifetime, tf);
        append_minmax(os, "width",    d.trail.trail_width,    tf);
        os << "}";
    }
    if (!d.sub_emitters.empty()) {
        if (!f) os << ","; f = false;
        os << "\"sub_emitters\":[";
        for (size_t i = 0; i < d.sub_emitters.size(); ++i) {
            if (i) os << ",";
            const auto& se = d.sub_emitters[i];
            os << "{\"event\":" << static_cast<int>(se.event)
               << ",\"emitter_index\":" << se.emitter_index
               << ",\"probability\":" << se.probability << "}";
        }
        os << "]";
    }
    if (d.texture_sheet.enabled) {
        if (!f) os << ","; f = false;
        os << "\"texture_sheet\":{\"enabled\":true"
           << ",\"fps\":" << d.texture_sheet.fps
           << ",\"row_random\":" << (d.texture_sheet.random_row ? "true" : "false")
           << ",\"time_mode\":" << static_cast<int>(d.texture_sheet.time_mode) << "}";
    }
    os << "}";
    return os.str();
}

std::string MigrateFileToGpuJson(const std::string& path, MigrationReport& report) {
    std::ifstream f(path);
    if (!f) {
        report.Warn("file open failed: " + path);
        return {};
    }
    std::stringstream ss; ss << f.rdbuf();
    std::vector<ShurikenSource> systems;
    if (!ParsePrefabYaml(ss.str(), systems, report)) return {};

    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < systems.size(); ++i) {
        if (i) out << ",";
        const auto desc = ConvertToGpuEmitter(systems[i], report);
        out << EmitterDescriptorToJson(desc);
        ++report.convertedSystems;
    }
    out << "]";
    return out.str();
}

std::string MigrateFileToGpuTreeJson(const std::string& path, MigrationReport& report) {
    std::ifstream f(path);
    if (!f) {
        report.Warn("file open failed: " + path);
        return {};
    }
    std::stringstream ss; ss << f.rdbuf();
    ShurikenNode root;
    if (!ParsePrefabTree(ss.str(), root, report)) return {};

    std::vector<GpuEmitterNode> nodes;
    ConvertTreeToGpuEmitters(root, nodes, report);

    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i) out << ",";
        const auto& nd = nodes[i];
        out << "{\"name\":\"" << nd.name << "\""
            << ",\"parent\":" << nd.parent_index
            << ",\"local_position\":[" << nd.local_position[0] << ","
            << nd.local_position[1] << "," << nd.local_position[2] << "]"
            << ",\"local_rotation\":[" << nd.local_rotation[0] << ","
            << nd.local_rotation[1] << "," << nd.local_rotation[2] << ","
            << nd.local_rotation[3] << "]"
            << ",\"local_scale\":[" << nd.local_scale[0] << ","
            << nd.local_scale[1] << "," << nd.local_scale[2] << "]"
            << ",\"has_emitter\":" << (nd.has_emitter ? "true" : "false");
        if (nd.has_emitter)
            out << ",\"emitter\":" << EmitterDescriptorToJson(nd.emitter);
        out << "}";
    }
    out << "]";
    return out.str();
}

} // namespace ergo::shuriken_migrator
