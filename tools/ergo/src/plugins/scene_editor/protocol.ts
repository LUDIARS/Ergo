/// Wire protocol for the scene_editor plugin.
///
/// Engine clients (C++: `ergo_scene_link`) push full scene JSON; UI clients
/// receive structured snapshots and send field-level patches back to the
/// owning engine, which applies them with `ergo::scene::Scene::apply_patch`
/// and re-publishes the scene. Spec: spec/feature/tool/scene_editor.md.

export interface ActorSummary {
    id:     string;
    name:   string;
    type:   string;
    /** Parent actor id in the same scene; "" for a root actor. */
    parent: string;
    transform: {
        pos:   [number, number, number];
        /** Quaternion [x, y, z, w]. The UI edits it as Euler degrees. */
        rot:   [number, number, number, number];
        scale: [number, number, number];
    };
    visual:     { kind: string; ref: string; material: string };
    /** Prefab reference; "" when the actor is not a prefab instance. */
    instanceOf: string;
    /** Prefab overrides; `value` is JSON text. */
    overrides:  Array<{ path: string; value: string }>;
    /** Public vars; `value` is JSON text ("" when the engine sent none). */
    vars:       Array<{ name: string; type: string; value: string }>;
    /** Components; each prop value is JSON text. */
    components: Array<{ type: string; props: Array<[string, string]> }>;
}

export interface SceneInfo {
    id:      string;
    domain:  string;
    mount:   string;
    actors:  ActorSummary[];
    camera: {
        mode:     string;
        target:   [number, number, number];
        distance: number;
        fov_deg:  number;
    };
    app: string;
}

// ---- Engine → Server -------------------------------------------------------
/**
 * `hello` must come first (frames from a connection without it are ignored);
 * `ergo_scene_link` re-sends `hello` and every published `scene` on each
 * (re)connect. `json` is the text of `Scene::to_json()`.
 */
export type EngineMsg =
    | { op: "hello"; role: "engine"; app?: string }
    | { op: "scene"; id: string; json: string }
    | { op: "scene_remove"; id: string };

// ---- UI → Server -----------------------------------------------------------
/**
 * Patch field paths accepted by `Scene::apply_patch` (anything else is
 * rejected by the engine, which then re-publishes the unchanged scene):
 *
 *   name | type | visual.kind | visual.ref | visual.material  value: string
 *   transform.pos | transform.scale                           value: [x, y, z]
 *   transform.rot        value: quaternion [x, y, z, w] (engine normalizes)
 *   vars/<name>/value    value: any JSON (the var must already exist)
 *   parent               value: actor id in the same scene, or "" to clear
 *
 * The old "transform.rot.xyz" (Euler xyz) path is not accepted.
 */
export type UiMsg =
    | { op: "hello"; role: "ui" }
    | { op: "patch"; scene_id: string; actor_id: string; field: string; value: unknown };

// ---- Server → UI -----------------------------------------------------------
export type ServerToUi =
    | { op: "scenes"; scenes: SceneInfo[] }
    | { op: "scene_update"; scene: SceneInfo }
    | { op: "scene_removed"; id: string };

// ---- Server → Engine -------------------------------------------------------
/** A UI patch, forwarded verbatim to the engine that published `scene_id`. */
export type ServerToEngine =
    | { op: "patch"; scene_id: string; actor_id: string; field: string; value: unknown };
