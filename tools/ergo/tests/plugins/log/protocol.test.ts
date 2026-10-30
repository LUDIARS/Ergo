import { test } from "node:test";
import assert from "node:assert/strict";

import {
    isLevelTag, parseConsoleLine, sanitizeLine, sanitizeLinesMsg,
} from "../../../src/plugins/log/protocol.js";

test("parseConsoleLine reads ergo_log's console format", () => {
    assert.deepEqual(parseConsoleLine("[F00012345][INF] player connected"),
        { frame: 12345, lvl: "INF", msg: "player connected" });
    assert.deepEqual(parseConsoleLine("[F00000000][ERR] boom: [x] = 1"),
        { frame: 0, lvl: "ERR", msg: "boom: [x] = 1" });
    // %08llu pads to 8 digits but larger frames simply get longer.
    assert.deepEqual(parseConsoleLine("[F123456789012][DBG] big"),
        { frame: 123456789012, lvl: "DBG", msg: "big" });
    // Line endings from a pipe are ignored; an empty payload is allowed.
    assert.deepEqual(parseConsoleLine("[F00000007][WRN] careful\r\n"),
        { frame: 7, lvl: "WRN", msg: "careful" });
    assert.deepEqual(parseConsoleLine("[F00000007][WRN] careful\r"),
        { frame: 7, lvl: "WRN", msg: "careful" });
    assert.deepEqual(parseConsoleLine("[F00000001][INF] "), { frame: 1, lvl: "INF", msg: "" });
    assert.deepEqual(parseConsoleLine("[F00000001][INF]"),  { frame: 1, lvl: "INF", msg: "" });
});

test("parseConsoleLine rejects other formats", () => {
    assert.equal(parseConsoleLine(""), null);
    assert.equal(parseConsoleLine("plain stdout text"), null);
    assert.equal(parseConsoleLine("[F00000001][INFO] long tag"), null);
    assert.equal(parseConsoleLine("[Fabc][INF] not a frame"), null);
    assert.equal(parseConsoleLine("  [F00000001][INF] indented"), null);
    assert.equal(parseConsoleLine("[F00000001][INF]no space"), null);
});

test("isLevelTag accepts only the four tags", () => {
    for (const t of ["ERR", "WRN", "INF", "DBG"]) assert.equal(isLevelTag(t), true);
    for (const t of ["err", "INFO", "", 2, null, undefined]) assert.equal(isLevelTag(t), false);
});

test("sanitizeLine keeps well-formed lines only", () => {
    const ok = { lvl: "WRN", frame: 3, ts_us: 1500, tid: 140234, msg: "hi" };
    assert.deepEqual(sanitizeLine(ok), ok);
    assert.deepEqual(sanitizeLine({ ...ok, extra: true }), ok);   // unknown keys dropped

    assert.equal(sanitizeLine(null), null);
    assert.equal(sanitizeLine("text"), null);
    assert.equal(sanitizeLine({ ...ok, lvl: "INFO" }), null);
    assert.equal(sanitizeLine({ ...ok, msg: 5 }), null);
    assert.equal(sanitizeLine({ ...ok, frame: "3" }), null);
    assert.equal(sanitizeLine({ ...ok, ts_us: Number.NaN }), null);
    const { tid: _tid, ...noTid } = ok;
    assert.equal(sanitizeLine(noTid), null);
});

test("sanitizeLinesMsg skips bad lines and normalizes dropped / app", () => {
    const good = { lvl: "INF", frame: 1, ts_us: 2, tid: 3, msg: "a" };
    const body = sanitizeLinesMsg({ op: "lines", app: "game", dropped: 4, lines: [good, { lvl: "?" }, 7] });
    assert.deepEqual(body, { app: "game", dropped: 4, lines: [good] });

    assert.deepEqual(sanitizeLinesMsg({ op: "lines", lines: [] }), { app: null, dropped: 0, lines: [] });
    assert.equal(sanitizeLinesMsg({ op: "lines", app: "", dropped: -3, lines: [] })!.dropped, 0);
    assert.equal(sanitizeLinesMsg({ op: "lines", app: "", lines: [] })!.app, null);
    assert.equal(sanitizeLinesMsg({ op: "lines", lines: "nope" }), null);
    assert.equal(sanitizeLinesMsg({ op: "lines" }), null);
});

test("a frame encoded by ergo_log_live passes the guards", () => {
    // Shape of ergo::log::encode_lines_frame() (jsonm: keys sorted).
    const frame = '{"app":"game","dropped":2,"lines":[{"frame":12,"lvl":"ERR","msg":"q\\"x\\ny","tid":140737,"ts_us":3456}],"op":"lines"}';
    const body = sanitizeLinesMsg(JSON.parse(frame));
    assert.deepEqual(body, {
        app: "game", dropped: 2,
        lines: [{ lvl: "ERR", frame: 12, ts_us: 3456, tid: 140737, msg: "q\"x\ny" }],
    });
});
