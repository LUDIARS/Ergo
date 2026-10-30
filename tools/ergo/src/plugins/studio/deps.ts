// @implements SPEC-ergo-studio
/// Studio の依存 (設定・接続先・各クライアント) を 1 か所で組み立てる。

import { ActioClient } from "./clients/actio.js";
import { BreviariumClient } from "./clients/breviarium.js";
import { ConcordiaClient } from "./clients/concordia.js";
import { CurareClient } from "./clients/curare.js";
import { GitClient } from "./clients/git.js";
import { RevisorClient } from "./clients/revisor.js";
import { loadStudioConfig, type StudioConfig } from "./config.js";
import { PluginInstaller } from "./domain/plugin-installer.js";
import { EndpointResolver } from "./net/endpoints.js";

export interface StudioDeps {
    cfg:       StudioConfig;
    endpoints: EndpointResolver;
    cc:        ConcordiaClient;
    actio:     ActioClient;
    br:        BreviariumClient;
    cu:        CurareClient;
    revisor:   RevisorClient;
    git:       GitClient;
    installer: PluginInstaller;
}

export function createStudioDeps(env: NodeJS.ProcessEnv = process.env): StudioDeps {
    const cfg = loadStudioConfig(env);
    const endpoints = new EndpointResolver(cfg);
    const t = cfg.requestTimeoutMs;
    return {
        cfg,
        endpoints,
        cc:        new ConcordiaClient(endpoints, t),
        actio:     new ActioClient(endpoints, cfg.actio, t),
        br:        new BreviariumClient(endpoints, t),
        cu:        new CurareClient(endpoints, cfg.curareToken, t),
        revisor:   new RevisorClient(cfg.ludiarsRoot, t),
        git:       new GitClient(t),
        installer: new PluginInstaller(cfg.home, t),
    };
}
