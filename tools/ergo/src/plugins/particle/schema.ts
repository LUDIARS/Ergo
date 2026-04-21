/// Canonical particle effect schema (v1) — shared by browser UI, server,
/// and engine clients. Keep this in sync with:
///   - ui/index.html  (DEFAULTS / SCHEMA constants)
///   - ergo_particle  (ParticleEffectConfig struct in C++)

export const SCHEMA_VERSION = 1;

/// Shape that governs initial particle positions.
///
/// - `disc`      uses `initial.positionRadius` (original behavior).
/// - `svgStroke` samples uniformly along the length of every geometric
///               element inside `initial.svgSource` (paths, polygons,
///               rect/circle/ellipse/line). Useful for silhouette outlines.
/// - `svgFill`   rejection-samples inside any shape's filled area. Useful
///               for emitting "from inside a logo".
///
/// The engine-side C++ consumer (`ergo_particle`) currently only implements
/// the `disc` mode; `svgStroke` / `svgFill` fall back to disc. The UI
/// preview fully supports all modes.
export type PositionShape = "disc" | "svgStroke" | "svgFill";

export interface ParticleEffectConfig {
    version: number;
    name:    string;
    emission: { rate: number; maxAlive: number };
    initial: {
        positionShape:          PositionShape;
        /// Raw SVG markup (full `<svg>` document) or a single path's `d` data.
        /// Ignored when `positionShape === "disc"` or when the string is empty.
        svgSource:              string;
        /// Uniform scale applied after the SVG is centered on its bbox.
        svgScale:               number;
        positionRadius:         number;
        velocityAngleDeg:       number;
        velocityAngleSpreadDeg: number;
        speedMin: number;
        speedMax: number;
        lifetimeMin: number;
        lifetimeMax: number;
        size:  number;
        color: [number, number, number, number];
    };
    overLife: {
        sizeStart:   number;
        sizeEnd:     number;
        colorStart:  [number, number, number, number];
        colorEnd:    [number, number, number, number];
        velocityDamping: number;
    };
    forces: { gravity: [number, number] };
    render: { blend: "additive" | "alpha"; shape: "circle" | "square" };
}

export const DEFAULT_EFFECT: ParticleEffectConfig = {
    version: SCHEMA_VERSION,
    name: "untitled",
    emission: { rate: 60, maxAlive: 400 },
    initial: {
        positionShape: "disc",
        svgSource: "",
        svgScale: 1,
        positionRadius: 4,
        velocityAngleDeg: 270,
        velocityAngleSpreadDeg: 60,
        speedMin: 80,
        speedMax: 180,
        lifetimeMin: 0.7,
        lifetimeMax: 1.4,
        size: 8,
        color: [1, 0.75, 0.25, 1],
    },
    overLife: {
        sizeStart: 1.0,
        sizeEnd:   0.0,
        colorStart: [1, 0.75, 0.25, 1],
        colorEnd:   [0.7, 0.1, 0.0, 0],
        velocityDamping: 0.6,
    },
    forces: { gravity: [0, 120] },
    render: { blend: "additive", shape: "circle" },
};

/// Recursively merge `partial` into `base`, returning a new object.
/// Arrays are replaced wholesale (not element-merged).
export function mergeConfig(
    base: ParticleEffectConfig,
    partial: Partial<ParticleEffectConfig> | unknown,
): ParticleEffectConfig {
    const out: any = JSON.parse(JSON.stringify(base));
    if (!partial || typeof partial !== "object") return out;
    const p = partial as Record<string, any>;
    for (const k of Object.keys(out)) {
        if (p[k] == null) continue;
        if (typeof p[k] === "object" && !Array.isArray(p[k])) {
            out[k] = { ...out[k], ...p[k] };
        } else {
            out[k] = p[k];
        }
    }
    out.version = SCHEMA_VERSION;
    return out;
}

// ---- Wire protocol -------------------------------------------------------

export type Inbound =
    | { op: "set";     config: Partial<ParticleEffectConfig> }
    | { op: "replace"; config: ParticleEffectConfig }
    | { op: "ping" };

export type Outbound =
    | { op: "state"; config: ParticleEffectConfig; clients: number }
    | { op: "ack" };
