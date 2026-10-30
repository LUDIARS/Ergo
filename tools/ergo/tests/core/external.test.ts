// Plugin-pack loading (ERGO_PLUGIN_DIR) against synthetic packs built in a
// temporary directory. No game-specific code: every entry is generated here.

import { test, before, after, mock } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, delimiter, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { loadExternalFactories, loadPack, type ExternalFactory } from "../../src/core/external.js";
import { PLUGIN_FACTORIES } from "../../src/core/registry.js";
import { createApp, instantiatePlugins } from "../../src/core/server.js";
import { quietContext } from "../helpers/fake_ws.js";

let base  = "";
let pack1 = "";
let pack2 = "";

function entry(pack: string, name: string, file: string, code: string): void {
    mkdirSync(join(pack, name), { recursive: true });
    writeFileSync(join(pack, name, file), code);
}

before(() => {
    // Served static roots of the built-ins are cwd-relative (tools/ergo).
    process.chdir(fileURLToPath(new URL("../../", import.meta.url)));

    base  = mkdtempSync(join(tmpdir(), "ergo-packs-"));
    pack1 = join(base, "pack1");
    pack2 = join(base, "pack2");
    for (const p of [pack1, pack2]) {
        mkdirSync(p);
        writeFileSync(join(p, "package.json"), JSON.stringify({ type: "module" }));
    }

    entry(pack1, "good",         "index.js", `export default () => ({ id: "good", title: "Good", health: () => ({ ok: true }) });`);
    entry(pack1, "nodefault",    "index.js", `export const notAFactory = 1;`);
    entry(pack1, "importthrows", "index.js", `throw new Error("boom at import");`);
    entry(pack1, "ctorthrows",   "index.js", `export default () => { throw new Error("boom in factory"); };`);
    entry(pack1, "dupid",        "index.js", `export default () => ({ id: "particle", title: "Shadow particle" });`);
    entry(pack1, "both",         "index.js", `export default () => ({ id: "both-js", title: "JS wins" });`);
    entry(pack1, "both",         "index.ts", `export default (): { id: string; title: string } => ({ id: "both-ts", title: "TS" });`);
    mkdirSync(join(pack1, "empty"));
    writeFileSync(join(pack1, "README.md"), "not a plugin");

    entry(pack2, "second", "index.ts", `export default (): { id: string; title: string } => ({ id: "second", title: "Second" });`);
    entry(pack2, "badid",  "index.js", `export default () => ({ id: "Bad.Id", title: "Bad" });`);
});

after(() => {
    rmSync(base, { recursive: true, force: true });
});

/** Run `fn` with console output captured instead of printed. */
async function captured<T>(fn: () => Promise<T>): Promise<{ result: T; lines: string[] }> {
    const lines: string[] = [];
    const sink = (...args: unknown[]) => { lines.push(args.map(String).join(" ")); };
    const spies = [
        mock.method(console, "log",   sink),
        mock.method(console, "warn",  sink),
        mock.method(console, "error", sink),
    ];
    try {
        return { result: await fn(), lines };
    } finally {
        for (const s of spies) s.mock.restore();
    }
}

function entryName(e: ExternalFactory): string {
    return basename(dirname(e.source));
}

test("no directory configured gives no factories", async () => {
    assert.deepEqual(await loadExternalFactories(undefined), []);
    assert.deepEqual(await loadExternalFactories(""), []);
    assert.deepEqual(await loadExternalFactories(` ${delimiter} `), []);
});

test("a missing pack directory is reported and skipped", async () => {
    const { result, lines } = await captured(() => loadPack(join(base, "does-not-exist")));
    assert.deepEqual(result, []);
    assert.ok(lines.some((l) => l.includes("plugin dir not readable")));
});

test("every directory in the list is loaded; broken entries are skipped at import", async () => {
    const { result, lines } = await captured(() => loadExternalFactories(`${pack1}${delimiter}${pack2}`));

    assert.deepEqual(
        result.map(entryName).sort(),
        ["badid", "both", "ctorthrows", "dupid", "good", "second"],
    );
    assert.ok(lines.some((l) => l.includes("'nodefault'") && l.includes("no default PluginFactory")));
    assert.ok(lines.some((l) => l.includes("'importthrows'") && l.includes("boom at import")));

    // .js is preferred over .ts when a directory has both.
    const both = result.find((e) => entryName(e) === "both")!;
    assert.equal(basename(both.source), "index.js");
    assert.equal(both.factory().id, "both-js");

    // .ts entries load through tsx.
    const second = result.find((e) => entryName(e) === "second")!;
    assert.equal(basename(second.source), "index.ts");
    assert.equal(second.factory().id, "second");
});

test("broken or colliding pack entries never abort startup and never shadow a built-in", async () => {
    const { result: externals } = await captured(() => loadExternalFactories(`${pack1}${delimiter}${pack2}`));
    const logs: string[] = [];
    const ctx = quietContext(logs);

    const loaded = instantiatePlugins(PLUGIN_FACTORIES, externals, ctx.log);
    const { app, plugins, rejected } = createApp(loaded.plugins, loaded.rejected, ctx);

    assert.deepEqual(
        rejected.map((r) => basename(dirname(r.source))).sort(),
        ["badid", "ctorthrows", "dupid"],
    );
    const reasons = Object.fromEntries(rejected.map((r) => [basename(dirname(r.source)), r.reason]));
    assert.match(reasons.ctorthrows, /factory threw: boom in factory/);
    assert.match(reasons.dupid, /duplicate id 'particle'/);
    assert.match(reasons.badid, /invalid id 'Bad\.Id'/);
    assert.equal(logs.length, 3);
    assert.ok(logs.every((l) => l.startsWith("error: ")));

    const ids = plugins.map((p) => p.id);
    assert.equal(ids.length, PLUGIN_FACTORIES.length + 3);
    for (const id of ["good", "both-js", "second"]) assert.ok(ids.includes(id), id);
    assert.equal(ids.filter((id) => id === "particle").length, 1);

    // The built-in particle plugin still answers on its own routes.
    const effect = await app.request("/particle/api/effect");
    assert.equal(effect.status, 200);
    assert.ok("emission" in ((await effect.json()) as object));

    const health = (await (await app.request("/api/health")).json()) as {
        ok: boolean; rejected: { source: string; ok: boolean; err: string }[];
    };
    assert.equal(health.ok, false);
    assert.equal(health.rejected.length, 3);
    assert.ok(health.rejected.every((r) => r.ok === false && r.err.length > 0));
});
