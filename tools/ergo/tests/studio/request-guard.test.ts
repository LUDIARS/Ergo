// 変更系 API を Ergo 自身の画面からの呼び出しに限る。

import { test } from "node:test";
import assert from "node:assert/strict";

import { checkMutation } from "../../src/plugins/studio/domain/request-guard.js";

const json = "application/json; charset=utf-8";

test("読み取りは通す", () => {
    assert.ok(checkMutation({ method: "GET", host: "evil.example", origin: "https://evil.example", contentType: undefined }).ok);
});

test("同じ host:port の画面からの JSON は通す", () => {
    assert.ok(checkMutation({ method: "POST", host: "localhost:5170", origin: "http://localhost:5170", contentType: json }).ok);
    assert.ok(checkMutation({ method: "POST", host: "127.0.0.1:5170", origin: "http://127.0.0.1:5170", contentType: json }).ok);
});

test("Origin の無い呼び出し (curl) は loopback の Host なら通す", () => {
    assert.ok(checkMutation({ method: "PATCH", host: "127.0.0.1:5170", origin: undefined, contentType: json }).ok);
});

test("別サイト・別ポート・loopback 以外の Host は拒否", () => {
    assert.equal(checkMutation({ method: "POST", host: "localhost:5170", origin: "https://evil.example", contentType: json }).ok, false);
    assert.equal(checkMutation({ method: "POST", host: "localhost:5170", origin: "http://localhost:3000", contentType: json }).ok, false);
    assert.equal(checkMutation({ method: "POST", host: "evil.example:5170", origin: undefined, contentType: json }).ok, false);
    assert.equal(checkMutation({ method: "POST", host: undefined, origin: undefined, contentType: json }).ok, false);
    assert.equal(checkMutation({ method: "POST", host: "localhost:5170", origin: "null", contentType: json }).ok, false);
});

test("JSON 以外の POST (フォーム送信) は拒否、DELETE は本文なしで可", () => {
    assert.equal(checkMutation({ method: "POST", host: "localhost:5170", origin: "http://localhost:5170",
        contentType: "application/x-www-form-urlencoded" }).ok, false);
    assert.equal(checkMutation({ method: "POST", host: "localhost:5170", origin: undefined, contentType: "text/plain" }).ok, false);
    assert.ok(checkMutation({ method: "DELETE", host: "localhost:5170", origin: "http://localhost:5170", contentType: undefined }).ok);
});

test("IPv6 loopback", () => {
    assert.ok(checkMutation({ method: "POST", host: "[::1]:5170", origin: "http://[::1]:5170", contentType: json }).ok);
});
