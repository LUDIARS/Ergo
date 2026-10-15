/// Runs one generation: optional direction advice, then breed().
/// The advisor is asked only when a direction is given; any advisor failure
/// is reported in `direction.status` and the children are bred without bias.

import { toConfig } from "./genome.js";
import { breed, type Child } from "./breed.js";
import { toBias, type DirectionWeights } from "./direction.js";
import type { DirectionAdvisor } from "./advisor.js";
import type { GenerationRequest } from "./request.js";

export type DirectionStatus = "none" | "applied" | "unavailable" | "failed";

export interface GenerationResult {
    children:   Child[];
    seed:       number;
    generation: number;
    direction: {
        status:   DirectionStatus;
        weights?: DirectionWeights;
        reason?:  string;
    };
}

export async function runGeneration(
    req: GenerationRequest,
    advisor: DirectionAdvisor,
): Promise<GenerationResult> {
    let bias: number[] | null = null;
    let direction: GenerationResult["direction"] = { status: "none" };

    if (req.direction) {
        const h = advisor.health();
        if (!h.ok) {
            direction = { status: "unavailable", reason: h.reason ?? "advisor not ready" };
        } else {
            try {
                const weights = await advisor.suggest(req.direction, toConfig(req.parents[0]));
                bias = toBias(weights);
                direction = { status: "applied", weights };
            } catch (e) {
                direction = { status: "failed", reason: String(e) };
            }
        }
    }

    const generation = req.generation + 1;
    const children = breed({
        parents: req.parents,
        count: req.count,
        strength: req.strength,
        seed: req.seed,
        bias,
        generation,
    });
    return { children, seed: req.seed, generation, direction };
}
