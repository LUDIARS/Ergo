/// Dummy plug — provides ergo_particle symbols as no-ops.

#include "ergo/particle/effect_config.h"
#include "ergo/particle/particle_system.h"

namespace ergo::particle {

bool parse_config_json(const std::string&, ParticleEffectConfig&) { return false; }

ParticleSystem::ParticleSystem() : rng_(0) {}
void ParticleSystem::set_config(const ParticleEffectConfig&) {}
ParticleEffectConfig ParticleSystem::config() const { return {}; }
void ParticleSystem::update(float) {}
void ParticleSystem::burst(int) {}
void ParticleSystem::reset() {}
void ParticleSystem::emit_one() {}

} // namespace ergo::particle
