/// The gene table: which ParticleEffectConfig values breed, and how each one
/// maps to a normalised 0..1 gene.
///
/// - Numeric fields take their range from particle/ranges.ts (the same table
///   vfx-suggest's sanitize clamps against).
/// - Colors breed as HSV + alpha; hue wraps around.
/// - render.blend / render.shape are the discrete genes (see genome.ts).
/// - positionShape / svgSource / svgScale are not genes: they are carried
///   over from the parent unchanged.

import { PARTICLE_RANGES, COLOR_RANGE, type ParticleRangeKey, type ValueRange } from "../particle/ranges.js";

export type ColorField = "initial.color" | "overLife.colorStart" | "overLife.colorEnd";

export type GeneSource =
    | { kind: "number"; key: ParticleRangeKey }
    | { kind: "color";  field: ColorField; channel: 0 | 1 | 2 | 3 };

export interface GeneSpec {
    /** Stable id, also the key the direction advisor answers with. */
    id:      string;
    /** Short description shown to the advisor LLM. */
    label:   string;
    range:   ValueRange;
    /** log: normalise on a log scale (for values spanning orders of magnitude). */
    scale:   "linear" | "log";
    /** wrap: the gene is circular (0 and 1 are the same value). */
    wrap:    boolean;
    integer: boolean;
    source:  GeneSource;
}

const num = (
    key: ParticleRangeKey,
    label: string,
    opts: Partial<Pick<GeneSpec, "scale" | "wrap" | "integer">> = {},
): GeneSpec => ({
    id: key,
    label,
    range: PARTICLE_RANGES[key],
    scale: opts.scale ?? "linear",
    wrap: opts.wrap ?? false,
    integer: opts.integer ?? false,
    source: { kind: "number", key },
});

const CHANNELS = [
    { suffix: "hue",        label: "hue",        wrap: true  },
    { suffix: "saturation", label: "saturation", wrap: false },
    { suffix: "value",      label: "brightness", wrap: false },
    { suffix: "alpha",      label: "opacity",    wrap: false },
] as const;

const color = (field: ColorField, label: string): GeneSpec[] =>
    CHANNELS.map((ch, i) => ({
        id: `${field}.${ch.suffix}`,
        label: `${label} ${ch.label}`,
        range: COLOR_RANGE,
        scale: "linear" as const,
        wrap: ch.wrap,
        integer: false,
        source: { kind: "color" as const, field, channel: i as 0 | 1 | 2 | 3 },
    }));

export const GENES: readonly GeneSpec[] = [
    num("emission.rate",                  "particles emitted per second", { scale: "log" }),
    num("emission.maxAlive",              "max particles alive at once",  { scale: "log", integer: true }),
    num("initial.positionRadius",         "spawn disc radius (px)"),
    num("initial.velocityAngleDeg",       "launch direction (deg, 270 = up)", { wrap: true }),
    num("initial.velocityAngleSpreadDeg", "launch cone width (deg)"),
    num("initial.speedMin",               "min launch speed (px/s)"),
    num("initial.speedMax",               "max launch speed (px/s)"),
    num("initial.lifetimeMin",            "min lifetime (s)"),
    num("initial.lifetimeMax",            "max lifetime (s)"),
    num("initial.size",                   "particle size (px)"),
    num("overLife.sizeStart",             "size multiplier at birth"),
    num("overLife.sizeEnd",               "size multiplier at death"),
    num("overLife.velocityDamping",       "velocity kept per second (1 = no drag)"),
    num("forces.gravity.x",               "horizontal force (px/s^2)"),
    num("forces.gravity.y",               "vertical force (px/s^2, + = down)"),
    ...color("initial.color",       "spawn color"),
    ...color("overLife.colorStart", "start color"),
    ...color("overLife.colorEnd",   "end color"),
];

export const GENE_IDS: readonly string[] = GENES.map((g) => g.id);

export function geneIndex(id: string): number {
    return GENE_IDS.indexOf(id);
}
