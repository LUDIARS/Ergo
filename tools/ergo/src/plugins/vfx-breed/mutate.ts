/// Gaussian mutation in the normalised gene space.
///
/// Each gene moves by `strength * (BIAS_GAIN * bias + N(0, 1))`: `bias`
/// (from the direction advisor, -1..1 per gene) shifts the mean, `strength`
/// scales both the shift and the noise. Discrete genes flip with a small
/// probability that grows with strength.

import { GENES } from "./genes.js";
import { fold, type Genome } from "./genome.js";
import type { Rng } from "./rng.js";

export const BIAS_GAIN = 1;
export const FLIP_PER_STRENGTH = 0.5;
export const MAX_FLIP = 0.25;

export function flipProbability(strength: number): number {
    return Math.min(MAX_FLIP, Math.max(0, strength) * FLIP_PER_STRENGTH);
}

export function mutate(
    genome: Genome,
    strength: number,
    bias: readonly number[] | null,
    rng: Rng,
): Genome {
    const genes = GENES.map((spec, i) => {
        const shift = BIAS_GAIN * (bias?.[i] ?? 0);
        const noise = rng.gaussian();
        return fold((genome.genes[i] ?? 0) + strength * (shift + noise), spec.wrap);
    });
    const p = flipProbability(strength);
    const blend = rng.next() < p ? (genome.blend === "additive" ? "alpha" : "additive") : genome.blend;
    const shape = rng.next() < p ? (genome.shape === "circle" ? "square" : "circle") : genome.shape;
    return { genes, blend, shape };
}
