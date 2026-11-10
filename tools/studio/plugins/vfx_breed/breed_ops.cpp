#include "breed_ops.h"

#include "breed_genes.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ergo::studio::plugins::vfx_breed {
namespace {

/// Strip a trailing "-g<digits>-<digits>".
std::string base_name(const JsonValue& cfg) {
    const JsonValue* n = field(cfg, "name");
    std::string s = n && n->is_string() && !n->s.empty() ? n->s : std::string("effect");
    auto digits_back = [&](std::size_t end) {   // start index of the digit run ending at `end`
        std::size_t i = end;
        while (i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1]))) --i;
        return i;
    };
    const std::size_t d2 = digits_back(s.size());
    if (d2 < s.size() && d2 > 0 && s[d2 - 1] == '-') {
        const std::size_t d1 = digits_back(d2 - 1);
        if (d1 < d2 - 1 && d1 >= 2 && s[d1 - 1] == 'g' && s[d1 - 2] == '-') s = s.substr(0, d1 - 2);
    }
    return s.empty() ? std::string("effect") : s;
}

} // namespace

double flip_probability(double strength) { return std::min(kMaxFlip, std::max(0.0, strength) * kFlipPerStrength); }

Genome crossover(const Genome& a, const Genome& b, BreedRng& rng) {
    Genome out;
    for (std::size_t i = 0; i < a.genes.size(); ++i) {
        const bool from_a = rng.next() < 0.5;
        out.genes.push_back(from_a || i >= b.genes.size() ? a.genes[i] : b.genes[i]);
    }
    out.blend = rng.next() < 0.5 ? a.blend : b.blend;
    out.shape = rng.next() < 0.5 ? a.shape : b.shape;
    return out;
}

Genome mutate(const Genome& g, double strength, const std::vector<double>* bias, BreedRng& rng) {
    Genome out;
    const auto& spec = genes();
    for (std::size_t i = 0; i < spec.size(); ++i) {
        const double shift = kBiasGain * (bias && i < bias->size() ? (*bias)[i] : 0);
        const double noise = rng.gaussian();
        out.genes.push_back(fold((i < g.genes.size() ? g.genes[i] : 0) + strength * (shift + noise), spec[i].wrap));
    }
    const double p = flip_probability(strength);
    out.blend = rng.next() < p ? (g.blend == "additive" ? "alpha" : "additive") : g.blend;
    out.shape = rng.next() < p ? (g.shape == "circle" ? "square" : "circle") : g.shape;
    return out;
}

std::vector<Child> breed(const BreedInput& in) {
    if (in.parents.empty()) throw std::invalid_argument("breed needs at least one parent");
    std::vector<JsonValue> parents;
    std::vector<Genome>    genomes;
    for (const auto& p : in.parents) {
        parents.push_back(to_config(p));
        genomes.push_back(encode(p));
    }
    BreedRng rng(in.seed);
    const uint32_t n = static_cast<uint32_t>(parents.size());
    const std::vector<double>* bias = in.bias ? &*in.bias : nullptr;

    std::vector<Child> out;
    for (int i = 0; i < in.count; ++i) {
        uint32_t a = 0;
        Genome   genome = genomes[0];
        std::vector<int> from{0};
        if (n >= 2) {
            a = rng.below(n);
            const uint32_t b = (a + 1 + rng.below(n - 1)) % n;
            genome = crossover(genomes[a], genomes[b], rng);
            from   = {static_cast<int>(a), static_cast<int>(b)};
        }
        const Genome mutated = mutate(genome, in.strength, bias, rng);
        const std::string name = base_name(parents[a]) + "-g" + std::to_string(in.generation) + "-" + std::to_string(i + 1);
        out.push_back({decode(mutated, in.parents[a], name), from});
    }
    return out;
}

} // namespace ergo::studio::plugins::vfx_breed
