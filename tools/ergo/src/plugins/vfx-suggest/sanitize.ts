/// Clamps and normalises raw LLM output into valid ParticleEffectConfig values.
/// Fills in fields LLM is not asked to generate (positionShape, svgSource, svgScale, version).

import {
    type ParticleEffectConfig,
    DEFAULT_EFFECT,
    SCHEMA_VERSION,
} from "../particle/schema.js";
import type { VfxPreset } from "./schema.js";

const clamp = (v: unknown, lo: number, hi: number, def: number): number => {
    const n = Number(v);
    return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : def;
};

const rgba = (
    c: unknown,
    def: [number, number, number, number],
): [number, number, number, number] => {
    if (!Array.isArray(c) || c.length < 4) return def;
    return [
        clamp(c[0], 0, 1, def[0]),
        clamp(c[1], 0, 1, def[1]),
        clamp(c[2], 0, 1, def[2]),
        clamp(c[3], 0, 1, def[3]),
    ];
};

function sanitizeConfig(raw: unknown): ParticleEffectConfig {
    const d  = DEFAULT_EFFECT;
    const c  = (raw && typeof raw === "object") ? raw as Record<string, any> : {};
    const em = (c.emission  && typeof c.emission  === "object") ? c.emission  as Record<string, any> : {};
    const in_ = (c.initial  && typeof c.initial  === "object") ? c.initial   as Record<string, any> : {};
    const ol  = (c.overLife && typeof c.overLife  === "object") ? c.overLife  as Record<string, any> : {};
    const fo  = (c.forces   && typeof c.forces    === "object") ? c.forces    as Record<string, any> : {};
    const re  = (c.render   && typeof c.render    === "object") ? c.render    as Record<string, any> : {};
    const grav = Array.isArray(fo.gravity) ? fo.gravity : d.forces.gravity;

    return {
        version: SCHEMA_VERSION,
        name: typeof c.name === "string" && c.name ? c.name : d.name,
        emission: {
            rate:     clamp(em.rate,     1, 400,  d.emission.rate),
            maxAlive: clamp(em.maxAlive, 1, 2000, d.emission.maxAlive),
        },
        initial: {
            positionShape:          "disc",
            svgSource:              "",
            svgScale:               1,
            positionRadius:         clamp(in_.positionRadius,         0,   256, d.initial.positionRadius),
            velocityAngleDeg:       clamp(in_.velocityAngleDeg,       0,   360, d.initial.velocityAngleDeg),
            velocityAngleSpreadDeg: clamp(in_.velocityAngleSpreadDeg, 0,   360, d.initial.velocityAngleSpreadDeg),
            speedMin:               clamp(in_.speedMin,               0,   600, d.initial.speedMin),
            speedMax:               clamp(in_.speedMax,               0,   600, d.initial.speedMax),
            lifetimeMin:            clamp(in_.lifetimeMin,            0.05, 6,  d.initial.lifetimeMin),
            lifetimeMax:            clamp(in_.lifetimeMax,            0.05, 6,  d.initial.lifetimeMax),
            size:                   clamp(in_.size,                   0,   64,  d.initial.size),
            color:                  rgba(in_.color, d.initial.color),
        },
        overLife: {
            sizeStart:       clamp(ol.sizeStart,       0, 8, 1),
            sizeEnd:         clamp(ol.sizeEnd,         0, 8, 0),
            colorStart:      rgba(ol.colorStart, d.overLife.colorStart),
            colorEnd:        rgba(ol.colorEnd,   d.overLife.colorEnd),
            velocityDamping: clamp(ol.velocityDamping, 0, 1, d.overLife.velocityDamping),
        },
        forces: {
            gravity: [
                clamp(grav[0], -600, 600, d.forces.gravity[0]),
                clamp(grav[1], -600, 600, d.forces.gravity[1]),
            ],
        },
        render: {
            blend: re.blend === "alpha"  ? "alpha"  : "additive",
            shape: re.shape === "square" ? "square" : "circle",
        },
    };
}

export function sanitizePresets(raw: unknown[]): VfxPreset[] {
    if (!Array.isArray(raw)) return [];
    return raw.slice(0, 4).map((item, i) => {
        const obj = (item && typeof item === "object") ? item as Record<string, any> : {};
        return {
            name:      typeof obj.name      === "string" ? obj.name      : `preset-${i + 1}`,
            rationale: typeof obj.rationale === "string" ? obj.rationale : "",
            config:    sanitizeConfig(obj.config ?? obj),
        };
    });
}

/// Extract JSON from LLM text that may include markdown fences or surrounding prose.
export function extractJson(text: string): string {
    const fenced = text.match(/```(?:json)?\s*([\s\S]*?)```/);
    if (fenced) return fenced[1]!.trim();
    const first = text.indexOf("{");
    const last  = text.lastIndexOf("}");
    if (first !== -1 && last > first) return text.slice(first, last + 1);
    return text.trim();
}
