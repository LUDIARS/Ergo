/// Visus plugin — 各プロジェクトの `assets/visus/*.visus.json` を scan + 編集する。
///
/// Visus は Pictor 側で定義される描画定義レイヤー (material より上位、 ObjectDescriptor
/// 1 単位の作り方レシピ)。 各 LUDIARS ゲームは `assets/visus/{name}.visus.json` を
/// 持ち、 起動時に Pictor の `VisusRegistry` へロードする。
///
/// プロトコル (Phase 1):
///   GET  /visus/api/roots                       → 設定済 project root の一覧
///   GET  /visus/api/scan?root=<idx>             → root 内の *.visus.json をリスト
///   GET  /visus/api/load?root=<idx>&path=<rel>  → 個別 visus を読み込み (生 JSON)
///   POST /visus/api/save  body:{root,path,content} → JSON pretty-print で書込み
///   GET  /visus/api/health                      → 標準
///   WS   /visus/ws                              → save 通知 broadcast (live reload)
///
/// project root の設定:
///   環境変数 `VISUS_PROJECT_ROOTS` に絶対パスをセミコロン区切りで指定。
///   未設定なら cwd だけを root として扱う。 path traversal 防止のため、 全 API
///   は指定 root 配下のパスしか受け付けない。

import { Hono } from "hono";
import { WebSocket as WS } from "ws";
import { promises as fsp, existsSync } from "node:fs";
import { join, resolve, relative, sep } from "node:path";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

interface VisusSummary {
    rel_path:        string;
    abs_path:        string;
    name:            string;
    geometry_kind:   string;
    materials_count: number;
    bytes:           number;
    mtime:           string;
}

function configuredRoots(): string[] {
    const raw = process.env.VISUS_PROJECT_ROOTS ?? "";
    const parts = raw.split(";").map((s) => s.trim()).filter(Boolean);
    if (parts.length === 0) return [resolve(process.cwd())];
    return parts.map((p) => resolve(p));
}

function rootAt(idx: number): string | null {
    const roots = configuredRoots();
    if (!Number.isFinite(idx) || idx < 0 || idx >= roots.length) return null;
    return roots[idx];
}

/// 与えられた相対パスが root 配下に収まることを検証して絶対パスに解決する。
/// `..` でエスケープしようとしたら null。
function resolveSafe(root: string, rel: string): string | null {
    const target = resolve(root, rel);
    const rel2   = relative(root, target);
    if (rel2.startsWith("..") || rel2.startsWith(sep + "..") || rel2 === ".." ||
        (rel2.length >= 1 && rel2[0] === sep))
    {
        return null;
    }
    return target;
}

async function listVisusFiles(root: string): Promise<VisusSummary[]> {
    const out: VisusSummary[] = [];

    async function walk(dir: string) {
        let entries;
        try {
            entries = await fsp.readdir(dir, { withFileTypes: true });
        } catch {
            return;
        }
        for (const ent of entries) {
            if (ent.name.startsWith(".") || ent.name === "node_modules" ||
                ent.name === "build" || ent.name === "dist")
            {
                continue;
            }
            const full = join(dir, ent.name);
            if (ent.isDirectory()) {
                await walk(full);
            } else if (ent.isFile() && ent.name.endsWith(".visus.json")) {
                try {
                    const buf = await fsp.readFile(full, "utf-8");
                    const st  = await fsp.stat(full);
                    const obj = JSON.parse(buf);
                    out.push({
                        rel_path:        relative(root, full).replaceAll(sep, "/"),
                        abs_path:        full,
                        name:            String(obj.name ?? ""),
                        geometry_kind:   String(obj.geometry?.kind ?? "none"),
                        materials_count: Array.isArray(obj.materials) ? obj.materials.length : 0,
                        bytes:           st.size,
                        mtime:           st.mtime.toISOString(),
                    });
                } catch {
                    // 壊れた visus は summary に出さない (UI 側で気付けない欠点はあるが
                    // Phase 1 は素朴に。 必要なら後で error 列を返す)。
                }
            }
        }
    }

    await walk(root);
    out.sort((a, b) => a.rel_path.localeCompare(b.rel_path));
    return out;
}

const factory: PluginFactory = () => {
    const clients = new Set<WS>();

    function broadcast(payload: unknown) {
        const text = JSON.stringify(payload);
        for (const c of clients) {
            if (c.readyState === WS.OPEN) c.send(text);
        }
    }

    const plugin: Plugin = {
        id:          "visus",
        title:       "Visus Editor",
        icon:        "👁",
        description: "各プロジェクトの assets/visus/*.visus.json を scan + 編集 (Pictor 描画定義)。",
        staticRoot:  "./src/plugins/visus/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/roots", (c) => {
                return c.json({
                    roots: configuredRoots().map((p, i) => ({ index: i, path: p })),
                });
            });

            app.get("/api/scan", async (c) => {
                const idx  = Number(c.req.query("root") ?? "0");
                const root = rootAt(idx);
                if (!root) return c.json({ ok: false, err: "invalid root index" }, 400);
                if (!existsSync(root)) return c.json({ ok: false, err: `root not found: ${root}` }, 404);
                const files = await listVisusFiles(root);
                return c.json({ ok: true, root, files });
            });

            app.get("/api/load", async (c) => {
                const idx  = Number(c.req.query("root") ?? "0");
                const rel  = c.req.query("path") ?? "";
                const root = rootAt(idx);
                if (!root) return c.json({ ok: false, err: "invalid root index" }, 400);
                if (!rel)  return c.json({ ok: false, err: "missing path"        }, 400);
                const full = resolveSafe(root, rel);
                if (!full) return c.json({ ok: false, err: "path outside root"   }, 400);
                try {
                    const buf = await fsp.readFile(full, "utf-8");
                    return c.json({ ok: true, root_index: idx, rel_path: rel, content: buf });
                } catch (e) {
                    return c.json({ ok: false, err: String(e) }, 404);
                }
            });

            app.post("/api/save", async (c) => {
                let body: any;
                try { body = await c.req.json(); }
                catch { return c.json({ ok: false, err: "invalid JSON body" }, 400); }

                const idx     = Number(body?.root ?? 0);
                const rel     = String(body?.path ?? "");
                const content = String(body?.content ?? "");
                const root    = rootAt(idx);
                if (!root)    return c.json({ ok: false, err: "invalid root index" }, 400);
                if (!rel)     return c.json({ ok: false, err: "missing path"        }, 400);
                if (!rel.endsWith(".visus.json")) {
                    return c.json({ ok: false, err: "path must end with .visus.json" }, 400);
                }
                const full = resolveSafe(root, rel);
                if (!full) return c.json({ ok: false, err: "path outside root" }, 400);

                // 構文だけ検証 (schema は Pictor 側が permissive なので緩く)
                try { JSON.parse(content); }
                catch (e) { return c.json({ ok: false, err: `not valid JSON: ${e}` }, 400); }

                try {
                    await fsp.mkdir(join(full, ".."), { recursive: true });
                    await fsp.writeFile(full, content, "utf-8");
                    ctx.log("info", `[visus] saved ${full}`);
                    broadcast({ op: "saved", root_index: idx, rel_path: rel });
                    return c.json({ ok: true });
                } catch (e) {
                    return c.json({ ok: false, err: String(e) }, 500);
                }
            });

            app.get("/api/health", (c) => {
                const roots = configuredRoots();
                return c.json({
                    ok:       true,
                    version:  SCHEMA_VERSION,
                    roots:    roots.length,
                    clients:  clients.size,
                });
            });

            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            clients.add(ws);
            ws.on("message", (raw: any) => {
                let msg: any;
                try { msg = JSON.parse(raw.toString()); } catch { return; }
                if (msg?.op === "ping") {
                    if (ws.readyState === WS.OPEN) ws.send(JSON.stringify({ op: "ack" }));
                }
            });
            ws.on("close", () => clients.delete(ws));
            ws.on("error", () => {});
        },

        health() {
            return {
                ok:       true,
                version:  SCHEMA_VERSION,
                roots:    configuredRoots().length,
                clients:  clients.size,
            };
        },
    };

    return plugin;
};

export default factory;
