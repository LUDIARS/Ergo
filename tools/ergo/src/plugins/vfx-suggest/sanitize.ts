/// Clamps and normalises raw LLM output into valid ParticleEffectConfig values.
/// Fills in fields LLM is not asked to generate (positionShape, svgSource, svgScale, version).

import {
    type ParticleEffectConfig,
    DEFAULT_EFFECT,
    SCHEMA_VERSION,
} from "../particle/schema.js";
import { PARTICLE_RANGES as R, COLOR_RANGE, type ValueRange } from "../particle/ranges.js";
import type { VfxPreset } from "./schema.js";

const clamp = (v: unknown, lo: number, hi: number, def: number): number => {
    const n = Number(v);
    return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : def;
};

const within = (v: unknown, r: ValueRange, def: number): number => clamp(v, r.lo, r.hi, def);

const rgba = (
    c: unknown,
    def: [number, number, number, number],
): [number, number, number, number] => {
    if (!Array.isArray(c) || c.length < 4) return def;
    return [
        within(c[0], COLOR_RANGE, def[0]),
        within(c[1], COLOR_RANGE, def[1]),
        within(c[2], COLOR_RANGE, def[2]),
        within(c[3], COLOR_RANGE, def[3]),
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
            rate:     within(em.rate, R["emission.rate"], d.emission.rate),
            maxAlive: within(em.maxAlive, R["emission.maxAlive"], d.emission.maxAlive),
        },
        initial: {
            positionShape:          "disc",
            svgSource:              "",
            svgScale:               1,
            positionRadius:         within(in_.positionRadius, R["initial.positionRadius"], d.initial.positionRadius),
            velocityAngleDeg:       within(in_.velocityAngleDeg, R["initial.velocityAngleDeg"], d.initial.velocityAngleDeg),
            velocityAngleSpreadDeg: within(in_.velocityAngleSpreadDeg, R["initial.velocityAngleSpreadDeg"], d.initial.velocityAngleSpreadDeg),
            speedMin:               within(in_.speedMin, R["initial.speedMin"], d.initial.speedMin),
            speedMax:               within(in_.speedMax, R["initial.speedMax"], d.initial.speedMax),
            lifetimeMin:            within(in_.lifetimeMin, R["initial.lifetimeMin"], d.initial.lifetimeMin),
            lifetimeMax:            within(in_.lifetimeMax, R["initial.lifetimeMax"], d.initial.lifetimeMax),
            size:                   within(in_.size, R["initial.size"], d.initial.size),
            color:                  rgba(in_.color, d.initial.color),
        },
        overLife: {
            sizeStart:       within(ol.sizeStart, R["overLife.sizeStart"], 1),
            sizeEnd:         within(ol.sizeEnd, R["overLife.sizeEnd"], 0),
            colorStart:      rgba(ol.colorStart, d.overLife.colorStart),
            colorEnd:        rgba(ol.colorEnd,   d.overLife.colorEnd),
            velocityDamping: within(ol.velocityDamping, R["overLife.velocityDamping"], d.overLife.velocityDamping),
        },
        forces: {
            gravity: [
                within(grav[0], R["forces.gravity.x"], d.forces.gravity[0]),
                within(grav[1], R["forces.gravity.y"], d.forces.gravity[1]),
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
