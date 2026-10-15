/// ParticleEffectConfig <-> Genome.
///
/// A genome is the normalised 0..1 vector of GENES plus the two discrete
/// genes (blend / shape). decode() always yields a config whose values sit
/// inside the shared ranges (particle/ranges.ts).

import {
    type ParticleEffectConfig,
    DEFAULT_EFFECT,
    SCHEMA_VERSION,
    mergeConfig,
} from "../particle/schema.js";
import { GENES, type GeneSpec, type ColorField } from "./genes.js";
import { rgbaToHsva, hsvaToRgba, type Rgba } from "./color.js";

export type Blend = ParticleEffectConfig["render"]["blend"];
export type Shape = ParticleEffectConfig["render"]["shape"];

export interface Genome {
    /** Aligned with GENES; every value in [0, 1]. */
    genes: number[];
    blend: Blend;
    shape: Shape;
}

const clamp01 = (v: number): number => Math.min(1, Math.max(0, v));

/// Bring a gene back into [0, 1]: wrap circular genes, clamp the rest.
export function fold(v: number, wrap: boolean): number {
    if (!Number.isFinite(v)) return 0;
    return wrap ? ((v % 1) + 1) % 1 : clamp01(v);
}

function normalise(spec: GeneSpec, v: number): number {
    const { lo, hi } = spec.range;
    const x = Math.min(hi, Math.max(lo, Number.isFinite(v) ? v : lo));
    if (spec.scale === "log") return clamp01(Math.log(x / lo) / Math.log(hi / lo));
    return hi > lo ? clamp01((x - lo) / (hi - lo)) : 0;
}

function denormalise(spec: GeneSpec, g: number): number {
    const { lo, hi } = spec.range;
    const t = clamp01(g);
    const raw = spec.scale === "log" ? lo * Math.pow(hi / lo, t) : lo + (hi - lo) * t;
    const rounded = spec.integer ? Math.round(raw) : Math.round(raw * 1000) / 1000;
    return Math.min(hi, Math.max(lo, rounded));
}

function readColor(cfg: ParticleEffectConfig, field: ColorField): Rgba {
    return field === "initial.color" ? cfg.initial.color
         : field === "overLife.colorStart" ? cfg.overLife.colorStart
         : cfg.overLife.colorEnd;
}

function readNumber(cfg: ParticleEffectConfig, key: string): number {
    switch (key) {
        case "forces.gravity.x": return cfg.forces.gravity[0];
        case "forces.gravity.y": return cfg.forces.gravity[1];
    }
    const [group, field] = key.split(".") as [string, string];
    return Number((cfg as any)[group][field]);
}

/// Accepts any partial / foreign JSON; missing fields fall back to the default effect.
export function toConfig(raw: unknown): ParticleEffectConfig {
    return mergeConfig(DEFAULT_EFFECT, raw);
}

export function encode(raw: unknown): Genome {
    const cfg = toConfig(raw);
    const hsv = new Map<ColorField, number[]>();
    const genes = GENES.map((spec) => {
        const src = spec.source;
        if (src.kind === "number") return normalise(spec, readNumber(cfg, src.key));
        let c = hsv.get(src.field);
        if (!c) {
            c = rgbaToHsva(readColor(cfg, src.field).map((x) => clamp01(Number(x))) as Rgba);
            hsv.set(src.field, c);
        }
        return fold(c[src.channel]!, spec.wrap);
    });
    return {
        genes,
        blend: cfg.render.blend === "alpha" ? "alpha" : "additive",
        shape: cfg.render.shape === "square" ? "square" : "circle",
    };
}

/// Build a config from a genome. `carrier` supplies the non-gene fields
/// (positionShape / svgSource / svgScale) — they are inherited, never bred.
export function decode(genome: Genome, carrier: unknown, name: string): ParticleEffectConfig {
    const base = toConfig(carrier);
    const nums = new Map<string, number>();
    const colors = new Map<ColorField, number[]>();

    GENES.forEach((spec, i) => {
        const g = genome.genes[i] ?? 0;
        const src = spec.source;
        if (src.kind === "number") {
            nums.set(src.key, denormalise(spec, g));
        } else {
            const c = colors.get(src.field) ?? [0, 0, 0, 0];
            c[src.channel] = fold(g, spec.wrap);
            colors.set(src.field, c);
        }
    });

    const n = (k: string): number => nums.get(k)!;
    const rgba = (f: ColorField): Rgba =>
        hsvaToRgba(colors.get(f) as Rgba).map((x) => Math.round(clamp01(x) * 1000) / 1000) as Rgba;
    const ordered = (a: number, b: number): [number, number] => (a <= b ? [a, b] : [b, a]);

    const [speedMin, speedMax] = ordered(n("initial.speedMin"), n("initial.speedMax"));
    const [lifetimeMin, lifetimeMax] = ordered(n("initial.lifetimeMin"), n("initial.lifetimeMax"));

    return {
        version: SCHEMA_VERSION,
        name,
        emission: {
            rate:     n("emission.rate"),
            maxAlive: n("emission.maxAlive"),
        },
        initial: {
            positionShape:          base.initial.positionShape,
            svgSource:              base.initial.svgSource,
            svgScale:               base.initial.svgScale,
            positionRadius:         n("initial.positionRadius"),
            velocityAngleDeg:       n("initial.velocityAngleDeg"),
            velocityAngleSpreadDeg: n("initial.velocityAngleSpreadDeg"),
            speedMin,
            speedMax,
            lifetimeMin,
            lifetimeMax,
            size:                   n("initial.size"),
            color:                  rgba("initial.color"),
        },
        overLife: {
            sizeStart:       n("overLife.sizeStart"),
            sizeEnd:         n("overLife.sizeEnd"),
            colorStart:      rgba("overLife.colorStart"),
            colorEnd:        rgba("overLife.colorEnd"),
            velocityDamping: n("overLife.velocityDamping"),
        },
        forces: { gravity: [n("forces.gravity.x"), n("forces.gravity.y")] },
        render: { blend: genome.blend, shape: genome.shape },
    };
}
