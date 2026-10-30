// git 状態の読み取り・早送りの判定・連携図・設定・失敗の分類。

import { test } from "node:test";
import assert from "node:assert/strict";

import { loadStudioConfig } from "../../src/plugins/studio/config.js";
import { buildConnectionMap, stateOf, type ProbeResult } from "../../src/plugins/studio/domain/connection-map.js";
import { decideFastForward, parseBranchStatus } from "../../src/plugins/studio/domain/source-sync.js";
import { classifyStatus, summarizeError } from "../../src/plugins/studio/net/http.js";

const porcelain = [
    "# branch.oid 0123456789abcdef",
    "# branch.head main",
    "# branch.upstream origin/main",
    "# branch.ab +0 -3",
    "1 .M N... 100644 100644 100644 aaa bbb src/a.ts",
    "u UU N... 100644 100644 100644 100644 a b c conflict.ts",
    "? new.txt",
].join("\n");

test("porcelain v2 を読む", () => {
    assert.deepEqual(parseBranchStatus(porcelain),
        { branch: "main", upstream: "origin/main", ahead: 0, behind: 3, changed: 2, untracked: 1 });
    assert.equal(parseBranchStatus("# branch.head (detached)\n").branch, null);
});

test("早送りは clean な main が遅れているときだけ", () => {
    const clean = { branch: "main", upstream: "origin/main", ahead: 0, behind: 2, changed: 0, untracked: 4 };
    assert.deepEqual(decideFastForward(clean), { allowed: true, branch: "main" });
    assert.equal(decideFastForward({ ...clean, changed: 1 }).allowed, false);
    assert.equal(decideFastForward({ ...clean, ahead: 1 }).allowed, false);
    assert.equal(decideFastForward({ ...clean, behind: 0 }).allowed, false);
    assert.equal(decideFastForward({ ...clean, branch: "feat/x" }).allowed, false);
    assert.equal(decideFastForward({ ...clean, upstream: null }).allowed, false);
});

const probe = (key: ProbeResult["key"], ok: boolean, failure?: ProbeResult["failure"]): ProbeResult =>
    ({ key, url: ok ? "http://x" : null, ok, failure, checkedAt: "t", lastOkAt: null });

test("失敗の種類を図の状態に写す", () => {
    assert.equal(stateOf(probe("actio", true)), "ok");
    assert.equal(stateOf(probe("actio", false, "unauthorized")), "unauthorized");
    assert.equal(stateOf(probe("actio", false, "not_configured")), "not_configured");
    assert.equal(stateOf(probe("actio", false, "timeout")), "down");
    assert.equal(stateOf(probe("actio", false, "rejected")), "degraded");
    assert.equal(stateOf(undefined), "not_configured");
});

test("連携図: Studio と全サービスのノード、辺は両端が動いているときだけ active", () => {
    const map = buildConnectionMap([probe("concordia", true), probe("actio", true), probe("breviarium", false, "unreachable")]);
    assert.equal(map.nodes[0]!.id, "studio");
    assert.equal(map.nodes.length, 8);
    const ccActio = map.edges.find((e) => e.from === "concordia" && e.to === "actio")!;
    assert.equal(ccActio.active, true);
    const brActio = map.edges.find((e) => e.from === "breviarium" && e.to === "actio")!;
    assert.equal(brActio.active, false);
    assert.equal(map.nodes.find((n) => n.id === "curare")!.state, "not_configured");
});

test("設定: Actio はトークンがあれば token、無ければ local mode。取得元の既定は LUDIARS", () => {
    const local = loadStudioConfig({});
    assert.deepEqual(local.actio, { mode: "local" });
    assert.deepEqual(local.pluginOwners, ["LUDIARS"]);
    assert.equal(local.excubitorUrl, null);
    const token = loadStudioConfig({ ERGO_ACTIO_TOKEN: " t ", ERGO_PLUGIN_OWNERS: "LUDIARS, MyOrg", EXCUBITOR_URL: "http://127.0.0.1:1/",
        ERGO_STUDIO_TIMEOUT_MS: "abc" });
    assert.deepEqual(token.actio, { mode: "token", token: "t" });
    assert.deepEqual(token.pluginOwners, ["LUDIARS", "MyOrg"]);
    assert.equal(token.excubitorUrl, "http://127.0.0.1:1");
    assert.equal(token.requestTimeoutMs, 8000);
});

test("HTTP 失敗の分類と本文の要約", () => {
    assert.equal(classifyStatus(401), "unauthorized");
    assert.equal(classifyStatus(403), "forbidden");
    assert.equal(classifyStatus(404), "not_found");
    assert.equal(classifyStatus(409), "rejected");
    assert.equal(classifyStatus(503), "server_error");
    assert.equal(summarizeError(JSON.stringify({ error: "title is required" })), "title is required");
    assert.equal(summarizeError("<html>\n  boom </html>"), "<html> boom </html>");
    assert.equal(summarizeError("x".repeat(500)).length, 300);
});
