/// Validation of POST /vfx-breed/api/generation bodies.
///
/// Body: { parents?: Config[], selected?: number[], direction?: string,
///         strength?: number, seed?: number, count?: number, generation?: number }
/// - `selected` picks parents by index; omitted -> every parent.
/// - no parent at all -> the default effect is the single parent.

import { DEFAULT_EFFECT } from "../particle/schema.js";
import { freshSeed } from "./rng.js";

export const COUNT_MIN = 4;
export const COUNT_MAX = 16;
export const COUNT_DEFAULT = 8;
export const STRENGTH_DEFAULT = 0.15;
export const DIRECTION_MAX_CHARS = 500;

export interface GenerationRequest {
    parents:    unknown[];
    direction:  string;
    strength:   number;
    seed:       number;
    count:      number;
    generation: number;
}

export type ParseResult =
    | { ok: true;  req: GenerationRequest }
    | { ok: false; err: string };

const num = (v: unknown): number | null => {
    const n = typeof v === "number" ? v : typeof v === "string" && v.trim() ? Number(v) : NaN;
    return Number.isFinite(n) ? n : null;
};

export function parseGenerationRequest(body: unknown, seedSource: () => number = freshSeed): ParseResult {
    if (!body || typeof body !== "object" || Array.isArray(body)) {
        return { ok: false, err: "request body must be a JSON object" };
    }
    const b = body as Record<string, unknown>;

    const all = b["parents"] ?? [];
    if (!Array.isArray(all)) return { ok: false, err: "parents must be an array of configs" };
    if (all.some((p) => !p || typeof p !== "object" || Array.isArray(p))) {
        return { ok: false, err: "every parent must be a config object" };
    }

    let parents: unknown[] = all;
    if (b["selected"] != null) {
        const sel = b["selected"];
        if (!Array.isArray(sel) || sel.some((i) => !Number.isInteger(i) || i < 0 || i >= all.length)) {
            return { ok: false, err: "selected must be indices into parents" };
        }
        parents = [...new Set(sel as number[])].map((i) => all[i]);
    }
    if (parents.length === 0) parents = [DEFAULT_EFFECT];

    const count = Math.round(num(b["count"]) ?? COUNT_DEFAULT);
    const strength = num(b["strength"]) ?? STRENGTH_DEFAULT;
    const seed = num(b["seed"]);
    const generation = Math.max(0, Math.floor(num(b["generation"]) ?? 0));
    const direction = typeof b["direction"] === "string" ? b["direction"].trim().slice(0, DIRECTION_MAX_CHARS) : "";

    return {
        ok: true,
        req: {
            parents,
            direction,
            strength: Math.min(1, Math.max(0, strength)),
            seed: seed == null ? seedSource() : Math.floor(seed) >>> 0,
            count: Math.min(COUNT_MAX, Math.max(COUNT_MIN, count)),
            generation,
        },
    };
}
