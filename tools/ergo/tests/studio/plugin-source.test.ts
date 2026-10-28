// プラグイン取得元の検査と台帳の読み取り。

import { test } from "node:test";
import assert from "node:assert/strict";
import { join } from "node:path";

import { enabledPackDirs, parseLedger, studioHome } from "../../src/core/plugin-ledger.js";
import { normalizeSubpath, packId, parsePluginSource } from "../../src/plugins/studio/domain/plugin-source.js";

const allow = ["LUDIARS"];

test("許可された owner の owner/repo:subpath[@rev] を読む", () => {
    const r = parsePluginSource("LUDIARS/AdventureCube:tools/ac-web/plugins@main", allow);
    assert.ok(r.ok);
    assert.deepEqual(r.source, { owner: "LUDIARS", repo: "AdventureCube", subpath: "tools/ac-web/plugins", rev: "main" });
    assert.equal(packId(r.source), "LUDIARS/AdventureCube:tools/ac-web/plugins");
});

test("rev 省略は HEAD、owner は大文字小文字を区別しない", () => {
    const r = parsePluginSource("ludiars/Ergo:plugins", allow);
    assert.ok(r.ok);
    assert.equal(r.source.rev, "HEAD");
});

test("許可リスト外・形の崩れ・危険な subpath / rev は拒否", () => {
    assert.equal(parsePluginSource("someone/repo:plugins", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/repo", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/repo:../../etc", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/repo:/abs", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/repo:a\\b", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/repo:plugins@a..b", allow).ok, false);
    assert.equal(parsePluginSource("LUDIARS/re po:plugins", allow).ok, false);
});

test("subpath の正規化", () => {
    assert.equal(normalizeSubpath("./tools/plugins/"), "tools/plugins");
    assert.equal(normalizeSubpath("C:/x"), null);
    assert.equal(normalizeSubpath("a//b"), null);
});

test("台帳は壊れた行を捨て、有効なパックだけを読み込み対象にする", () => {
    const ledger = parseLedger(JSON.stringify({
        version: 1,
        packs: [
            { id: "LUDIARS/A:p", owner: "LUDIARS", repo: "A", subpath: "p", rev: "a".repeat(40), checkoutDir: "/h/a", enabled: true, plugins: ["x", 3] },
            { id: "LUDIARS/B:p", owner: "LUDIARS", repo: "B", subpath: "p", rev: "b".repeat(40), checkoutDir: "/h/b", enabled: false },
            { id: 1 },
            null,
        ],
    }));
    assert.equal(ledger.packs.length, 2);
    assert.deepEqual(ledger.packs[0]!.plugins, ["x"]);
    assert.deepEqual(enabledPackDirs(ledger), [join("/h/a", "p")]);
});

test("壊れた台帳は空として扱う (起動を止めない)", () => {
    assert.deepEqual(parseLedger("{oops").packs, []);
});

test("Studio の置き場は ERGO_STUDIO_HOME を優先する", () => {
    assert.ok(studioHome({ ERGO_STUDIO_HOME: "/data/studio" }).replace(/\\/g, "/").endsWith("/data/studio"));
    assert.ok(studioHome({}).replace(/\\/g, "/").endsWith("/.ergo-studio"));
});
