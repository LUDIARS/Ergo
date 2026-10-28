// @implements SPEC-ergo-studio
// @implements SPEC-ergo-studio-guard
/// Ergo Studio — デスクトップでゲームを AI と作るための画面。
///
/// LUDIARS の各サービスと Ergo のサーバ側で話し、画面 (ui/) はこのプラグインの
/// API (/studio/api/*) とだけ話す:
///   - プロジェクト: Concordia の登録プロジェクトと、AI への実装指示 (delegation)
///   - タスク: Actio のタスク管理と Breviarium の状態要約、タスクから Cc への実装依頼
///   - ソース: 各リポの git 状態と Revisor の PR、fetch / main の早送り
///   - リソース: Curare のアセット・版と、選んだ版のプロジェクトへの取得
///   - プラグイン: 許可した GitHub 取得元からのプラグインパック取得 (承認・リビジョン固定)
///   - 連携: 各サービスの生死・接続先・認証と、サービス間のつながりの図
///
/// 接続先は Excubitor の catalog / topology が正本 (net/endpoints.ts)。仕様は
/// spec/feature/tool/studio.md。

import { Hono } from "hono";

import type { Plugin, PluginFactory } from "../../core/plugin.js";
import { createStudioDeps } from "./deps.js";
import { checkMutation } from "./domain/request-guard.js";
import { connectionRoutes } from "./routes/connections.js";
import { pluginRoutes } from "./routes/plugins.js";
import { projectRoutes } from "./routes/projects.js";
import { resourceRoutes } from "./routes/resources.js";
import { sourceRoutes } from "./routes/sources.js";
import { taskRoutes } from "./routes/tasks.js";

const factory: PluginFactory = () => {
    const deps = createStudioDeps();

    const plugin: Plugin = {
        id:          "studio",
        title:       "Ergo Studio",
        icon:        "🎮",
        description: "LUDIARS のサービスと連携して、ゲームを AI と作る (プロジェクト・タスク・ソース・リソース・プラグイン・連携)。",
        staticRoot:  "./src/plugins/studio/ui",

        routes(ctx) {
            const app = new Hono();
            // 変更系は Ergo 自身の画面からの JSON 呼び出しに限る (domain/request-guard.ts)。
            app.use("/api/*", async (c, next) => {
                const decision = checkMutation({
                    method:      c.req.method,
                    host:        c.req.header("host"),
                    origin:      c.req.header("origin"),
                    contentType: c.req.header("content-type"),
                });
                if (!decision.ok) {
                    ctx.log("warn", `[studio] rejected ${c.req.method} ${c.req.path}: ${decision.reason}`);
                    return c.json({ error: "forbidden", message: decision.reason }, 403);
                }
                await next();
            });
            app.get("/api/config", (c) => c.json({
                home:         deps.cfg.home,
                actioAuth:    deps.cfg.actio.mode,
                curareToken:  deps.cfg.curareToken !== null,
                pluginOwners: deps.cfg.pluginOwners,
                ludiarsRoot:  deps.cfg.ludiarsRoot,
            }));
            for (const sub of [projectRoutes, taskRoutes, sourceRoutes, resourceRoutes, pluginRoutes, connectionRoutes]) {
                app.route("/", sub(deps));
            }
            return app;
        },

        health() {
            return { ok: true, actioAuth: deps.cfg.actio.mode, curareToken: deps.cfg.curareToken !== null };
        },
    };
    return plugin;
};

export default factory;
