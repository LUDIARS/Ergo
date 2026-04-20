/// variable-editor server.
///
/// Hosts a WS hub on /ws + a static UI on /. Tracks which engine connection
/// owns which variable so that UI edits get routed back to that owner only.

import { Hono } from "hono";
import { serve } from "@hono/node-server";
import { serveStatic } from "@hono/node-server/serve-static";
import { WebSocketServer, WebSocket as WS } from "ws";
import { createServer } from "node:http";

import type {
  BoundVar, EngineMsg, ServerToUi, ServerToEngine, UiMsg, VarValue,
} from "./protocol.js";

const PORT = Number(process.env.PORT) || 5174;

interface ConnState {
  ws: WS;
  role: "engine" | "ui" | "unknown";
  app: string;       // for engines
}

const conns = new Set<ConnState>();

// name -> bound variable
const registry = new Map<string, BoundVar & { ownerWs: WS }>();

function listVars(): BoundVar[] {
  return [...registry.values()].map(v => ({
    name: v.name, kind: v.kind, meta: v.meta, value: v.value, app: v.app,
  }));
}

function send(ws: WS, msg: ServerToUi | ServerToEngine) {
  if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
}

function broadcastRegistry() {
  const msg: ServerToUi = { op: "registry", vars: listVars() };
  const text = JSON.stringify(msg);
  for (const c of conns) {
    if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
  }
}

function broadcastValue(name: string, value: VarValue) {
  const msg: ServerToUi = { op: "value", name, value };
  const text = JSON.stringify(msg);
  for (const c of conns) {
    if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
  }
}

// ---- HTTP -----------------------------------------------------------------
const app = new Hono();
app.use("/*", serveStatic({ root: "./public" }));
app.get("/api/health", (c) =>
  c.json({ ok: true, vars: registry.size, clients: conns.size })
);
app.get("/api/vars", (c) => c.json(listVars()));

// ---- HTTP + WS -------------------------------------------------------------
const httpServer = createServer();
serve({ fetch: app.fetch, createServer: () => httpServer, port: PORT });

const wss = new WebSocketServer({ server: httpServer, path: "/ws" });

wss.on("connection", (ws) => {
  const state: ConnState = { ws, role: "unknown", app: "" };
  conns.add(state);

  ws.on("message", (raw) => {
    let msg: EngineMsg | UiMsg;
    try { msg = JSON.parse(raw.toString()); } catch { return; }
    if (!msg || typeof msg !== "object" || !("op" in msg)) return;

    // Identify role on first hello.
    if (msg.op === "hello") {
      if ("role" in msg && msg.role === "engine") {
        state.role = "engine";
        state.app  = (msg as any).app ?? "anonymous";
      } else if ("role" in msg && msg.role === "ui") {
        state.role = "ui";
        // Send the initial registry snapshot.
        send(ws, { op: "registry", vars: listVars() });
      }
      return;
    }

    // Engine messages
    if (state.role === "engine") {
      const em = msg as EngineMsg;
      if (em.op === "bind") {
        registry.set(em.name, {
          name: em.name, kind: em.kind, meta: em.meta, value: em.value,
          app: state.app, ownerWs: ws,
        });
        broadcastRegistry();
      } else if (em.op === "value") {
        const v = registry.get(em.name);
        if (v) {
          v.value = em.value;
          broadcastValue(em.name, em.value);
        }
      } else if (em.op === "unbind") {
        if (registry.delete(em.name)) broadcastRegistry();
      }
      return;
    }

    // UI messages
    if (state.role === "ui") {
      const um = msg as UiMsg;
      if (um.op === "set") {
        const v = registry.get(um.name);
        if (!v) return;
        // Forward the set to the owner. The owner will apply, then echo a
        // `value` message back, which we'll broadcast to all UIs.
        const fwd: ServerToEngine = { op: "set", name: um.name, value: um.value };
        if (v.ownerWs.readyState === WS.OPEN) v.ownerWs.send(JSON.stringify(fwd));
      }
    }
  });

  ws.on("close", () => {
    conns.delete(state);
    if (state.role === "engine") {
      // Drop all variables this engine owned.
      let dirty = false;
      for (const [name, v] of registry) {
        if (v.ownerWs === ws) { registry.delete(name); dirty = true; }
      }
      if (dirty) broadcastRegistry();
    }
  });

  ws.on("error", () => { /* close handler tidies up */ });
});

httpServer.listen(PORT, () => {
  console.log(`[variable-editor] http://localhost:${PORT}/`);
  console.log(`[variable-editor] ws://localhost:${PORT}/ws`);
});
