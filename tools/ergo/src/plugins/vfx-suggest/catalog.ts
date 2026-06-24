/// Hardcoded VFX presets — reference library and LLM few-shot source.
/// Each preset is a complete, engine-ready ParticleEffectConfig.

import { SCHEMA_VERSION, type ParticleEffectConfig } from "../particle/schema.js";
import type { GameId, VfxPreset } from "./schema.js";

function p(
    name: string,
    rationale: string,
    cfg: Omit<ParticleEffectConfig, "version" | "name"> & { name?: string },
): VfxPreset {
    return {
        name,
        rationale,
        config: { version: SCHEMA_VERSION, name: cfg.name ?? name, ...cfg },
    };
}

export const CATALOG: VfxPreset[] = [
    p("炎", "上昇する暖色の炎。前方斬撃・爆炎スキルに。additive blend で重なりが映える。", {
        emission:  { rate: 80,  maxAlive: 200 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 4,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 50,
            speedMin: 60,  speedMax: 160,
            lifetimeMin: 0.5, lifetimeMax: 1.2,
            size: 10, color: [1, 0.55, 0.12, 1],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.0,
            colorStart: [1, 0.45, 0.05, 1],
            colorEnd:   [0.6, 0.1, 0.0, 0],
            velocityDamping: 0.65,
        },
        forces: { gravity: [0, -40] },
        render:  { blend: "additive", shape: "circle" },
    }),

    p("氷結", "青白い破片が四散。氷属性スキル・凍結ヒット時に。", {
        emission:  { rate: 60,  maxAlive: 150 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 5,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 180,
            speedMin: 40,  speedMax: 120,
            lifetimeMin: 0.6, lifetimeMax: 1.8,
            size: 6, color: [0.65, 0.92, 1, 1],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.2,
            colorStart: [0.75, 0.95, 1, 1],
            colorEnd:   [0.25, 0.55, 1, 0],
            velocityDamping: 0.55,
        },
        forces: { gravity: [0, 90] },
        render:  { blend: "additive", shape: "square" },
    }),

    p("雷撃", "瞬間的な高速白黄パーティクル。KS Effect/Lightning 向き。短命・鮮烈。", {
        emission:  { rate: 250, maxAlive: 120 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 3,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 360,
            speedMin: 180, speedMax: 420,
            lifetimeMin: 0.08, lifetimeMax: 0.28,
            size: 4, color: [1, 1, 0.85, 1],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.0,
            colorStart: [1, 1, 1, 1],
            colorEnd:   [0.5, 0.8, 1, 0],
            velocityDamping: 0.4,
        },
        forces: { gravity: [0, 0] },
        render:  { blend: "additive", shape: "square" },
    }),

    p("爆発", "全方位バースト。KS Effect/Shockwave 向き。強烈な閃光から残煙へ。", {
        emission:  { rate: 350, maxAlive: 300 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 6,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 360,
            speedMin: 100, speedMax: 320,
            lifetimeMin: 0.25, lifetimeMax: 0.85,
            size: 14, color: [1, 0.65, 0.12, 1],
        },
        overLife: {
            sizeStart: 1.6, sizeEnd: 0.0,
            colorStart: [1, 0.4, 0.05, 1],
            colorEnd:   [0.25, 0.08, 0.0, 0],
            velocityDamping: 0.7,
        },
        forces: { gravity: [0, 55] },
        render:  { blend: "additive", shape: "circle" },
    }),

    p("回復", "緑・シアンの粒子が上昇。HP/MP 回復スキルに。alpha blend で柔らかく。", {
        emission:  { rate: 35,  maxAlive: 90 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 12,
            velocityAngleDeg: 90, velocityAngleSpreadDeg: 60,
            speedMin: 25,  speedMax: 75,
            lifetimeMin: 1.0, lifetimeMax: 2.5,
            size: 8, color: [0.22, 1, 0.52, 0.85],
        },
        overLife: {
            sizeStart: 0.5, sizeEnd: 1.6,
            colorStart: [0.3, 1, 0.6, 0.8],
            colorEnd:   [0.1, 0.8, 0.45, 0],
            velocityDamping: 0.75,
        },
        forces: { gravity: [0, -55] },
        render:  { blend: "additive", shape: "circle" },
    }),

    p("衝撃波", "放射状に広がる白い波紋。AC の被弾リアクション・KS リフレクトに。", {
        emission:  { rate: 120, maxAlive: 100 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 3,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 360,
            speedMin: 140, speedMax: 260,
            lifetimeMin: 0.18, lifetimeMax: 0.5,
            size: 7, color: [0.9, 0.92, 1, 0.85],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.0,
            colorStart: [1, 1, 1, 0.95],
            colorEnd:   [0.5, 0.72, 1, 0],
            velocityDamping: 0.35,
        },
        forces: { gravity: [0, 0] },
        render:  { blend: "additive", shape: "circle" },
    }),

    p("砂煙", "ゆっくり漂う土埃。着地・移動残像・環境演出に。alpha blend で透過感。", {
        emission:  { rate: 22,  maxAlive: 100 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 9,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 120,
            speedMin: 18,  speedMax: 55,
            lifetimeMin: 1.5, lifetimeMax: 3.0,
            size: 12, color: [0.72, 0.62, 0.5, 0.5],
        },
        overLife: {
            sizeStart: 0.8, sizeEnd: 1.5,
            colorStart: [0.8, 0.7, 0.58, 0.4],
            colorEnd:   [0.7, 0.62, 0.5, 0],
            velocityDamping: 0.88,
        },
        forces: { gravity: [0, 30] },
        render:  { blend: "alpha", shape: "circle" },
    }),

    p("魔法陣", "紫・ピンクの輝粒子が緩やかに舞う。詠唱・バフ・スキル発動前兆に。", {
        emission:  { rate: 45,  maxAlive: 160 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 8,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 360,
            speedMin: 45,  speedMax: 115,
            lifetimeMin: 0.8, lifetimeMax: 2.0,
            size: 5, color: [0.82, 0.32, 1, 1],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.0,
            colorStart: [0.9, 0.42, 1, 1],
            colorEnd:   [0.28, 0.1, 0.85, 0],
            velocityDamping: 0.72,
        },
        forces: { gravity: [0, -22] },
        render:  { blend: "additive", shape: "circle" },
    }),

    // ── KS 固有 ─────────────────────────────────────────────────────────────

    p("Effect/ReflectLaserBeam", "レーザーが反射する瞬間の閃光散乱。高速スパーク+残光。KS ReflectLaserBeam キー向き。", {
        name: "Effect/ReflectLaserBeam",
        emission:  { rate: 180, maxAlive: 100 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 2,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 140,
            speedMin: 120, speedMax: 300,
            lifetimeMin: 0.1, lifetimeMax: 0.35,
            size: 5, color: [0.7, 0.95, 1, 1],
        },
        overLife: {
            sizeStart: 1.2, sizeEnd: 0.0,
            colorStart: [0.8, 1, 1, 1],
            colorEnd:   [0.3, 0.6, 1, 0],
            velocityDamping: 0.45,
        },
        forces: { gravity: [0, 0] },
        render:  { blend: "additive", shape: "square" },
    }),

    p("Effect/Clone", "半透明の残像分身。幽霊的な薄い輝き粒子が短命で散る。KS Clone キー向き。", {
        name: "Effect/Clone",
        emission:  { rate: 55, maxAlive: 120 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 10,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 360,
            speedMin: 15,  speedMax: 55,
            lifetimeMin: 0.5, lifetimeMax: 1.5,
            size: 7, color: [0.55, 0.8, 1, 0.55],
        },
        overLife: {
            sizeStart: 0.8, sizeEnd: 0.3,
            colorStart: [0.65, 0.85, 1, 0.5],
            colorEnd:   [0.35, 0.55, 1, 0],
            velocityDamping: 0.85,
        },
        forces: { gravity: [0, -15] },
        render:  { blend: "additive", shape: "circle" },
    }),

    // ── AC 固有 ─────────────────────────────────────────────────────────────

    p("斬撃フラッシュ", "auto_melee_forward の瞬間フラッシュ。前方への鋭い白フラッシュ+残光。", {
        emission:  { rate: 200, maxAlive: 60 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 1,
            velocityAngleDeg: 0, velocityAngleSpreadDeg: 40,
            speedMin: 80,  speedMax: 200,
            lifetimeMin: 0.05, lifetimeMax: 0.18,
            size: 8, color: [1, 0.95, 0.8, 1],
        },
        overLife: {
            sizeStart: 1.5, sizeEnd: 0.0,
            colorStart: [1, 1, 0.9, 1],
            colorEnd:   [1, 0.65, 0.3, 0],
            velocityDamping: 0.3,
        },
        forces: { gravity: [0, 0] },
        render:  { blend: "additive", shape: "square" },
    }),

    p("追尾弾軌跡", "auto_timed_projectile の弾軌跡。小さな輝粒子が後方に流れる。", {
        emission:  { rate: 60, maxAlive: 80 },
        initial: {
            positionShape: "disc", svgSource: "", svgScale: 1,
            positionRadius: 1,
            velocityAngleDeg: 270, velocityAngleSpreadDeg: 30,
            speedMin: 10,  speedMax: 40,
            lifetimeMin: 0.15, lifetimeMax: 0.4,
            size: 4, color: [0.9, 0.7, 1, 1],
        },
        overLife: {
            sizeStart: 1.0, sizeEnd: 0.0,
            colorStart: [0.95, 0.75, 1, 1],
            colorEnd:   [0.5, 0.2, 0.8, 0],
            velocityDamping: 0.9,
        },
        forces: { gravity: [0, 20] },
        render:  { blend: "additive", shape: "circle" },
    }),
];

/// Game-specific tags map event keywords → catalog preset names for few-shot.
export const GAME_HINTS: Record<GameId, string[]> = {
    ks: ["雷撃", "爆発", "衝撃波", "Effect/ReflectLaserBeam", "Effect/Clone", "炎"],
    ac: ["斬撃フラッシュ", "追尾弾軌跡", "炎", "衝撃波", "魔法陣", "回復"],
    ul: ["回復", "魔法陣", "砂煙", "炎", "氷結"],
    generic: ["炎", "爆発", "回復", "衝撃波", "魔法陣"],
};

/// Returns up to `n` catalog presets relevant for the given game (for LLM few-shot).
export function catalogExamples(game: GameId, n = 3): VfxPreset[] {
    const hints = GAME_HINTS[game];
    const ordered = [
        ...hints.map(h => CATALOG.find(p => p.name === h)!).filter(Boolean),
        ...CATALOG,
    ];
    const seen = new Set<string>();
    const result: VfxPreset[] = [];
    for (const p of ordered) {
        if (!seen.has(p.name)) { seen.add(p.name); result.push(p); }
        if (result.length >= n) break;
    }
    return result;
}
