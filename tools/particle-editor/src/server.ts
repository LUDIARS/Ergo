/// particle-editor server.
///
/// - Serves the static editor UI from `public/`.
/// - Hosts a WebSocket hub on the same port that mirrors the current
///   ParticleEffectConfig to all connected clients (browser editors,
///   engine clients via ergo_particle).
/// - Holds the canonical effect state in memory (single source of truth).
///   Persistence to disk is intentionally out of scope; clients can
///   download/import JSON via the browser UI.

import { Hono } from "hono";
import { serve } from "@hono/node-server";
import { serveStatic } from "@hono/node-server/serve-static";
import { WebSocketServer, WebSocket as WS } from "ws";
import { createServer } from "node:http";

import {
  DEFAULT_EFFECT,
  mergeConfig,
  SCHEMA_VERSION,
  type Inbound,
  type Outbound,
  type ParticleEffectConfig,
} from "./schema.js";

const PORT = Number(process.env.PORT) || 5173;

// ---- State ----------------------------------------------------------------
let current: ParticleEffectConfig = JSON.parse(JSON.stringify(DEFAULT_EFFECT));

// ---- HTTP -----------------------------------------------------------------
const app = new Hono();

// Static assets (the editor UI)
app.use("/*", serveStatic({ root: "./public" }));

// REST shortcuts — handy for engine clients that don't speak WS.
app.get("/api/effect", (c) => c.json(current));
app.post("/api/effect", async (c) => {
  const body = await c.req.json().catch(() => null);
  if (!body || typeof body !== "object") {
    return c.json({ ok: false, err: "json" }, 400);
  }
  current = mergeConfig(current, body);
  broadcastState();
  return c.json({ ok: true, config: current });
});

// Health
app.get("/api/health", (c) =>
  c.json({ ok: true, version: SCHEMA_VERSION, clients: clients.size })
);

// ---- HTTP + WS server -----------------------------------------------------
// Hono's Node adapter lets us hand it the underlying http.Server so we can
// also bolt a WebSocketServer onto the same port.
const httpServer = createServer();
serve({ fetch: app.fetch, createServer: () => httpServer, port: PORT });

const wss = new WebSocketServer({ server: httpServer, path: "/ws" });
const clients = new Set<WS>();

function send(ws: WS, msg: Outbound) {
  if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
}

function broadcastState() {
  const msg: Outbound = { op: "state", config: current, clients: clients.size };
  const text = JSON.stringify(msg);
  for (const c of clients) {
    if (c.readyState === WS.OPEN) c.send(text);
  }
}

wss.on("connection", (ws) => {
  clients.add(ws);
  // Send the current state immediately so the new client paints correctly.
  send(ws, { op: "state", config: current, clients: clients.size });
  // Notify others that the client count changed.
  for (const c of clients) {
    if (c !== ws && c.readyState === WS.OPEN) {
      c.send(JSON.stringify({ op: "state", config: current, clients: clients.size }));
    }
  }

  ws.on("message", (raw) => {
    let msg: Inbound;
    try {
      msg = JSON.parse(raw.toString());
    } catch {
      return; // ignore garbage
    }
    if (!msg || typeof msg !== "object" || !("op" in msg)) return;

    switch (msg.op) {
      case "set":
        current = mergeConfig(current, msg.config ?? {});
        broadcastState();
        break;
      case "replace":
        if (msg.config && typeof msg.config === "object") {
          current = mergeConfig(DEFAULT_EFFECT, msg.config);
          broadcastState();
        }
        break;
      case "ping":
        send(ws, { op: "ack" });
        break;
    }
  });

  ws.on("close", () => {
    clients.delete(ws);
    // Update remaining clients on count change.
    for (const c of clients) {
      if (c.readyState === WS.OPEN) {
        c.send(JSON.stringify({ op: "state", config: current, clients: clients.size }));
      }
    }
  });

  ws.on("error", () => { /* swallow; close handler runs */ });
});

httpServer.listen(PORT, () => {
  console.log(`[particle-editor] http://localhost:${PORT}/`);
  console.log(`[particle-editor] ws://localhost:${PORT}/ws`);
});
