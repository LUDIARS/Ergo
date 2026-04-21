/// Dummy plug — no-op implementation of every ergo_world_time symbol.
/// Used by hosts that want to disable the feature at link time without
/// changing call sites.

#include "ergo/world_time/world_time.h"

namespace ergo::world_time {

struct Engine::Impl {};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(nullptr) {}
Engine::~Engine() {}

void Engine::hit_stop(float)                              {}
void Engine::hit_slow(float, float, float, float, Ease)   {}
void Engine::force_stop()                                 {}

float Engine::update(float real_dt) { return real_dt < 0.0f ? 0.0f : real_dt; }

void Engine::register_target(ITimeScaleTarget*)   {}
void Engine::unregister_target(ITimeScaleTarget*) {}

Phase       Engine::current_phase()      const { return Phase::None; }
float       Engine::current_time_scale() const { return 1.0f; }
bool        Engine::is_hit_stop()        const { return false; }
bool        Engine::is_hit_slow()        const { return false; }
std::size_t Engine::target_count()       const { return 0; }

} // namespace ergo::world_time
