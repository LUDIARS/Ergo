#pragma once
/// Crossover, mutation and one generation of breeding (port of
/// vfx-breed/crossover.ts, mutate.ts, breed.ts). Pure and deterministic:
/// the same parents, count, strength, bias and seed give the same children.
#include "breed_genome.h"
#include "breed_rng.h"

#include <optional>
#include <vector>

namespace ergo::studio::plugins::vfx_breed {

constexpr double kBiasGain        = 1;
constexpr double kFlipPerStrength = 0.5;
constexpr double kMaxFlip         = 0.25;

double flip_probability(double strength);

/// Uniform: every gene (the discrete ones too) from either parent, 50/50.
Genome crossover(const Genome& a, const Genome& b, BreedRng& rng);

/// Each gene moves by strength * (kBiasGain * bias + N(0,1)); blend / shape
/// flip with flip_probability(strength).
Genome mutate(const Genome& g, double strength, const std::vector<double>* bias, BreedRng& rng);

struct BreedInput {
    std::vector<JsonValue>     parents;   ///< at least one (partial JSON is merged with the defaults)
    int                        count      = 8;
    double                     strength   = 0.15;
    double                     seed       = 0;
    std::optional<std::vector<double>> bias;   ///< aligned with genes(), -1..1
    int                        generation = 1;
};

struct Child {
    JsonValue        config;
    std::vector<int> parents;   ///< indices into BreedInput::parents
};

/// Children named "<base>-g<generation>-<n>" (an old "-gX-Y" suffix is
/// dropped from the base). Throws std::invalid_argument without parents.
std::vector<Child> breed(const BreedInput& in);

} // namespace ergo::studio::plugins::vfx_breed
