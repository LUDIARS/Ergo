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

export const SCHEMA_VERSION = 1;

export type VarKind =
    | "bool" | "int32" | "int64" | "float" | "double"
    | "string" | "color" | "vec3";

export interface VarMeta {
    min?: number;
    max?: number;
    step?: number;
    read_only?: boolean;
    category?: string;
    unit?: string;
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

export type EngineMsg =
    | { op: "hello";  role: "engine"; app: string }
    | { op: "bind";   name: string; kind: VarKind; meta: VarMeta; value: VarValue }
    | { op: "value";  name: string; value: VarValue }
    | { op: "unbind"; name: string };

export type UiMsg =
    | { op: "hello"; role: "ui" }
    | { op: "set";   name: string; value: VarValue };

export type ServerToUi =
    | { op: "registry"; vars: BoundVar[] }
    | { op: "value";    name: string; value: VarValue };

export type ServerToEngine =
    | { op: "set"; name: string; value: VarValue };
