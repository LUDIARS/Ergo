// @implements SPEC-ergo-studio-sources
// ソース: 登録プロジェクトの git 状態一覧と、選択プロジェクトの Revisor PR・同期。
// main への反映は Revisor の local PR (審査・マージ) が担う。ここでは取得と早送りだけ。

import { api, badge, errorBox, fmtTime, h, panel, selectedProject, setSelectedProject, toast } from "../lib.js";

const PR_STATE = { merged: "ok", open: "warn", reviewing: "warn", closed: "off" };
const CHECK_STATE = { test_ok: "ok", test_ng: "bad", running: "warn" };

function statusCell(s) {
  if (!s) return h("span", { class: "muted" }, "—");
  const parts = [badge(s.changed > 0 ? "warn" : "ok", s.branch ?? "detached")];
  if (s.ahead) parts.push(" ", badge("warn", `+${s.ahead}`));
  if (s.behind) parts.push(" ", badge("warn", `-${s.behind}`));
  if (s.changed) parts.push(" ", h("span", { class: "muted" }, `変更 ${s.changed}`));
  if (s.untracked) parts.push(" ", h("span", { class: "muted" }, `未追跡 ${s.untracked}`));
  return h("span", {}, ...parts);
}

async function detailPanel(code) {
  const box = h("div", {}, h("p", { class: "muted" }, "読み込み中…"));
  async function load() {
    try {
      const d = await api(`/api/sources/${encodeURIComponent(code)}`);
      const fetchBtn = h("button", {}, "取得 (fetch)");
      const ffBtn = h("button", { class: "primary", disabled: !d.fastForward?.allowed }, "main を早送り");
      fetchBtn.addEventListener("click", () => act("fetch", fetchBtn));
      ffBtn.addEventListener("click", () => act("fast-forward", ffBtn));
      box.replaceChildren(
        h("div", { class: "row" }, statusCell(d.status),
          d.repository ? h("span", { class: "muted" }, d.repository) : null,
          h("span", { class: "muted" }, `workflow: ${d.workflow ?? "未設定"}`)),
        d.statusError ? errorBox(d.statusError) : null,
        d.status?.lastCommit ? h("p", { class: "muted" }, `最新: ${d.status.lastCommit.sha} ${d.status.lastCommit.subject} (${fmtTime(d.status.lastCommit.date)})`) : null,
        h("div", { class: "row" }, fetchBtn, ffBtn,
          d.fastForward && !d.fastForward.allowed ? h("span", { class: "muted" }, d.fastForward.reason) : null),
        h("h2", {}, "Revisor の PR"),
        d.prsError ? errorBox(d.prsError) : null,
        h("table", {}, h("thead", {}, h("tr", {}, h("th", {}, "#"), h("th", {}, "題名"), h("th", {}, "状態"), h("th", {}, "審査"), h("th", {}, "更新"))),
          h("tbody", {}, ...(d.prs.length === 0 ? [h("tr", {}, h("td", { colspan: 5, class: "muted" }, "PR はありません"))]
            : d.prs.map((p) => h("tr", {},
                h("td", {}, `#${p.number}`),
                h("td", {}, p.title, h("div", { class: "muted" }, `${p.headRef} → ${p.baseRef}`)),
                h("td", {}, badge(PR_STATE[p.status] ?? "off", p.status)),
                h("td", {}, p.checkStatus ? badge(CHECK_STATE[p.checkStatus] ?? "off", p.checkStatus) : "—"),
                h("td", { class: "muted" }, fmtTime(p.updatedAt ?? p.createdAt))))))));
    } catch (err) {
      box.replaceChildren(errorBox(err));
    }
  }
  async function act(kind, btn) {
    btn.disabled = true;
    try {
      const r = await api(`/api/sources/${encodeURIComponent(code)}/${kind}`, { method: "POST", body: {} });
      toast(r.message);
    } catch (err) {
      toast(err.message, true);
    }
    await load();
  }
  await load();
  return panel(`ソース (${code})`, box);
}

export async function render(view) {
  const code = selectedProject();
  const tbody = h("tbody", {}, h("tr", {}, h("td", { colspan: 3, class: "muted" }, "git の状態を集めています…")));
  view.append(h("div", { class: "grid" },
    panel("登録プロジェクトの git 状態",
      h("table", {}, h("thead", {}, h("tr", {}, h("th", {}, "コード"), h("th", {}, "状態"), h("th", {}, "最新コミット"))), tbody)),
    code ? await detailPanel(code) : panel("ソース", h("p", { class: "muted" }, "一覧の行を選ぶと PR と同期の操作が出ます。"))));
  try {
    const { sources } = await api("/api/sources");
    tbody.replaceChildren(...sources.map((s) => {
      const tr = h("tr", { class: `selectable${s.code === code ? " selected" : ""}` },
        h("td", {}, s.code),
        h("td", {}, s.error ? h("span", { class: "error" }, s.error) : statusCell(s.status)),
        h("td", { class: "muted" }, s.status?.lastCommit ? `${s.status.lastCommit.sha} ${fmtTime(s.status.lastCommit.date)}` : "—"));
      tr.addEventListener("click", () => {
        setSelectedProject(s.code);
        document.getElementById("project-select").value = s.code;
      });
      return tr;
    }));
  } catch (err) {
    tbody.replaceChildren(h("tr", {}, h("td", { colspan: 3 }, errorBox(err))));
  }
}
