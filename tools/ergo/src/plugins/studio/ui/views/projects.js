// @implements SPEC-ergo-studio-instruct
// プロジェクト: Cc 登録プロジェクトの状態 (Br) と、AI への指示・委託の追跡、プロジェクト登録。

import { api, badge, errorBox, fmtTime, h, panel, selectedProject, toast } from "../lib.js";
import { templateForm } from "./template-form.js";

let runsListener = null;

const RUN_STATE = { completed: "ok", failed: "bad", cancelled: "off", running: "warn", launching: "warn", queued: "warn", pending: "warn" };

function registerPanel() {
  const code = h("input", { type: "text", placeholder: "コード (例: KS)" });
  const repo = h("input", { type: "text", placeholder: "リポジトリの絶対パス (ワークスペース内の git リポ)" });
  const name = h("input", { type: "text", placeholder: "プロジェクト名 (任意。省略時は検査結果から補完)" });
  const registerButton = h("button", { class: "primary" }, "Cc に登録");
  registerButton.addEventListener("click", async () => {
    registerButton.disabled = true;
    try {
      await api("/api/projects", { method: "POST", body: { code: code.value.trim(), repo_path: repo.value.trim(), project: name.value.trim() || undefined } });
      toast(`${code.value.trim()} を登録しました`);
      window.dispatchEvent(new Event("studio:projects-changed"));
    } catch (err) {
      toast(err.message, true);
    } finally {
      registerButton.disabled = false;
    }
  });
  return panel("プロジェクトを Cc に登録",
    h("p", { class: "muted" }, "登録したゲームは Cc のセッション・委託・Actio の同期で同じコードとして扱われます。"),
    h("div", { class: "field" }, code), h("div", { class: "field" }, repo), h("div", { class: "field" }, name), registerButton);
}

async function statusPanel(code) {
  const body = h("div", {}, h("p", { class: "muted" }, "Breviarium の状態を読み込み中…"));
  const refresh = h("button", {}, "Br を取り直す");
  refresh.addEventListener("click", async () => {
    refresh.disabled = true;
    try {
      await api(`/api/status/${encodeURIComponent(code)}/refresh`, { method: "POST", body: {} });
      toast("Breviarium を更新しました");
      await load();
    } catch (err) {
      toast(err.message, true);
    } finally {
      refresh.disabled = false;
    }
  });
  async function load() {
    try {
      const { overview } = await api(`/api/status/${encodeURIComponent(code)}`);
      const wf = overview?.workflow ?? {};
      body.replaceChildren(
        h("div", { class: "row" },
          badge("ok", wf.state?.label ?? wf.state ?? "状態不明"),
          wf.sprint ? h("span", {}, `スプリント: ${wf.sprint.name ?? ""}`) : null),
        h("details", {}, h("summary", { class: "muted" }, "概要 (JSON)"), h("pre", {}, JSON.stringify(overview, null, 2))));
    } catch (err) {
      body.replaceChildren(errorBox(err), h("p", { class: "muted" }, "Br に未登録のプロジェクトは状態が出ません。"));
    }
  }
  await load();
  return panel("プロジェクトの状態 (Breviarium)", body, refresh);
}

async function instructPanel(code) {
  let templates;
  try {
    ({ templates } = await api("/api/templates"));
  } catch (err) {
    return panel("AI に指示する", errorBox(err));
  }
  const form = templateForm(templates.filter((t) => !t.call_only));
  const send = h("button", { class: "primary" }, "Cc に委託する");
  const result = h("div", {});
  send.addEventListener("click", async () => {
    send.disabled = true;
    try {
      const { run } = await api(`/api/projects/${encodeURIComponent(code)}/instruct`, { method: "POST", body: form.value() });
      result.replaceChildren(h("p", {}, "委託しました: ", h("code", {}, run.run_id)));
      form.reset();
      window.dispatchEvent(new Event("studio:runs-changed"));
    } catch (err) {
      result.replaceChildren(errorBox(err));
    } finally {
      send.disabled = false;
    }
  });
  return panel(`AI に指示する (${code})`,
    h("p", { class: "muted" }, "作業場所はこのプロジェクトの repo に固定されます。Cc がセッションを起動し、Actio にタスクを記録します。"),
    form.element, send, result);
}

async function runsPanel(code) {
  const table = h("tbody", {});
  async function load() {
    try {
      const { runs: mine } = await api(`/api/runs?project=${encodeURIComponent(code)}`);
      table.replaceChildren(...(mine.length === 0
        ? [h("tr", {}, h("td", { colspan: 4, class: "muted" }, "Studio から出した委託はまだありません"))]
        : mine.map((r) => h("tr", {},
            h("td", {}, fmtTime(r.created_at)),
            h("td", {}, r.call_name),
            h("td", {}, badge(RUN_STATE[r.status] ?? "off", r.status)),
            h("td", {}, h("code", {}, r.id))))));
    } catch (err) {
      table.replaceChildren(h("tr", {}, h("td", { colspan: 4 }, errorBox(err))));
    }
  }
  // 再描画のたびに受け手を差し替える (古い画面の受け手を残さない)。
  if (runsListener) window.removeEventListener("studio:runs-changed", runsListener);
  runsListener = load;
  window.addEventListener("studio:runs-changed", runsListener);
  await load();
  const reload = h("button", {}, "更新");
  reload.addEventListener("click", load);
  return panel("委託の進み具合",
    h("table", {}, h("thead", {}, h("tr", {}, h("th", {}, "開始"), h("th", {}, "テンプレート"), h("th", {}, "状態"), h("th", {}, "run"))), table),
    reload);
}

export async function render(view) {
  const code = selectedProject();
  const left = h("div", {}, registerPanel());
  const right = h("div", {});
  view.append(h("div", { class: "grid" }, left, right));
  if (!code) {
    right.append(panel("プロジェクト", h("p", { class: "muted" }, "上のプロジェクトを選ぶと、状態・AI への指示・委託の一覧が出ます。")));
    return;
  }
  right.append(await instructPanel(code), await runsPanel(code));
  left.prepend(await statusPanel(code));
}
