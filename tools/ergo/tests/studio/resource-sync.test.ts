// リソース取得の判定: 上書きしない・名前と取得先の安全。

import { test } from "node:test";
import assert from "node:assert/strict";
import { join, resolve } from "node:path";

import { decideSyncAction, resolveInside, safeFileName, toContentHash } from "../../src/plugins/studio/domain/resource-sync.js";

const H1 = toContentHash("a".repeat(64));
const H2 = toContentHash("b".repeat(64));

test("同じ内容があれば取得しない、別内容なら衝突 (上書きしない)", () => {
    const item = { contentHash: H1, needsDownload: true, downloadUrl: "https://x" };
    assert.deepEqual(decideSyncAction(item, H1), { action: "skip_same" });
    assert.deepEqual(decideSyncAction(item, H2), { action: "conflict", localHash: H2 });
    assert.deepEqual(decideSyncAction(item, null), { action: "download" });
});

test("取得が要るのに URL が無ければ取得不可、計画が要らないとしたものは取得しない", () => {
    assert.deepEqual(decideSyncAction({ contentHash: H1, needsDownload: true, downloadUrl: null }, null), { action: "unavailable" });
    assert.deepEqual(decideSyncAction({ contentHash: H1, needsDownload: false, downloadUrl: null }, null), { action: "skip_same" });
});

test("ファイル名は区切り・予約名・制御文字を避ける", () => {
    assert.equal(safeFileName("hero.fbx"), "hero.fbx");
    assert.equal(safeFileName("../../evil.dll"), ".._.._evil.dll");
    assert.equal(safeFileName("a\\b:c.png"), "a_b_c.png");
    assert.equal(safeFileName(".."), null);
    assert.equal(safeFileName("CON"), null);
    assert.equal(safeFileName("nul.txt"), null);
    assert.equal(safeFileName("   "), null);
});

test("取得先は repo の中の相対パスだけ", () => {
    const repo = resolve("/work/repo");
    assert.equal(resolveInside(repo, "assets/curare"), join(repo, "assets", "curare"));
    assert.equal(resolveInside(repo, ""), repo);
    assert.equal(resolveInside(repo, "../outside"), null);
    assert.equal(resolveInside(repo, "a/../../outside"), null);
    assert.equal(resolveInside(repo, resolve("/etc")), null);
});

test("内容ハッシュは Curare の書式 sha256:<hex>", () => {
    assert.equal(toContentHash("ABC"), "sha256:abc");
});
