import { Hono } from "hono";
import { WebSocket as WS } from "ws";
import { promises as fs } from "node:fs";
import path from "node:path";

import type { Plugin, PluginFactory } from "../../core/plugin.js";

interface BridgeConfig {
    patchUrl: string;
    pullUrl: string;
}

const factory: PluginFactory = () => {
    const cfg: BridgeConfig = {
        patchUrl: "http://127.0.0.1:8080/ui_layout/patch",
        pullUrl: "http://127.0.0.1:8080/ui_layout/state",
    };
    const clients = new Set<WS>();
    const rootDir = process.cwd();

    function broadcast(msg: unknown) {
        const raw = JSON.stringify(msg);
        for (const ws of clients) if (ws.readyState === WS.OPEN) ws.send(raw);
    }

    function normalizeUserPath(inputPath: string, ext = ".uilayout.json"): string {
        if (!inputPath.trim()) throw new Error("path is empty");
        const absPath = path.isAbsolute(inputPath) ? inputPath : path.join(rootDir, inputPath);
        const out = path.normalize(absPath);
        if (!out.endsWith(ext)) throw new Error(`file extension must be ${ext}`);
        // Containment guard: never read/write outside rootDir (path-traversal protection).
        const rel = path.relative(rootDir, out);
        if (rel.startsWith("..") || path.isAbsolute(rel)) throw new Error("path must stay within the tool root");
        return out;
    }

    const plugin: Plugin = {
        id: "ui_layout",
        title: "UI Layout",
        icon: "🧩",
        description: "Figma-like UI layout editor with canvas, hierarchy, properties, file I/O and custos bridge.",
        staticRoot: "./src/plugins/ui_layout/ui",
        routes() {
            const app = new Hono();

            app.get("/api/health", (c) => c.json({ ok: true, clients: clients.size, patchUrl: cfg.patchUrl, pullUrl: cfg.pullUrl }));
            app.get("/api/bridge/config", (c) => c.json(cfg));

            app.post("/api/bridge/config", async (c) => {
                const body = await c.req.json<Record<string, string>>();
                if (typeof body.patchUrl === "string" && body.patchUrl.length > 0) cfg.patchUrl = body.patchUrl;
                if (typeof body.pullUrl === "string" && body.pullUrl.length > 0) cfg.pullUrl = body.pullUrl;
                return c.json({ ok: true, patchUrl: cfg.patchUrl, pullUrl: cfg.pullUrl });
            });

            app.get("/api/file/open", async (c) => {
                try {
                    const filePath = normalizeUserPath(c.req.query("path") ?? "");
                    const json = await fs.readFile(filePath, "utf8");
                    return c.json({ ok: true, path: filePath, json });
                } catch (e) {
                    return c.json({ ok: false, error: (e as Error).message }, 400);
                }
            });

            app.post("/api/file/save", async (c) => {
                try {
                    const body = await c.req.json<{ path: string; json: string }>();
                    const filePath = normalizeUserPath(body.path);
                    await fs.writeFile(filePath, body.json, "utf8");
                    return c.json({ ok: true, path: filePath });
                } catch (e) {
                    return c.json({ ok: false, error: (e as Error).message }, 400);
                }
            });

            // vector ノードのサムネ用に参照 SVG を配信する。canvas が <img> で
            // 読み込んで実サムネを描く。拡張子 .svg + containment guard で保護。
            app.get("/api/file/svg", async (c) => {
                try {
                    const filePath = normalizeUserPath(c.req.query("path") ?? "", ".svg");
                    const svg = await fs.readFile(filePath, "utf8");
                    return c.body(svg, 200, { "content-type": "image/svg+xml; charset=utf-8" });
                } catch (e) {
                    return c.json({ ok: false, error: (e as Error).message }, 400);
                }
            });

            app.post("/api/bridge/patch", async (c) => {
                const patch = await c.req.text();
                const res = await fetch(cfg.patchUrl, {
                    method: "POST",
                    headers: { "content-type": "application/json" },
                    body: patch,
                }).catch(() => null);
                broadcast({ op: "patch", payload: patch });
                return c.json({ ok: !!res && res.ok, status: res?.status ?? 0 });
            });

            app.post("/api/bridge/pull", async (c) => {
                const res = await fetch(cfg.pullUrl, { method: "GET" }).catch(() => null);
                if (!res || !res.ok) return c.json({ ok: false, status: res?.status ?? 0 }, 502);
                const payload = await res.text();
                broadcast({ op: "document", payload });
                return c.json({ ok: true, payload });
            });

            return app;
        },
        onUpgrade(_req, ws) {
            clients.add(ws);
            ws.on("close", () => clients.delete(ws));
            // 未処理 error イベントで Node プロセスが落ちるのを防ぐ (particle と同作法)。
            ws.on("error", () => clients.delete(ws));
            ws.on("message", (raw) => {
                try {
                    const m = JSON.parse(raw.toString()) as { op?: string };
                    if (m?.op === "patch" || m?.op === "document") broadcast(m);
                } catch {
                    // ignore malformed payloads from clients
                }
            });
        },
        health() {
            return { ok: true, clients: clients.size, patchUrl: cfg.patchUrl, pullUrl: cfg.pullUrl };
        },
    };

    return plugin;
};

export default factory;
