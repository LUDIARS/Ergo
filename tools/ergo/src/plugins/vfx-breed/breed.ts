/// One generation: parents -> children. Pure and deterministic — the same
/// parents, count, strength, bias and seed always give the same children.
///
/// With 2+ parents each child is a uniform crossover of two distinct parents
/// followed by mutation; with one parent it is a mutation of that parent.
/// Non-gene fields (positionShape / svgSource / svgScale) are inherited from
/// the first parent of the pair.

import type { ParticleEffectConfig } from "../particle/schema.js";
import { encode, decode, toConfig } from "./genome.js";
import { crossover } from "./crossover.js";
import { mutate } from "./mutate.js";
import { makeRng } from "./rng.js";

export interface BreedInput {
    /** At least one parent config (partial JSON is merged with the default effect). */
    parents:    unknown[];
    count:      number;
    /** 0..1, scales the gaussian noise and the bias shift. */
    strength:   number;
    seed:       number;
    /** Per-gene mean shift (-1..1, aligned with GENES) or null for none. */
    bias:       readonly number[] | null;
    /** Number of the generation being produced (used in child names). */
    generation: number;
}

export interface Child {
    config:  ParticleEffectConfig;
    /** Indices into `parents` this child came from. */
    parents: number[];
}

const SUFFIX = /-g\d+-\d+$/;

function baseName(cfg: ParticleEffectConfig): string {
    return (cfg.name || "effect").replace(SUFFIX, "") || "effect";
}

export function breed(input: BreedInput): Child[] {
    const parents = input.parents.map(toConfig);
    if (parents.length === 0) throw new Error("breed needs at least one parent");
    const genomes = parents.map(encode);
    const rng = makeRng(input.seed);
    const n = parents.length;

    const children: Child[] = [];
    for (let i = 0; i < input.count; ++i) {
        let a = 0;
        let genome = genomes[0]!;
        const from = [0];
        if (n >= 2) {
            a = rng.int(n);
            const b = (a + 1 + rng.int(n - 1)) % n;
            genome = crossover(genomes[a]!, genomes[b]!, rng);
            from.splice(0, 1, a, b);
        }
        const mutated = mutate(genome, input.strength, input.bias, rng);
        const name = `${baseName(parents[a]!)}-g${input.generation}-${i + 1}`;
        children.push({ config: decode(mutated, parents[a], name), parents: from });
    }
    return children;
}
