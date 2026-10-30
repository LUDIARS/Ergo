// @implements SPEC-ergo-studio-resources
// リソース: Curare のプロジェクト → フォルダ → アセット → 版 を選び、選択中の Cc プロジェクトの
// repo 内フォルダへ取得する。手元に同名で別内容のファイルがあれば上書きせず「衝突」と出す。

import { api, badge, errorBox, fmtTime, h, panel, selectedProject, toast } from "../lib.js";

const RESULT = { downloaded: ["ok", "取得"], skip_same: ["off", "同じ内容あり"], conflict: ["bad", "衝突 (上書きしない)"],
  unavailable: ["bad", "取得 URL 無し"], failed: ["bad", "失敗"], invalid_name: ["bad", "名前が使えない"] };

function list(items, label, onPick) {
  if (items.length === 0) return h("p", { class: "muted" }, `${label}はありません`);
  return h("table", {}, h("tbody", {}, ...items.map((it) => {
    const tr = h("tr", { class: "selectable" }, h("td", {}, it.name ?? it.id));
    tr.addEventListener("click", () => {
      for (const s of tr.parentElement.children) s.classList.remove("selected");
      tr.classList.add("selected");
      onPick(it);
    });
    return tr;
  })));
}

export async function render(view) {
  const code = selectedProject();
  const projectsBox = h("div", {}, h("p", { class: "muted" }, "Curare を読み込み中…"));
  const foldersBox = h("div", {});
  const assetsBox = h("div", {});
  const versionsBox = h("div", {});
  const chosen = new Map();   // versionId → 表示名
  const chosenList = h("ul", {});
  const subdir = h("input", { type: "text", value: "assets/curare", placeholder: "repo 内の取得先 (相対パス)" });
  const syncBtn = h("button", { class: "primary" }, "選んだ版を取得");
  const resultBox = h("div", {});

  function renderChosen() {
    chosenList.replaceChildren(...[...chosen.values()].map((n) => h("li", {}, n)));
    syncBtn.disabled = chosen.size === 0 || !code;
  }

  async function pickAsset(asset) {
    versionsBox.replaceChildren(h("p", { class: "muted" }, "版を読み込み中…"));
    try {
      const { versions } = await api(`/api/resources/assets/${encodeURIComponent(asset.id)}/versions`);
      versionsBox.replaceChildren(h("table", {}, h("thead", {}, h("tr", {}, h("th", {}), h("th", {}, "版"), h("th", {}, "サイズ"), h("th", {}, "作成"), h("th", {}, "メモ"))),
        h("tbody", {}, ...versions.map((v) => {
          const cb = h("input", { type: "checkbox" });
          cb.checked = chosen.has(v.id);
          cb.addEventListener("change", () => {
            cb.checked ? chosen.set(v.id, `${asset.name} v${v.versionNum}`) : chosen.delete(v.id);
            renderChosen();
          });
          return h("tr", {}, h("td", {}, cb), h("td", {}, `v${v.versionNum}`),
            h("td", { class: "muted" }, `${Math.round((v.fileSize ?? 0) / 1024)} KB`),
            h("td", { class: "muted" }, fmtTime(v.createdAt)), h("td", {}, v.comment ?? ""));
        }))));
    } catch (err) {
      versionsBox.replaceChildren(errorBox(err));
    }
  }
  async function pickFolder(project, folder) {
    assetsBox.replaceChildren(h("p", { class: "muted" }, "アセットを読み込み中…"));
    try {
      const { assets } = await api(`/api/resources/projects/${encodeURIComponent(project.id)}/folders/${encodeURIComponent(folder.id)}/assets`);
      assetsBox.replaceChildren(list(assets, "アセット", pickAsset));
    } catch (err) {
      assetsBox.replaceChildren(errorBox(err));
    }
  }
  async function pickProject(project) {
    foldersBox.replaceChildren(h("p", { class: "muted" }, "フォルダを読み込み中…"));
    try {
      const { folders } = await api(`/api/resources/projects/${encodeURIComponent(project.id)}/folders`);
      foldersBox.replaceChildren(list(folders, "フォルダ", (f) => pickFolder(project, f)));
    } catch (err) {
      foldersBox.replaceChildren(errorBox(err));
    }
  }

  syncBtn.addEventListener("click", async () => {
    syncBtn.disabled = true;
    try {
      const r = await api("/api/resources/sync", { method: "POST", body: { code, subdir: subdir.value, versionIds: [...chosen.keys()] } });
      resultBox.replaceChildren(
        h("p", { class: "muted" }, `取得先: ${r.destination} / 転送 ${Math.round(r.transferBytes / 1024)} KB`),
        h("table", {}, h("tbody", {}, ...r.items.map((it) => {
          const [state, label] = RESULT[it.result] ?? ["off", it.result];
          return h("tr", {}, h("td", {}, it.name), h("td", {}, badge(state, label)), h("td", { class: "muted" }, it.detail ?? ""));
        }))));
      toast("取得しました");
    } catch (err) {
      resultBox.replaceChildren(errorBox(err));
    } finally {
      renderChosen();
    }
  });

  renderChosen();
  view.append(h("div", { class: "grid" },
    h("div", {},
      panel("Curare のプロジェクト", projectsBox),
      panel("フォルダ", foldersBox),
      panel("アセット", assetsBox)),
    h("div", {},
      panel("版", versionsBox),
      panel(code ? `取得 (${code} の repo へ)` : "取得 (上でプロジェクトを選んでください)",
        h("p", { class: "muted" }, "リソースは Curare、ソースは git / Revisor で版を管理します。取得物を git に入れるかはプロジェクトの方針に従ってください。"),
        chosenList, h("div", { class: "field" }, h("label", {}, "取得先 (repo 内の相対パス)"), subdir), syncBtn, resultBox))));

  try {
    const { projects } = await api("/api/resources/projects");
    projectsBox.replaceChildren(list(projects, "プロジェクト", pickProject));
  } catch (err) {
    projectsBox.replaceChildren(errorBox(err),
      h("p", { class: "muted" }, "接続先は ERGO_CURARE_URL (または Excubitor の CURARE_URL)、認証は ERGO_CURARE_TOKEN で渡します。"));
  }
}
