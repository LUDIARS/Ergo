// @implements SPEC-ergo-studio-connections
// 連携: Studio が使う各サービスの生死・接続先・認証・最終成功時刻と、サービス間のつながりの図。

import { api, badge, errorBox, fmtTime, h, panel } from "../lib.js";

const STATE_LABEL = { ok: "接続", degraded: "応答異常", down: "停止", not_configured: "未接続", unauthorized: "認証が必要" };
const STATE_COLOR = { ok: "#4cc38a", degraded: "#e8b04a", down: "#ef6461", not_configured: "#6b7080", unauthorized: "#d48ee0" };
const SVG = "http://www.w3.org/2000/svg";

function svg(tag, attrs, text) {
  const el = document.createElementNS(SVG, tag);
  for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, String(v));
  if (text !== undefined) el.textContent = text;
  return el;
}

/// Studio を中心に、サービスを円周に並べる。
function drawMap(nodes, edges) {
  const W = 760, H = 460, cx = W / 2, cy = H / 2, R = 170;
  const others = nodes.filter((n) => n.id !== "studio");
  const pos = new Map([["studio", { x: cx, y: cy }]]);
  others.forEach((n, i) => {
    const a = (i / others.length) * Math.PI * 2 - Math.PI / 2;
    pos.set(n.id, { x: cx + R * Math.cos(a) * 1.5, y: cy + R * Math.sin(a) });
  });
  const root = svg("svg", { class: "map", viewBox: `0 0 ${W} ${H}`, width: "100%" });
  root.append(svg("defs", {}));
  for (const e of edges) {
    const a = pos.get(e.from), b = pos.get(e.to);
    if (!a || !b) continue;
    // 往復する辺は少しずらす。
    const back = edges.some((x) => x.from === e.to && x.to === e.from);
    const off = back ? 6 : 0;
    const dx = b.x - a.x, dy = b.y - a.y, len = Math.hypot(dx, dy) || 1;
    const ox = (-dy / len) * off, oy = (dx / len) * off;
    root.append(svg("line", { class: `edge${e.active ? " active" : ""}`, x1: a.x + ox, y1: a.y + oy, x2: b.x + ox, y2: b.y + oy }));
    root.append(svg("text", { class: "edge-label", x: (a.x + b.x) / 2 + ox * 2, y: (a.y + b.y) / 2 + oy * 2, "text-anchor": "middle" }, e.label));
  }
  for (const n of nodes) {
    const p = pos.get(n.id);
    const g = svg("g", {});
    g.append(svg("circle", { cx: p.x, cy: p.y, r: n.id === "studio" ? 30 : 22, fill: "#1e2128", stroke: STATE_COLOR[n.state] ?? "#6b7080", "stroke-width": 3 }));
    g.append(svg("text", { x: p.x, y: p.y + (n.id === "studio" ? 46 : 38), "text-anchor": "middle" }, n.label));
    g.append(svg("title", {}, `${n.label}: ${STATE_LABEL[n.state] ?? n.state}${n.detail ? ` — ${n.detail}` : ""}`));
    root.append(g);
  }
  return root;
}

export async function render(view) {
  const mapBox = h("div", {}, h("p", { class: "muted" }, "各サービスを確認しています…"));
  const tableBox = h("div", {});
  const othersBox = h("div", {});
  const reload = h("button", {}, "確認し直す");
  async function load() {
    reload.disabled = true;
    try {
      const r = await api("/api/connections");
      mapBox.replaceChildren(
        h("p", { class: "muted" }, `接続先の解決: ${r.topology === "ok" ? "Excubitor の topology" : r.topology === "not_configured" ? "未設定 (EXCUBITOR_URL / LUDIARS_ROOT)" : "Excubitor に接続できません"} (${fmtTime(r.resolvedAt)})`),
        drawMap(r.nodes, r.edges));
      tableBox.replaceChildren(h("table", {},
        h("thead", {}, h("tr", {}, h("th", {}, "サービス"), h("th", {}, "役割"), h("th", {}, "状態"), h("th", {}, "接続先"), h("th", {}, "認証"), h("th", {}, "最終成功"))),
        h("tbody", {}, ...r.nodes.filter((n) => n.id !== "studio").map((n) => h("tr", {},
          h("td", {}, n.label), h("td", { class: "muted" }, n.role),
          h("td", {}, badge(n.state, STATE_LABEL[n.state] ?? n.state), n.detail ? h("div", { class: "muted" }, n.detail) : null),
          h("td", {}, n.url ? h("code", {}, n.url) : "—"),
          h("td", { class: "muted" }, n.auth ?? "—"),
          h("td", { class: "muted" }, fmtTime(n.lastOkAt)))))));
      othersBox.replaceChildren(r.others.length === 0 ? h("p", { class: "muted" }, "topology を取得できていません")
        : h("table", {}, h("tbody", {}, ...r.others.map((o) => h("tr", {}, h("td", {}, o.name), h("td", {}, h("code", {}, o.url)))))));
    } catch (err) {
      mapBox.replaceChildren(errorBox(err));
    } finally {
      reload.disabled = false;
    }
  }
  reload.addEventListener("click", load);
  view.append(
    panel("連携の図", reload, mapBox),
    panel("サービスごとの状態", tableBox),
    h("details", {}, h("summary", { class: "muted" }, "Excubitor の topology に載っている全サービス"), othersBox));
  await load();
}
