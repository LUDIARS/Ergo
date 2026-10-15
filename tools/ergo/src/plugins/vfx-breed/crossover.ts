/// Uniform crossover: every gene (discrete ones included) comes from either
/// parent with equal probability.

import type { Genome } from "./genome.js";
import type { Rng } from "./rng.js";

export function crossover(a: Genome, b: Genome, rng: Rng): Genome {
    const genes = a.genes.map((g, i) => (rng.next() < 0.5 ? g : (b.genes[i] ?? g)));
    const blend = rng.next() < 0.5 ? a.blend : b.blend;
    const shape = rng.next() < 0.5 ? a.shape : b.shape;
    return { genes, blend, shape };
}
