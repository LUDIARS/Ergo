// @implements SPEC-ergo-studio-tasks
// タスク: Actio のタスク (選択中プロジェクトで絞る) の作成・状態変更と、Cc への実装依頼。

import { api, badge, errorBox, fmtTime, h, panel, selectedProject, toast } from "../lib.js";
import { templateForm } from "./template-form.js";

const STATUSES = [
  ["open", "未着手"], ["in_progress", "進行中"], ["blocked", "停止"], ["done", "完了"], ["cancelled", "取り下げ"],
];
const STATUS_BADGE = { open: "off", in_progress: "warn", blocked: "bad", done: "ok", cancelled: "off" };

function createPanel(code, onCreated) {
  const title = h("input", { type: "text", placeholder: "タスクの題名" });
  const desc = h("textarea", { placeholder: "目的・対象・受入条件" });
  const priority = h("select", {}, h("option", { value: "" }, "優先度 (既定)"),
    ...["low", "medium", "high", "critical"].map((p) => h("option", { value: p }, p)));
  const createButton = h("button", { class: "primary" }, "Actio に登録");
  createButton.addEventListener("click", async () => {
    createButton.disabled = true;
    try {
      await api("/api/tasks", { method: "POST", body: {
        title: title.value.trim(), description: desc.value, priority: priority.value || undefined, projectId: code ?? undefined,
      } });
      title.value = ""; desc.value = "";
      toast("タスクを登録しました");
      await onCreated();
    } catch (err) {
      toast(err.message, true);
    } finally {
      createButton.disabled = false;
    }
  });
  return panel(code ? `タスクを追加 (${code})` : "タスクを追加",
    h("div", { class: "field" }, title), h("div", { class: "field" }, desc), h("div", { class: "row" }, priority, createButton));
}

function implementPanel(code, templates) {
  const form = templateForm(templates.filter((t) => !t.call_only && (t.input_schema ?? []).every((f) => !f.required || ["task", "target_repo"].includes(f.name))), { hideTask: true });
  const target = h("p", { class: "muted" }, "左の一覧で「AI に実装させる」を押したタスクがここに入ります。");
  const send = h("button", { class: "primary", disabled: true }, "Cc に実装させる");
  let taskId = null;
  send.addEventListener("click", async () => {
    if (!taskId || !code) return;
    send.disabled = true;
    try {
      const v = form.value();
      const { run } = await api(`/api/tasks/${encodeURIComponent(taskId)}/implement`, { method: "POST", body: { code, call_name: v.call_name, extra_prompt: v.extra_prompt } });
      toast(`委託しました (run ${run.run_id})`);
      form.reset();
    } catch (err) {
      toast(err.message, true);
    } finally {
      send.disabled = false;
    }
  });
  return {
    element: panel("タスクを AI に実装させる",
      h("p", { class: "muted" }, "タスクの本文を依頼として Cc に委託し、同じ Actio タスクに結びつけます (新しいタスクは作りません)。"),
      target, form.element, send),
    choose(task) {
      taskId = task.id;
      target.textContent = `対象: ${task.title}`;
      send.disabled = !code;
      if (!code) target.textContent += " (プロジェクトを選ぶと依頼できます)";
    },
  };
}

export async function render(view) {
  const code = selectedProject();
  const includeDone = h("input", { type: "checkbox" });
  const tbody = h("tbody", {});
  let templates = [];
  try { ({ templates } = await api("/api/templates")); } catch (err) { toast(err.message, true); }
  const impl = implementPanel(code, templates);

  async function load() {
    try {
      const q = new URLSearchParams();
      if (code) q.set("project", code);
      if (includeDone.checked) q.set("done", "1");
      const { tasks } = await api(`/api/tasks?${q}`);
      tbody.replaceChildren(...(tasks.length === 0
        ? [h("tr", {}, h("td", { colspan: 4, class: "muted" }, "タスクはありません"))]
        : tasks.map((t) => {
            const status = h("select", {}, ...STATUSES.map(([v, l]) => h("option", { value: v }, l)));
            status.value = t.status;
            status.addEventListener("change", async () => {
              try {
                await api(`/api/tasks/${encodeURIComponent(t.id)}`, { method: "PATCH", body: { status: status.value } });
                toast("状態を更新しました");
              } catch (err) {
                toast(err.message, true);
                status.value = t.status;
              }
            });
            const go = h("button", {}, "AI に実装させる");
            go.addEventListener("click", () => impl.choose(t));
            return h("tr", {},
              h("td", {}, badge(STATUS_BADGE[t.status] ?? "off", t.status), " ", t.title,
                t.description ? h("div", { class: "muted" }, t.description.slice(0, 160)) : null),
              h("td", {}, status),
              h("td", { class: "muted" }, fmtTime(t.updatedAt ?? t.deadline)),
              h("td", {}, go));
          })));
    } catch (err) {
      tbody.replaceChildren(h("tr", {}, h("td", { colspan: 4 }, errorBox(err))));
    }
  }
  includeDone.addEventListener("change", load);

  view.append(h("div", { class: "grid" },
    h("div", {}, createPanel(code, load), impl.element),
    panel(code ? `Actio のタスク (${code})` : "Actio のタスク (全プロジェクト)",
      h("label", { class: "row muted" }, includeDone, "完了・取り下げも表示"),
      h("table", {}, h("thead", {}, h("tr", {}, h("th", {}, "タスク"), h("th", {}, "状態"), h("th", {}, "更新"), h("th", {}))), tbody))));
  await load();
}
