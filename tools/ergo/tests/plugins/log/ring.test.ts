import { test } from "node:test";
import assert from "node:assert/strict";

import { LineRing, RING_CAPACITY } from "../../../src/plugins/log/ring.js";
import type { LevelTag, LogLine } from "../../../src/plugins/log/protocol.js";

function line(n: number, lvl: LevelTag = "INF", msg = `line ${n}`): LogLine {
    return { lvl, frame: n, ts_us: n * 1000, tid: 1, msg };
}

test("seq numbers lines from 1 and the default capacity is 10k", () => {
    const ring = new LineRing();
    assert.equal(ring.capacity, RING_CAPACITY);
    assert.equal(RING_CAPACITY, 10_000);
    assert.equal(ring.lastSeq, 0);
    assert.equal(ring.firstSeq, 1);
    assert.deepEqual(ring.query(), []);

    const a = ring.push(line(10));
    const b = ring.push(line(11));
    assert.equal(a.seq, 1);
    assert.equal(b.seq, 2);
    assert.equal(ring.size, 2);
    assert.equal(ring.lastSeq, 2);
    assert.deepEqual(ring.query().map((l) => l.msg), ["line 10", "line 11"]);
});

test("a full ring evicts the oldest lines", () => {
    const ring = new LineRing(3);
    for (let i = 1; i <= 7; ++i) ring.push(line(i));
    assert.equal(ring.size, 3);
    assert.equal(ring.firstSeq, 5);
    assert.equal(ring.lastSeq, 7);
    assert.deepEqual(ring.query().map((l) => l.seq), [5, 6, 7]);
    assert.deepEqual(ring.query().map((l) => l.frame), [5, 6, 7]);
});

test("since returns only newer lines, also across eviction", () => {
    const ring = new LineRing(4);
    for (let i = 1; i <= 6; ++i) ring.push(line(i));   // holds seq 3..6
    assert.deepEqual(ring.query({ since: 4 }).map((l) => l.seq), [5, 6]);
    assert.deepEqual(ring.query({ since: 6 }), []);
    assert.deepEqual(ring.query({ since: 99 }), []);
    // Older than the ring: everything still held.
    assert.deepEqual(ring.query({ since: 0 }).map((l) => l.seq), [3, 4, 5, 6]);
    assert.deepEqual(ring.query({ since: 1 }).map((l) => l.seq), [3, 4, 5, 6]);
    assert.deepEqual(ring.query({ since: 4.7 }).map((l) => l.seq), [5, 6]);
});

test("level, text and frame filters combine", () => {
    const ring = new LineRing(16);
    ring.push(line(1, "ERR", "Disk full"));
    ring.push(line(2, "WRN", "slow frame"));
    ring.push(line(3, "INF", "loaded level"));
    ring.push(line(4, "DBG", "FRAME stats"));
    ring.push(line(5, "ERR", "frame budget blown"));

    const msgs = (f: Parameters<LineRing["query"]>[0]) => ring.query(f).map((l) => l.msg);
    assert.deepEqual(msgs({ levels: new Set<LevelTag>(["ERR"]) }), ["Disk full", "frame budget blown"]);
    assert.deepEqual(msgs({ q: "frame" }), ["slow frame", "FRAME stats", "frame budget blown"]);
    assert.deepEqual(msgs({ frameMin: 2, frameMax: 4 }), ["slow frame", "loaded level", "FRAME stats"]);
    assert.deepEqual(
        msgs({ levels: new Set<LevelTag>(["ERR", "DBG"]), q: "FRAME", frameMin: 4 }),
        ["FRAME stats", "frame budget blown"],
    );
    assert.deepEqual(msgs({ levels: new Set<LevelTag>() }), []);
});

test("limit keeps the newest matches, oldest first", () => {
    const ring = new LineRing(100);
    for (let i = 1; i <= 50; ++i) ring.push(line(i, i % 2 ? "INF" : "WRN"));
    const got = ring.query({ levels: new Set<LevelTag>(["WRN"]) }, 3);
    assert.deepEqual(got.map((l) => l.frame), [46, 48, 50]);
    assert.deepEqual(ring.query({ since: 45 }, 2).map((l) => l.seq), [49, 50]);
});

test("capacity must be a positive integer", () => {
    assert.throws(() => new LineRing(0), RangeError);
    assert.throws(() => new LineRing(2.5), RangeError);
});
