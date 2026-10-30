// 接続先の解決: catalog からの Excubitor のポート、topology からのサービス URL。

import { test } from "node:test";
import assert from "node:assert/strict";

import { excubitorPortFromCatalog, pickEndpoints } from "../../src/plugins/studio/net/endpoints.js";

const catalog = [
    "services:",
    "  - code: excubitor-viewer-dmz",
    "    port: 17334",
    "  # comment",
    "  - code: excubitor",
    "    cwd: ${ARS_ROOT}/Excubitor",
    "    port: 17332   # 本体",
    "    ports:",
    "      - role: federation",
    "        port: 17335",
    "  - code: concordia",
    "    port: 11111",
].join("\r\n");

test("excubitor 本体の port だけを読む (dmz や拠点間 port に引っかからない)", () => {
    assert.equal(excubitorPortFromCatalog(catalog), 17332);
});

test("excubitor が無ければ null", () => {
    assert.equal(excubitorPortFromCatalog("services:\n  - code: concordia\n    port: 11111\n"), null);
});

test("topology の <CODE>_URL から Studio のサービスを選び、末尾の / を落とす", () => {
    const env = {
        CONCORDIA_URL: "http://localhost:11111/", CONCORDIA_PORT: "11111",
        ACTIO_URL: "http://127.0.0.1:17880", BREVIARIUM_URL: "http://localhost:4370",
        REVISOR_URL: "http://localhost:4400", CERNERE_URL: "http://localhost:8086",
        OTHER_URL: "http://localhost:1",
    };
    const { urls, all } = pickEndpoints(env, { curareUrl: null }, "http://127.0.0.1:17332");
    assert.equal(urls.concordia, "http://localhost:11111");
    assert.equal(urls.actio, "http://127.0.0.1:17880");
    assert.equal(urls.excubitor, "http://127.0.0.1:17332");
    assert.equal(urls.curare, null);
    assert.equal(all.OTHER, "http://localhost:1");
    assert.equal(all.CONCORDIA_PORT, undefined);
});

test("Curare は明示した ERGO_CURARE_URL を優先する", () => {
    const { urls } = pickEndpoints({ CURARE_URL: "http://localhost:9" }, { curareUrl: "https://cu.example" }, null);
    assert.equal(urls.curare, "https://cu.example");
    assert.equal(urls.excubitor, null);
});
