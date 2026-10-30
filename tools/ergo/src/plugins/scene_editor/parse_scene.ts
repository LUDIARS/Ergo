/// parseScene — turn an engine's `scene` frame (ergo_scene `Scene::to_json()`
/// text) into the SceneInfo snapshot the UI renders.
///
/// Pure and tolerant: a malformed document yields null, missing fields fall
/// back to ergo_scene's own defaults. Free-form values (vars, component
/// props, prefab overrides) are kept as JSON text so the UI can edit them as
/// JSON and send them back verbatim (`vars/<name>/value`).

import type { ActorSummary, SceneInfo } from "./protocol.js";

type Vec3 = [number, number, number];
type Vec4 = [number, number, number, number];

/// `id` is the relay key from the frame (`{op:"scene", id, json}`); it wins
/// over the document's own `id` because UI patches are routed by it.
export function parseScene(id: string, json: string, app: string): SceneInfo | null {
    let root: unknown;
    try { root = JSON.parse(json); } catch { return null; }
    if (!isRecord(root)) return null;

    const rawActors = Array.isArray(root.actors) ? root.actors : [];
    const actors: ActorSummary[] = rawActors.filter(isRecord).map((a) => {
        const tr = isRecord(a.transform) ? a.transform : {};
        const vi = isRecord(a.visual) ? a.visual : {};
        return {
            id:        String(a.id ?? ""),
            name:      String(a.name ?? ""),
            type:      String(a.type ?? "Actor"),
            parent:    typeof a.parent === "string" ? a.parent : "",
            transform: {
                pos:   asVec3(tr.pos)   ?? [0, 0, 0],
                rot:   asVec4(tr.rot)   ?? [0, 0, 0, 1],
                scale: asVec3(tr.scale) ?? [1, 1, 1],
            },
            visual: {
                kind:     String(vi.kind ?? ""),
                ref:      String(vi.ref ?? ""),
                material: String(vi.material ?? ""),
            },
            instanceOf: String(a.instanceOf ?? ""),
            overrides:  parseOverrides(a.overrides),
            vars:       parseVars(a.vars),
            components: parseComponents(a.components),
        };
    });

    const cam = isRecord(root.camera) ? root.camera : {};
    return {
        id,
        domain: String(root.domain ?? "level"),
        mount:  String(root.mount ?? "/level"),
        actors,
        camera: {
            mode:     String(cam.mode ?? "orbit"),
            target:   asVec3(cam.target) ?? [0, 0, 0],
            distance: typeof cam.distance === "number" ? cam.distance : 8,
            fov_deg:  typeof cam.fov_deg  === "number" ? cam.fov_deg  : 50,
        },
        app,
    };
}

function isRecord(v: unknown): v is Record<string, unknown> {
    return typeof v === "object" && v !== null && !Array.isArray(v);
}

/// JSON text of a value; "" when the engine sent none.
function jsonText(v: unknown): string {
    return v === undefined ? "" : JSON.stringify(v);
}

function parseVars(raw: unknown): ActorSummary["vars"] {
    if (!Array.isArray(raw)) return [];
    return raw.filter(isRecord).map((v) => ({
        name:  String(v.name ?? ""),
        type:  String(v.type ?? ""),
        value: jsonText(v.value),
    }));
}

function parseOverrides(raw: unknown): ActorSummary["overrides"] {
    if (!Array.isArray(raw)) return [];
    return raw.filter(isRecord).map((o) => ({
        path:  String(o.path ?? ""),
        value: jsonText(o.value),
    }));
}

function parseComponents(raw: unknown): ActorSummary["components"] {
    if (!Array.isArray(raw)) return [];
    return raw.filter(isRecord).map((c) => ({
        type:  String(c.type ?? ""),
        props: Object.entries(c)
            .filter(([k]) => k !== "type")
            .map(([k, v]) => [k, jsonText(v)] as [string, string]),
    }));
}

function asVec3(v: unknown): Vec3 | null {
    if (!Array.isArray(v) || v.length < 3) return null;
    return [Number(v[0]), Number(v[1]), Number(v[2])];
}

function asVec4(v: unknown): Vec4 | null {
    if (!Array.isArray(v) || v.length < 4) return null;
    return [Number(v[0]), Number(v[1]), Number(v[2]), Number(v[3])];
}
