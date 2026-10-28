// AI への指示の組み立て: 作業場所の固定・必須入力・タスクの結びつけ。

import { test } from "node:test";
import assert from "node:assert/strict";

import type { CcProject, CcTemplate } from "../../src/plugins/studio/clients/concordia.js";
import {
    buildInvokeRequest, repoSlugFromOrigin, runBelongsTo, taskPrompt,
} from "../../src/plugins/studio/domain/delegation-request.js";

const project: CcProject = { code: "KS", project: "KuzuSurvivors", repo_path: "D:/LUDIARS/KuzuSurvivors" };
const template: CcTemplate = {
    call_name: "opus-5-5", title: "実装委託", description: null, target_provider: "claude", model: "claude-opus-5-5",
    is_active: 1,
    input_schema: [
        { name: "task", type: "string", required: true },
        { name: "target_repo", type: "string", required: true },
        { name: "context_extra", type: "string", required: false },
    ],
};

test("cwd と target_repo はプロジェクトの repo に固定される", () => {
    const r = buildInvokeRequest(project, template, { callName: "opus-5-5", args: { task: " 敵を増やす " } }, "ergo-studio");
    assert.ok(r.ok);
    assert.equal(r.request.cwd, project.repo_path);
    assert.deepEqual(r.request.args, { task: "敵を増やす", target_repo: project.repo_path });
    assert.equal(r.request.project, "KS");
    assert.equal(r.request.triggered_by, "ergo-studio");
    assert.equal(r.request.taskflow_reference, undefined);
});

test("画面から渡した余計な入力は送らない", () => {
    const r = buildInvokeRequest(project, template,
        { callName: "opus-5-5", args: { task: "x", cwd: "C:/Windows", evil: 1 } }, "ergo-studio");
    assert.ok(r.ok);
    assert.deepEqual(Object.keys(r.request.args).sort(), ["target_repo", "task"]);
    assert.equal(r.request.cwd, project.repo_path);
});

test("必須入力が空なら送らない", () => {
    const r = buildInvokeRequest(project, template, { callName: "opus-5-5", args: { task: "   " } }, "ergo-studio");
    assert.equal(r.ok, false);
});

test("テンプレートの不一致・長すぎる追加指示・不正なタスク ID は拒否", () => {
    assert.equal(buildInvokeRequest(project, template, { callName: "other", args: { task: "x" } }, "s").ok, false);
    assert.equal(buildInvokeRequest(project, template,
        { callName: "opus-5-5", args: { task: "x" }, extraPrompt: "a".repeat(20001) }, "s").ok, false);
    assert.equal(buildInvokeRequest(project, template,
        { callName: "opus-5-5", args: { task: "x" }, actioTaskId: "../x" }, "s").ok, false);
});

test("タスクからの依頼は task に本文を入れ、同じ Actio タスクに結びつける", () => {
    const r = buildInvokeRequest(project, template,
        { callName: "opus-5-5", args: {}, taskText: "本文", actioTaskId: "abc-123", extraPrompt: "急ぎ" }, "s");
    assert.ok(r.ok);
    assert.equal(r.request.args.task, "本文");
    assert.equal(r.request.taskflow_reference, "actio:abc-123");
    assert.equal(r.request.extra_prompt, "急ぎ");
});

test("task 入力の無いテンプレートでは本文を追加指示の先頭に入れる", () => {
    const noTask: CcTemplate = { ...template, input_schema: [{ name: "target_repo", type: "string", required: true }] };
    const r = buildInvokeRequest(project, noTask,
        { callName: "opus-5-5", args: {}, taskText: "本文", extraPrompt: "補足" }, "s");
    assert.ok(r.ok);
    assert.equal(r.request.extra_prompt, "本文\n\n補足");
});

test("taskPrompt は題名・本文・Actio 参照を並べる", () => {
    assert.equal(taskPrompt({ id: "t1", title: "題" }), "題\n\n(Actio: actio:t1)");
    assert.equal(taskPrompt({ id: "t1", title: "題", description: " 本文 " }), "題\n\n本文\n\n(Actio: actio:t1)");
});

test("repo origin から owner/repo を取り出す", () => {
    assert.equal(repoSlugFromOrigin("git@github.com:LUDIARS/Pictor.git"), "LUDIARS/Pictor");
    assert.equal(repoSlugFromOrigin("https://github.com/LUDIARS/Ergo"), "LUDIARS/Ergo");
    assert.equal(repoSlugFromOrigin("https://github.com/LUDIARS/Ergo.git/"), "LUDIARS/Ergo");
    assert.equal(repoSlugFromOrigin("https://gitlab.com/a/b"), null);
    assert.equal(repoSlugFromOrigin(null), null);
});

test("run のプロジェクト判定は cwd か target_repo のパスで (区切り・大小文字を無視)", () => {
    assert.ok(runBelongsTo({ cwd: "D:\\LUDIARS\\KuzuSurvivors\\" }, "D:/LUDIARS/KuzuSurvivors"));
    assert.ok(runBelongsTo({ cwd: null, args: { target_repo: "d:/ludiars/kuzusurvivors" } }, "D:/LUDIARS/KuzuSurvivors"));
    assert.equal(runBelongsTo({ cwd: "D:/LUDIARS/Other" }, "D:/LUDIARS/KuzuSurvivors"), false);
});
