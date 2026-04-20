/// Wire protocol for variable-editor <-> ergo_bind.
///
/// All frames are JSON over WebSocket text. The server is the source of
/// truth: it holds a registry of currently-bound variables (each with
/// kind, meta, last-known value, and the connection that bound it) and
/// fans value updates out to subscribed UI clients.
///
/// Two roles connect to the same /ws endpoint and self-identify with
/// `hello`:
///   * "engine"  — an application that calls bind() in C++ (one app may
///                 bind many variables on the same connection)
///   * "ui"      — a browser editor that wants to display & edit the
///                 currently bound variables.

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
  name: string;
  kind: VarKind;
  meta: VarMeta;
  value: VarValue;
  app: string;       // identifier of the engine connection that owns this var
}

// ---- Engine -> Server ------------------------------------------------------
export type EngineMsg =
  | { op: "hello"; role: "engine"; app: string }
  | { op: "bind"; name: string; kind: VarKind; meta: VarMeta; value: VarValue }
  | { op: "value"; name: string; value: VarValue }   // owner reports a change
  | { op: "unbind"; name: string };

// ---- UI -> Server ----------------------------------------------------------
export type UiMsg =
  | { op: "hello"; role: "ui" }
  | { op: "set"; name: string; value: VarValue };    // edit request

// ---- Server -> UI ----------------------------------------------------------
export type ServerToUi =
  | { op: "registry"; vars: BoundVar[] }             // full snapshot on connect or change
  | { op: "value"; name: string; value: VarValue };  // single value update

// ---- Server -> Engine ------------------------------------------------------
export type ServerToEngine =
  | { op: "set"; name: string; value: VarValue };    // forward a UI edit to the owner
