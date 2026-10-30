import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import makeVisusPlugin from "../../../src/plugins/visus/index.js";
import { connect, quietContext } from "../../helpers/fake_ws.js";

// base/          <- must never be written
// base/root/     <- the only configured project root
let base = "";
let root = "";
const savedRoots = process.env.VISUS_PROJECT_ROOTS;

before(() => {
    base = mkdtempSync(join(tmpdir(), "ergo-visus-"));
    root = join(base, "root");
    mkdirSync(root);
    process.env.VISUS_PROJECT_ROOTS = root;
});

after(() => {
    if (savedRoots === undefined) delete process.env.VISUS_PROJECT_ROOTS;
    else process.env.VISUS_PROJECT_ROOTS = savedRoots;
    rmSync(base, { recursive: true, force: true });
});

function save(app: ReturnType<typeof routes>, path: string, content = "{\"name\":\"x\"}") {
    return app.request("/api/save", {
        method:  "POST",
        headers: { "content-type": "application/json" },
        body:    JSON.stringify({ root: 0, path, content }),
    });
}

function routes() {
    return makeVisusPlugin().routes!(quietContext());
}

test("save rejects traversal and absolute paths outside the root and writes nothing", async () => {
    const app = routes();
    const attempts = [
        "../escape.visus.json",
        "assets/../../escape.visus.json",
        join(base, "abs.visus.json"),
    ];
    for (const p of attempts) {
        const res = await save(app, p);
        assert.equal(res.status, 400, p);
        assert.equal(((await res.json()) as { err: string }).err, "path outside root");
    }
    assert.equal(existsSync(join(base, "escape.visus.json")), false);
    assert.equal(existsSync(join(base, "abs.visus.json")), false);

    const res = await app.request(`/api/load?root=0&path=${encodeURIComponent("../root/../x.visus.json")}`);
    assert.equal(res.status, 400);
});

test("save validates the extension, the root index and the JSON", async () => {
    const app = routes();
    assert.equal((await save(app, "a.json")).status, 400);
    assert.equal((await save(app, "a.visus.json", "not json")).status, 400);
    assert.equal(existsSync(join(root, "a.visus.json")), false);

    const badRoot = await app.request("/api/save", {
        method: "POST",
        body:   JSON.stringify({ root: 5, path: "a.visus.json", content: "{}" }),
    });
    assert.equal(badRoot.status, 400);
});

test("a valid save writes under the root, broadcasts and shows up in scan / load", async () => {
    const plugin = makeVisusPlugin();
    const app = plugin.routes!(quietContext());
    const client = connect(plugin);

    const rel = "assets/visus/hero.visus.json";
    const content = JSON.stringify({ name: "hero", geometry: { kind: "mesh" }, materials: [{}, {}] });
    const res = await save(app, rel, content);
    assert.equal(res.status, 200);
    assert.equal(readFileSync(join(root, rel), "utf-8"), content);
    assert.deepEqual(client.last("saved"), { op: "saved", root_index: 0, rel_path: rel });

    // An absolute path that stays inside the root is accepted too.
    const abs = join(root, "abs.visus.json");
    assert.equal((await save(app, abs)).status, 200);
    assert.equal(existsSync(abs), true);

    const scan = (await (await app.request("/api/scan?root=0")).json()) as {
        files: { rel_path: string; geometry_kind: string; materials_count: number }[];
    };
    const hero = scan.files.find((f) => f.rel_path === rel);
    assert.ok(hero);
    assert.equal(hero.geometry_kind, "mesh");
    assert.equal(hero.materials_count, 2);

    const load = await app.request(`/api/load?root=0&path=${encodeURIComponent(rel)}`);
    assert.equal(load.status, 200);
    assert.equal(((await load.json()) as { content: string }).content, content);
});
