/// ParticleEffectConfig → KuzuSurvivors GPU EmitterDescriptor JSON 変換。
///
/// KS の VfxCatalog::register_from_json が読む EmitterDescriptor JSON
/// (ergo::shuriken_migrator::EmitterDescriptorToJson 出力形式) を生成する。
///
/// 制約:
///  - gravity は KS が World Y 軸を重力方向として使うため gravity_modifier に変換。
///  - EmitterShape: Sphere(1) を使用 (2D エミッタの positionRadius をそのまま利用)。
///  - BlendMode: additive→2, alpha→0 (gpu_particle::BlendMode の enum 値)。

import type { ParticleEffectConfig } from "../particle/schema.js";

/// gpu_particle::Curve のシリアライズ形式 (EmitterDescriptorToJson と同形)。
/// keys は [time, value] の列 (time は 0..1 正規化)。
interface CurveJson { keys: [number, number][]; }

/// gpu_particle::MinMaxCurve::Mode と同値 (Constant=0 / TwoConstants=1 /
/// Curve=2 / TwoCurves=3)。mode 2/3 では curve_min (+curve_max) が評価対象で、
/// min/max はカーブ未対応の消費側向けフォールバック値。
interface MinMaxCurveJson {
    mode: number;
    min: number;
    max: number;
    curve_min?: CurveJson;
    curve_max?: CurveJson;
}

function constant(v: number): MinMaxCurveJson {
    return { mode: 0, min: v, max: v };
}
function twoConsts(lo: number, hi: number): MinMaxCurveJson {
    return Math.abs(hi - lo) < 1e-6 ? constant(lo) : { mode: 1, min: lo, max: hi };
}
function linearCurve(v0: number, v1: number): CurveJson {
    return { keys: [[0, v0], [1, v1]] };
}
/// v0 (寿命開始) → v1 (寿命終了) の線形フェードを Curve モードで表す。
function fadeMinMax(v0: number, v1: number): MinMaxCurveJson {
    if (Math.abs(v0 - v1) < 1e-6) return constant(v0);
    return {
        mode: 2,
        min: Math.min(v0, v1),
        max: Math.max(v0, v1),
        curve_min: linearCurve(v0, v1),
    };
}

export interface KsEmitterJson {
    name:              string;
    duration:          number;
    loop:              boolean;
    start_lifetime:    MinMaxCurveJson;
    start_speed:       MinMaxCurveJson;
    start_size:        MinMaxCurveJson;
    start_color:       [number, number, number, number];
    max_particles:     number;
    simulation_space:  number;   // 0 = World
    gravity_modifier:  number;
    rate_over_time:    MinMaxCurveJson;
    shape:             number;   // 1 = Sphere
    sphere_radius:     number;
    cone_radius:       number;
    cone_angle_deg:    number;
    shape_scale:       [number, number, number];
    size_over_lifetime: MinMaxCurveJson;
    /// RGBA 各成分の over-life カーブ。overLife.colorStart→colorEnd の線形フェード。
    color_over_lifetime: { r: CurveJson; g: CurveJson; b: CurveJson; a: CurveJson };
    blend_mode:        number;   // 0 = Alpha, 2 = Additive
}

/// 2D ParticleEffectConfig を KS GPU EmitterDescriptor JSON に変換する。
/// `key` は VfxCatalog への登録キー (例 "Effect/Lightning")。
/// 省略すると config.name をそのまま使う。
export function toKsEmitterJson(cfg: ParticleEffectConfig, key?: string): KsEmitterJson {
    const ini = cfg.initial;
    const ol  = cfg.overLife;
    const em  = cfg.emission;
    const re  = cfg.render;

    // 2D gravity Y 正 = 下、GPU デフォルト gravity = {0,-9.81,0} (下向き)
    // gravity_modifier は GPU gravity を倍率スケーリングするため符号が逆
    const gravityModifier = cfg.forces.gravity[1] / 9.81;

    return {
        name:             key ?? cfg.name,
        duration:         0,       // 0 = infinite
        loop:             true,
        start_lifetime:   twoConsts(ini.lifetimeMin, ini.lifetimeMax),
        start_speed:      twoConsts(ini.speedMin,    ini.speedMax),
        start_size:       constant(ini.size),
        start_color:      [...ini.color] as [number, number, number, number],
        max_particles:    em.maxAlive,
        simulation_space: 0,       // World
        gravity_modifier: gravityModifier,
        rate_over_time:   constant(em.rate),
        shape:            1,       // EmitterShape::Sphere
        sphere_radius:    ini.positionRadius,
        cone_radius:      ini.positionRadius,
        cone_angle_deg:   ini.velocityAngleSpreadDeg / 2,
        shape_scale:      [1, 1, 1],
        // size_over_lifetime: sizeStart → sizeEnd の線形フェード (Curve モード)
        size_over_lifetime: fadeMinMax(ol.sizeStart, ol.sizeEnd),
        color_over_lifetime: {
            r: linearCurve(ol.colorStart[0], ol.colorEnd[0]),
            g: linearCurve(ol.colorStart[1], ol.colorEnd[1]),
            b: linearCurve(ol.colorStart[2], ol.colorEnd[2]),
            a: linearCurve(ol.colorStart[3], ol.colorEnd[3]),
        },
        blend_mode:       re.blend === "additive" ? 2 : 0,
    };
}
