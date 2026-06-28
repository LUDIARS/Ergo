/// Wire protocol for the scene_editor plugin.
///
/// Engine clients push full scene JSON; UI clients receive structured snapshots
/// and send field-level patches back to the owning engine.

export interface ActorSummary {
    id:     string;
    name:   string;
    type:   string;
    parent: string;
    transform: {
        pos:   [number, number, number];
        rot:   [number, number, number, number];
        scale: [number, number, number];
    };
    visual:     { kind: string; ref: string; material: string };
    instanceOf: string;
    vars:       Array<{ name: string; type: string; value: string }>;
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
export type EngineMsg =
    | { op: "hello"; role: "engine"; app?: string }
    | { op: "scene"; id: string; json: string }
    | { op: "scene_remove"; id: string };

// ---- UI → Server -----------------------------------------------------------
export type UiMsg =
    | { op: "hello"; role: "ui" }
    /** field: dot-separated path e.g. "transform.pos", "name", "vars/health/value" */
    | { op: "patch"; scene_id: string; actor_id: string; field: string; value: unknown };

// ---- Server → UI -----------------------------------------------------------
export type ServerToUi =
    | { op: "scenes"; scenes: SceneInfo[] }
    | { op: "scene_update"; scene: SceneInfo }
    | { op: "scene_removed"; id: string };

// ---- Server → Engine -------------------------------------------------------
export type ServerToEngine =
    | { op: "patch"; scene_id: string; actor_id: string; field: string; value: unknown };
