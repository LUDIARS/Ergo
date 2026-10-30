import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import type { Hono } from "hono";

import makeUiLayoutPlugin from "../../../src/plugins/ui_layout/index.js";
import { quietContext } from "../../helpers/fake_ws.js";

// The plugin captures process.cwd() as its root when the factory runs.
// base/       <- must never be written
// base/root/  <- cwd while the factory runs
let base = "";
let root = "";
let app: Hono;
const savedCwd = process.cwd();

before(() => {
    base = mkdtempSync(join(tmpdir(), "ergo-ui-layout-"));
    root = join(base, "root");
    mkdirSync(root);
    process.chdir(root);
    try {
        app = makeUiLayoutPlugin().routes!(quietContext());
    } finally {
        process.chdir(savedCwd);
    }
});

after(() => {
    rmSync(base, { recursive: true, force: true });
});

function save(path: string, json = "{\"nodes\":[]}") {
    return app.request("/api/file/save", {
        method:  "POST",
        headers: { "content-type": "application/json" },
        body:    JSON.stringify({ path, json }),
    });
}

test("save rejects traversal, absolute paths outside the root and wrong extensions", async () => {
    const attempts = [
        "../escape.uilayout.json",
        "a/../../escape.uilayout.json",
        join(base, "abs.uilayout.json"),
        "layout.json",
        "",
    ];
    for (const p of attempts) {
        const res = await save(p);
        assert.equal(res.status, 400, p);
        assert.equal(((await res.json()) as { ok: boolean }).ok, false);
    }
    assert.equal(existsSync(join(base, "escape.uilayout.json")), false);
    assert.equal(existsSync(join(base, "abs.uilayout.json")), false);
    assert.equal(existsSync(join(root, "layout.json")), false);
});

test("open and svg refuse paths outside the root", async () => {
    writeFileSync(join(base, "secret.uilayout.json"), "{}");
    writeFileSync(join(base, "secret.svg"), "<svg/>");
    for (const url of [
        `/api/file/open?path=${encodeURIComponent("../secret.uilayout.json")}`,
        `/api/file/open?path=${encodeURIComponent(join(base, "secret.uilayout.json"))}`,
        `/api/file/svg?path=${encodeURIComponent("../secret.svg")}`,
        `/api/file/svg?path=${encodeURIComponent(join(root, "x.uilayout.json"))}`,
    ]) {
        const res = await app.request(url);
        assert.equal(res.status, 400, url);
    }
});

test("a valid save writes under the root and opens again", async () => {
    const json = JSON.stringify({ nodes: [{ id: "n1" }] });
    const res = await save("main.uilayout.json", json);
    assert.equal(res.status, 200);
    assert.equal(readFileSync(join(root, "main.uilayout.json"), "utf8"), json);

    const opened = await app.request(`/api/file/open?path=${encodeURIComponent(join(root, "main.uilayout.json"))}`);
    assert.equal(opened.status, 200);
    assert.equal(((await opened.json()) as { json: string }).json, json);
});
