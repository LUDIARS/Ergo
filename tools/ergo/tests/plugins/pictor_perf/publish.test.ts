import { test } from "node:test";
import assert from "node:assert/strict";

import makePictorPerfPlugin from "../../../src/plugins/pictor_perf/index.js";
import { connect, quietContext } from "../../helpers/fake_ws.js";

const SNAPSHOT = JSON.stringify({ memory: { streams: [] }, passes: [] });

test("HTTP publish validates the snapshot, keeps it and broadcasts it", async () => {
    const plugin = makePictorPerfPlugin();
    const logs: string[] = [];
    const app = plugin.routes!(quietContext(logs));

    const empty = await app.request("/api/snapshot");
    assert.equal(empty.status, 404);

    const client = connect(plugin);
    assert.deepEqual(client.sent, [{ op: "snapshot", snapshot: null, clients: 1 }]);

    for (const bad of ["not json", JSON.stringify({ passes: [] }), "null"]) {
        const res = await app.request("/api/publish", { method: "POST", body: bad });
        assert.equal(res.status, 400, bad);
    }
    assert.equal(logs.length, 3);
    assert.equal(client.sent.length, 1, "rejected snapshots are not broadcast");

    const ok = await app.request("/api/publish", { method: "POST", body: SNAPSHOT });
    assert.equal(ok.status, 200);
    assert.deepEqual(await ok.json(), { ok: true, clients: 1 });
    assert.deepEqual(client.last("snapshot"), { op: "snapshot", snapshot: SNAPSHOT, clients: 1 });

    const got = await app.request("/api/snapshot");
    assert.equal(got.status, 200);
    assert.equal(await got.text(), SNAPSHOT);
    assert.equal(plugin.health!().has_snapshot, true);
});

test("WS publish reaches every client; a new client gets the latest snapshot", () => {
    const plugin = makePictorPerfPlugin();
    const host = connect(plugin);
    const viewer = connect(plugin);

    host.receive({ op: "publish", snapshot: "{}" });   // no memory field: rejected
    assert.equal(viewer.ops("snapshot").length, 1);

    host.receive({ op: "publish", snapshot: SNAPSHOT });
    assert.deepEqual(viewer.last("snapshot"), { op: "snapshot", snapshot: SNAPSHOT, clients: 2 });
    assert.deepEqual(host.last("snapshot"), { op: "snapshot", snapshot: SNAPSHOT, clients: 2 });

    host.receive({ op: "ping" });
    assert.deepEqual(host.last("ack"), { op: "ack" });

    viewer.close();
    const late = connect(plugin);
    assert.deepEqual(late.sent, [{ op: "snapshot", snapshot: SNAPSHOT, clients: 2 }]);
});
