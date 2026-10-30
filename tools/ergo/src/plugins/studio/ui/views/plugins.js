// @implements SPEC-ergo-studio-plugins
// プラグイン: 許可した取得元から plugin pack を取得する (プレビュー → 承認 → 導入)。
// 取得したプラグインは Ergo の中で動くので、承認前に中身 (コミット・プラグイン名・依存) を見せる。
// 読み込みは次回起動時。

import { api, badge, errorBox, fmtTime, h, panel, toast } from "../lib.js";

export async function render(view) {
  const ledgerBox = h("div", {}, h("p", { class: "muted" }, "読み込み中…"));
  const source = h("input", { type: "text", placeholder: "owner/repo:subpath[@rev]  例: LUDIARS/AdventureCube:tools/ac-web/plugins@main" });
  const previewBtn = h("button", { class: "primary" }, "中身を確認");
  const previewBox = h("div", {});
  const ownersLine = h("p", { class: "muted" });

  async function loadLedger() {
    try {
      const { packs, owners, home } = await api("/api/plugins");
      ownersLine.textContent = `許可された取得元: ${owners.join(", ")} (ERGO_PLUGIN_OWNERS) / 置き場: ${home}`;
      if (packs.length === 0) {
        ledgerBox.replaceChildren(h("p", { class: "muted" }, "取得したプラグインはありません"));
        return;
      }
      ledgerBox.replaceChildren(h("table", {},
        h("thead", {}, h("tr", {}, h("th", {}, "パック"), h("th", {}, "リビジョン"), h("th", {}, "プラグイン"), h("th", {}, "状態"), h("th", {}))),
        h("tbody", {}, ...packs.map((p) => {
          const toggle = h("button", {}, p.enabled ? "無効にする" : "有効にする");
          toggle.addEventListener("click", async () => {
            try {
              await api("/api/plugins/enable", { method: "POST", body: { id: p.id, enabled: !p.enabled } });
              toast("変更しました。Ergo の再起動で反映されます");
              await loadLedger();
            } catch (err) { toast(err.message, true); }
          });
          const remove = h("button", {}, "削除");
          remove.addEventListener("click", async () => {
            if (!confirm(`${p.id} を削除しますか (取得物も消します)`)) return;
            try {
              await api(`/api/plugins?id=${encodeURIComponent(p.id)}`, { method: "DELETE" });
              toast("削除しました。Ergo の再起動で反映されます");
              await loadLedger();
            } catch (err) { toast(err.message, true); }
          });
          return h("tr", {},
            h("td", {}, p.id, h("div", { class: "muted" }, fmtTime(p.installedAt))),
            h("td", {}, h("code", {}, p.rev.slice(0, 12))),
            h("td", {}, p.plugins.join(", ")),
            h("td", {}, badge(p.enabled ? "ok" : "off", p.enabled ? "有効" : "無効")),
            h("td", {}, h("div", { class: "row" }, toggle, remove)));
        }))));
    } catch (err) {
      ledgerBox.replaceChildren(errorBox(err));
    }
  }

  previewBtn.addEventListener("click", async () => {
    previewBtn.disabled = true;
    previewBox.replaceChildren(h("p", { class: "muted" }, "取得して中身を確認しています…"));
    try {
      const { preview } = await api("/api/plugins/preview", { method: "POST", body: { source: source.value.trim() } });
      const approve = h("button", { class: "primary" }, "承認して導入");
      approve.addEventListener("click", async () => {
        approve.disabled = true;
        try {
          await api("/api/plugins/install", { method: "POST", body: { token: preview.token } });
          toast("導入しました。Ergo の再起動で読み込まれます");
          previewBox.replaceChildren();
          await loadLedger();
        } catch (err) {
          toast(err.message, true);
          approve.disabled = false;
        }
      });
      previewBox.replaceChildren(
        h("table", {}, h("tbody", {},
          h("tr", {}, h("th", {}, "取得元"), h("td", {}, `${preview.owner}/${preview.repo}:${preview.subpath}`)),
          h("tr", {}, h("th", {}, "リビジョン"), h("td", {}, h("code", {}, preview.rev), ` (指定: ${preview.requested})`)),
          h("tr", {}, h("th", {}, "コミット"), h("td", {}, `${preview.commit.subject} (${fmtTime(preview.commit.date)})`)),
          h("tr", {}, h("th", {}, "プラグイン"), h("td", {}, preview.plugins.join(", "))),
          h("tr", {}, h("th", {}, "依存"), h("td", {}, preview.hasDependencies ? "package.json あり (npm install --ignore-scripts で導入)" : "なし")),
          preview.replaces ? h("tr", {}, h("th", {}, "置き換え"), h("td", {}, `導入済みの ${preview.replaces.slice(0, 12)} を置き換えます`)) : null,
          h("tr", {}, h("th", {}, "期限"), h("td", {}, `${fmtTime(preview.expiresAt)} までに承認`)))),
        h("p", { class: "muted" }, "プラグインは Ergo の中でそのまま動きます。取得元とコミットを確かめてから承認してください。"),
        approve);
    } catch (err) {
      previewBox.replaceChildren(errorBox(err));
    } finally {
      previewBtn.disabled = false;
    }
  });

  view.append(
    panel("プラグインを取得", ownersLine, h("div", { class: "row" }, source, previewBtn), previewBox),
    panel("取得済みのプラグイン", ledgerBox));
  await loadLedger();
}
