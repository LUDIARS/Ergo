/// Direction hint -> per-gene bias.
///
/// The LLM never writes config values. It only answers which genes to push
/// and which way, as weights in -1..1; this module builds that question and
/// turns the answer into a bias vector aligned with GENES.

import type { ParticleEffectConfig } from "../particle/schema.js";
import { extractJson } from "../vfx-suggest/sanitize.js";
import { GENES, geneIndex } from "./genes.js";

export type DirectionWeights = Record<string, number>;

export function buildDirectionPrompt(direction: string, current: ParticleEffectConfig): string {
    const genes = GENES.map((g) => `- ${g.id}: ${g.label}`).join("\n");
    return [
        "You steer an interactive evolution of a 2D particle effect.",
        "The user looked at the current effect and wrote how the next generation should change.",
        "Answer which genes to push and in which direction. Do NOT output config values.",
        "",
        "Genes (each is normalised to 0..1 over its range):",
        genes,
        "",
        "Current effect (JSON):",
        JSON.stringify(current),
        "",
        `User direction: ${direction}`,
        "",
        'Reply with JSON only: {"weights": {"<gene id>": <number in -1..1>, ...}}.',
        "Positive raises the gene, negative lowers it. Include only the genes that matter (usually 2-6).",
    ].join("\n");
}

/// Parse the LLM's reply. Accepts {"weights": {...}} or a bare {...}.
export function parseWeights(text: string): DirectionWeights {
    const data = JSON.parse(extractJson(text)) as unknown;
    const obj = (data && typeof data === "object") ? data as Record<string, unknown> : {};
    const raw = (obj["weights"] && typeof obj["weights"] === "object")
        ? obj["weights"] as Record<string, unknown>
        : obj;
    const out: DirectionWeights = {};
    for (const [k, v] of Object.entries(raw)) {
        const n = Number(v);
        if (geneIndex(k) >= 0 && Number.isFinite(n)) out[k] = n;
    }
    return out;
}

/// Weights -> bias vector aligned with GENES. Unknown ids are dropped,
/// values are clamped to -1..1, missing genes get 0.
export function toBias(weights: DirectionWeights): number[] {
    return GENES.map((g) => {
        const n = Number(weights[g.id]);
        return Number.isFinite(n) ? Math.min(1, Math.max(-1, n)) : 0;
    });
}
