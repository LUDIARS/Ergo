/// VFX Suggest plugin — shared types and JSON schema.
/// ParticleEffectConfig is the single source of truth from particle/schema.ts.

import {
    type ParticleEffectConfig,
    DEFAULT_EFFECT,
    SCHEMA_VERSION,
} from "../particle/schema.js";

export type { ParticleEffectConfig };
export { DEFAULT_EFFECT, SCHEMA_VERSION };

export type GameId = "ks" | "ac" | "ul" | "generic";

export const GAME_LABELS: Record<GameId, string> = {
    ks:      "KuzuSurvivors",
    ac:      "AdventureCube",
    ul:      "UniLand",
    generic: "汎用",
};

export function isGameId(v: unknown): v is GameId {
    return v === "ks" || v === "ac" || v === "ul" || v === "generic";
}

export interface GenRequest {
    game:   GameId;
    scene:  string;
    count?: number;
}

export interface VfxPreset {
    name:      string;
    rationale: string;
    config:    ParticleEffectConfig;
}

export type GenResult =
    | { ok: true;  presets: VfxPreset[] }
    | { ok: false; err: string };

const RGBA_SCHEMA = {
    type: "array" as const,
    items: { type: "number" as const },
    minItems: 4,
    maxItems: 4,
};

/// JSON Schema for SDK structured output — matches ParticleEffectConfig nested structure.
/// LLM is not asked to output positionShape / svgSource / svgScale (sanitize fills them).
export const OUTPUT_SCHEMA = {
    type: "object",
    additionalProperties: false,
    required: ["presets"],
    properties: {
        presets: {
            type: "array",
            items: {
                type: "object",
                additionalProperties: false,
                required: ["name", "rationale", "config"],
                properties: {
                    name:      { type: "string" },
                    rationale: { type: "string" },
                    config: {
                        type: "object",
                        additionalProperties: false,
                        required: ["name", "emission", "initial", "overLife", "forces", "render"],
                        properties: {
                            name: { type: "string" },
                            emission: {
                                type: "object",
                                additionalProperties: false,
                                required: ["rate", "maxAlive"],
                                properties: {
                                    rate:     { type: "number" },
                                    maxAlive: { type: "number" },
                                },
                            },
                            initial: {
                                type: "object",
                                additionalProperties: false,
                                required: [
                                    "positionRadius", "velocityAngleDeg",
                                    "velocityAngleSpreadDeg",
                                    "speedMin", "speedMax",
                                    "lifetimeMin", "lifetimeMax",
                                    "size", "color",
                                ],
                                properties: {
                                    positionRadius:         { type: "number" },
                                    velocityAngleDeg:       { type: "number" },
                                    velocityAngleSpreadDeg: { type: "number" },
                                    speedMin:               { type: "number" },
                                    speedMax:               { type: "number" },
                                    lifetimeMin:            { type: "number" },
                                    lifetimeMax:            { type: "number" },
                                    size:                   { type: "number" },
                                    color:                  RGBA_SCHEMA,
                                },
                            },
                            overLife: {
                                type: "object",
                                additionalProperties: false,
                                required: ["sizeStart", "sizeEnd", "colorStart", "colorEnd", "velocityDamping"],
                                properties: {
                                    sizeStart:       { type: "number" },
                                    sizeEnd:         { type: "number" },
                                    colorStart:      RGBA_SCHEMA,
                                    colorEnd:        RGBA_SCHEMA,
                                    velocityDamping: { type: "number" },
                                },
                            },
                            forces: {
                                type: "object",
                                additionalProperties: false,
                                required: ["gravity"],
                                properties: {
                                    gravity: {
                                        type: "array",
                                        items: { type: "number" },
                                        minItems: 2,
                                        maxItems: 2,
                                    },
                                },
                            },
                            render: {
                                type: "object",
                                additionalProperties: false,
                                required: ["blend", "shape"],
                                properties: {
                                    blend: { type: "string", enum: ["additive", "alpha"] },
                                    shape: { type: "string", enum: ["circle", "square"] },
                                },
                            },
                        },
                    },
                },
            },
        },
    },
};
