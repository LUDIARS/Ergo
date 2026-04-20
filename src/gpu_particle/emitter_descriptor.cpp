#include "ergo/gpu_particle/emitter_descriptor.h"

#include <string>

namespace ergo::gpu_particle {

namespace {
void set_err(std::string* err, const char* msg) {
    if (err) *err = msg;
}
} // namespace

bool EmitterDescriptor::validate(std::string* error) const {
    if (max_particles == 0) {
        set_err(error, "max_particles must be > 0");
        return false;
    }
    if (max_particles > 16u * 1024u * 1024u) {
        set_err(error, "max_particles exceeds 16M (would exhaust typical SSBO budgets)");
        return false;
    }
    if (duration < 0.0f) {
        set_err(error, "duration must be >= 0 (0 = infinite)");
        return false;
    }
    if (simulation_speed < 0.0f) {
        set_err(error, "simulation_speed must be >= 0");
        return false;
    }
    if (atlas_cols == 0 || atlas_rows == 0) {
        set_err(error, "atlas_cols / atlas_rows must be > 0 (use 1x1 for no atlas)");
        return false;
    }
    if (start_lifetime.mode == MinMaxCurve::Mode::Constant &&
        start_lifetime.constant_min <= 0.0f)
    {
        set_err(error, "start_lifetime must be > 0 (constant mode)");
        return false;
    }
    // Basic sanity on cone parameters.
    if (shape == EmitterShape::Cone) {
        if (cone_angle_deg < 0.0f || cone_angle_deg > 90.0f) {
            set_err(error, "cone_angle_deg must be in [0, 90]");
            return false;
        }
        if (cone_radius < 0.0f) {
            set_err(error, "cone_radius must be >= 0");
            return false;
        }
        if (cone_radius_thickness < 0.0f || cone_radius_thickness > 1.0f) {
            set_err(error, "cone_radius_thickness must be in [0, 1]");
            return false;
        }
    }
    for (const Burst& b : bursts) {
        if (b.time < 0.0f) {
            set_err(error, "Burst.time must be >= 0");
            return false;
        }
        if (b.count_max < b.count_min) {
            set_err(error, "Burst.count_max < count_min");
            return false;
        }
    }
    return true;
}

} // namespace ergo::gpu_particle
