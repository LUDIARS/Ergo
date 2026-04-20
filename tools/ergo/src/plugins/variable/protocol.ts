/// Wire protocol for the `variable` plugin <-> `ergo_bind` engine client.
///
/// All frames are JSON over WebSocket text. The plugin is the source of
/// truth: it holds a registry of currently-bound variables (each with
/// kind, meta, last-known value, and the connection that bound it) and
/// fans value updates out to subscribed UI clients.
///
/// Two roles connect to /variable/ws and self-identify with `hello`:
///   * "engine"  — an application that calls bind() in C++
///   * "ui"      — a browser editor that displays & edits bound variables.
///
/// v2: `ergo_actor` adds a scene-graph layer. Engines may announce
///     actors via `actor_register` / `actor_unregister`, and every
///     bind message may carry `meta.actor` pointing at the owner
///     actor's handle (0 / missing = global, unowned).

export const SCHEMA_VERSION = 2;

export type VarKind =
    | "bool" | "int32" | "int64" | "float" | "double"
    | "string" | "color" | "vec3";

export interface VarMeta {
    min?:       number;
    max?:       number;
    step?:      number;
    read_only?: boolean;
    category?:  string;
    unit?:      string;

    /// Owner actor handle, or 0/missing when the variable is global.
    actor?:     number;
}

export type VarValue =
    | boolean
    | number
    | string
    | [number, number, number, number]   // color (rgba)
    | [number, number, number];          // vec3

export interface BoundVar {
    name:  string;
    kind:  VarKind;
    meta:  VarMeta;
    value: VarValue;
    app:   string;
}

export interface ActorNode {
    handle: number;           // > 0
    parent: number;           // 0 = root
    name:   string;
    app:    string;           // owning engine connection identifier
}

// ---- Engine -> Server ---------------------------------------------------

export type EngineMsg =
    | { op: "hello";            role: "engine"; app: string }
    | { op: "bind";             name: string; kind: VarKind; meta: VarMeta; value: VarValue }
    | { op: "value";            name: string; value: VarValue }
    | { op: "unbind";           name: string }
    | { op: "actor_register";   handle: number; parent: number; name: string }
    | { op: "actor_unregister"; handle: number };

// ---- UI -> Server -------------------------------------------------------

export type UiMsg =
    | { op: "hello"; role: "ui" }
    | { op: "set";   name: string; value: VarValue };

// ---- Server -> UI -------------------------------------------------------

export type ServerToUi =
    | { op: "registry"; vars:  BoundVar[] }
    | { op: "actors";   nodes: ActorNode[] }
    | { op: "value";    name:  string; value: VarValue };

// ---- Server -> Engine ---------------------------------------------------

export type ServerToEngine =
    | { op: "set"; name: string; value: VarValue };
