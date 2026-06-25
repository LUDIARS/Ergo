/// Stickfig plugin — 棒人間 (stick figure) の幾何学スペックを生成する軽量ツール。
///
/// C++ 側 `ergo_stickfig` (`src/stickfig/stick_figure.{h,cpp}`) が実メッシュ
/// (頂点 + インデックス) を組み立てるのに対し、 本プラグインは同じ体型モデルを
/// **関節 + ボーン (capsule セグメント) + 頭部 (sphere) の構造化スペック** として
/// JSON で返す。 エディタ側はこのスペックをプレビュー表示したり、 C++ 生成器へ
/// 渡すパラメータの確認に使う。
///
/// プロトコル:
///   GET  /stickfig/api/preview                 → 既定パラメータのスペック
///   POST /stickfig/api/generate  body:{params} → パラメータ指定でスペック生成
///   GET  /stickfig/api/health                  → 標準
///
/// スペックと C++ `generate_stick_figure()` のジョイント計算は同一式 (height /
/// arm_span 比率) を共有する。 数値を変えるときは両方を同じ PR で更新すること。

import { Hono } from "hono";
import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

/// 棒人間の体型パラメータ (C++ `StickFigureParams` と対応)。
interface StickFigureParams {
    height:      number;  // 直立全高 (足元 y=0 → 頭頂 y=height)
    armSpan:     number;  // 指先〜指先の横幅
    limbRadius:  number;  // 四肢・胴カプセルの半径
    headRadius:  number;  // 頭部球の半径
    segments:    number;  // 円周方向のテッセレーション
}

const DEFAULT_PARAMS: StickFigureParams = {
    height:     1.8,
    armSpan:    1.6,
    limbRadius: 0.055,
    headRadius: 0.13,
    segments:   12,
};

type Vec3 = [number, number, number];

interface Bone {
    name:   string;
    from:   Vec3;
    to:     Vec3;
    radius: number;
    color:  [number, number, number, number];
}

interface Head {
    name:   string;
    center: Vec3;
    radius: number;
    color:  [number, number, number, number];
}

interface StickFigureSpec {
    schemaVersion: number;
    params:        StickFigureParams;
    bones:         Bone[];
    head:          Head;
    /// 描画用の概算バウンディング (足元〜頭頂)。
    bounds:        { minY: number; maxY: number; spanX: number };
}

/// 入力を検証し、 NaN / 非正値を既定にフォールバックさせず弾く。
/// (無言フォールバック禁止: 不正値は 400 で返す方針のため null を返す)
function coerceParams(raw: unknown): StickFigureParams | null {
    if (raw === undefined || raw === null) return { ...DEFAULT_PARAMS };
    if (typeof raw !== "object") return null;
    const o = raw as Record<string, unknown>;

    const pick = (key: keyof StickFigureParams, min: number, max: number): number | null => {
        const v = o[key];
        if (v === undefined) return DEFAULT_PARAMS[key];
        const n = Number(v);
        if (!Number.isFinite(n) || n < min || n > max) return null;
        return n;
    };

    const height     = pick("height",     0.1, 10);
    const armSpan    = pick("armSpan",     0.1, 10);
    const limbRadius = pick("limbRadius",  0.001, 1);
    const headRadius = pick("headRadius",  0.001, 1);
    const segmentsN  = pick("segments",    3, 128);
    if (height === null || armSpan === null || limbRadius === null ||
        headRadius === null || segmentsN === null) {
        return null;
    }
    return {
        height, armSpan, limbRadius, headRadius,
        segments: Math.round(segmentsN),
    };
}

/// C++ `generate_stick_figure()` と同一のジョイント式でスペックを構築する。
function buildSpec(p: StickFigureParams): StickFigureSpec {
    const H = p.height;
    const footY        = 0;
    const hipY         = H * 0.50;
    const shoulderY    = H * 0.82;
    const hipHalf      = p.limbRadius * 1.6;
    const shoulderHalf = p.armSpan * 0.10;
    const handX        = p.armSpan * 0.5;
    const handY        = shoulderY - H * 0.18;
    const headCy       = shoulderY + p.headRadius + H * 0.04;
    const torsoRadius  = p.limbRadius * 1.5;

    const colTorso: [number, number, number, number] = [0.25, 0.45, 0.85, 1.0];
    const colLeg:   [number, number, number, number] = [0.85, 0.30, 0.25, 1.0];
    const colArm:   [number, number, number, number] = [0.30, 0.75, 0.40, 1.0];

    const bones: Bone[] = [
        { name: "torso", from: [0, hipY, 0],            to: [0, shoulderY, 0],   radius: torsoRadius,  color: colTorso },
        { name: "leg_l", from: [-hipHalf, hipY, 0],     to: [-hipHalf, footY, 0], radius: p.limbRadius, color: colLeg },
        { name: "leg_r", from: [ hipHalf, hipY, 0],     to: [ hipHalf, footY, 0], radius: p.limbRadius, color: colLeg },
        { name: "arm_l", from: [-shoulderHalf, shoulderY, 0], to: [-handX, handY, 0], radius: p.limbRadius, color: colArm },
        { name: "arm_r", from: [ shoulderHalf, shoulderY, 0], to: [ handX, handY, 0], radius: p.limbRadius, color: colArm },
    ];

    const head: Head = {
        name:   "head",
        center: [0, headCy, 0],
        radius: p.headRadius,
        color:  [0.95, 0.80, 0.65, 1.0],
    };

    return {
        schemaVersion: SCHEMA_VERSION,
        params:        p,
        bones,
        head,
        bounds: {
            minY:  footY,
            maxY:  headCy + p.headRadius,
            spanX: p.armSpan,
        },
    };
}

const factory: PluginFactory = () => {
    const plugin: Plugin = {
        id:          "stickfig",
        title:       "Stick Figure",
        icon:        "🧍",
        description: "棒人間 (頭=球 / 四肢=カプセル) の幾何学スペックを生成する。",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/preview", (c) => {
                const spec = buildSpec({ ...DEFAULT_PARAMS });
                return c.json({ ok: true, spec });
            });

            app.post("/api/generate", async (c) => {
                let body: unknown;
                try {
                    body = await c.req.json();
                } catch {
                    return c.json({ ok: false, err: "invalid JSON body" }, 400);
                }
                const params = coerceParams((body as Record<string, unknown>)?.params ?? body);
                if (!params) {
                    return c.json({ ok: false, err: "invalid params (out of range or non-numeric)" }, 400);
                }
                const spec = buildSpec(params);
                ctx.log("info", `[stickfig] generated H=${params.height} span=${params.armSpan}`);
                return c.json({ ok: true, spec });
            });

            app.get("/api/health", (c) => {
                return c.json({ ok: true, version: SCHEMA_VERSION });
            });

            return app;
        },

        health() {
            return { ok: true, version: SCHEMA_VERSION };
        },
    };

    return plugin;
};

export default factory;
