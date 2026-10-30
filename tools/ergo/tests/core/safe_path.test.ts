import { test } from "node:test";
import assert from "node:assert/strict";
import path from "node:path";

import { resolveWithin } from "../../src/core/safe_path.js";

test("posix: paths inside the root resolve, escapes are rejected", () => {
    const p = path.posix;
    const root = "/proj";
    assert.equal(resolveWithin(root, "sub/ok.visus.json", p), "/proj/sub/ok.visus.json");
    assert.equal(resolveWithin(root, "./a/../b.json", p), "/proj/b.json");
    assert.equal(resolveWithin(root, "/proj/inner/x.json", p), "/proj/inner/x.json");
    assert.equal(resolveWithin(root, "", p), "/proj");
    assert.equal(resolveWithin(root, "..foo/x.json", p), "/proj/..foo/x.json", "a name starting with '..' is not an escape");

    assert.equal(resolveWithin(root, "../x", p), null);
    assert.equal(resolveWithin(root, "..", p), null);
    assert.equal(resolveWithin(root, "sub/../../x", p), null);
    assert.equal(resolveWithin(root, "/etc/x", p), null);
    assert.equal(resolveWithin(root, "/projector/x", p), null, "a sibling sharing the prefix is outside");
});

test("win32: cross-drive and UNC paths are rejected", () => {
    const w = path.win32;
    const root = "C:\\proj";
    assert.equal(resolveWithin(root, "sub\\ok.json", w), "C:\\proj\\sub\\ok.json");
    assert.equal(resolveWithin(root, "sub/ok.json", w), "C:\\proj\\sub\\ok.json");
    assert.equal(resolveWithin(root, "c:\\PROJ\\x.json", w), "c:\\PROJ\\x.json", "drive letters and case are insensitive");

    assert.equal(resolveWithin(root, "..\\x", w), null);
    assert.equal(resolveWithin(root, "../x", w), null);
    assert.equal(resolveWithin(root, "D:\\x", w), null);
    assert.equal(resolveWithin(root, "D:\\evil\\x.visus.json", w), null);
    assert.equal(resolveWithin(root, "\\\\srv\\share\\x", w), null);
    assert.equal(resolveWithin(root, "C:\\other\\x", w), null);
});

test("defaults to the host path module", () => {
    const root = path.resolve("some-root");
    assert.equal(resolveWithin(root, "a.json"), path.join(root, "a.json"));
    assert.equal(resolveWithin(root, `..${path.sep}a.json`), null);
});
