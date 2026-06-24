// Pictor Perf Monitor — PerfQueryAPI::export_json() のスナップショットを描画する。
//
// データ源:
//   1. WS /pictor_perf/ws … host が publish した最新スナップショットを受信 (ライブ)。
//   2. ファイルドロップ / 「JSON を開く」… CLI で吐いた perf JSON を offline 表示。
//
// JSON 契約は Pictor spec/subsystem/perf_introspection.md §5。

const $ = (id) => document.getElementById(id);

// ---- 表示ヘルパ ------------------------------------------------------------

function setStatus(text, cls) {
  const el = $("status");
  el.textContent = text;
  el.className = "status" + (cls ? " " + cls : "");
}

function fmtBytes(n) {
  if (n == null) return "—";
  if (n < 1024) return n + " B";
  if (n < 1024 * 1024) return (n / 1024).toFixed(1) + " KB";
  if (n < 1024 * 1024 * 1024) return (n / (1024 * 1024)).toFixed(2) + " MB";
  return (n / (1024 * 1024 * 1024)).toFixed(2) + " GB";
}
function fmtNum(n, d = 2) {
  return (n == null || Number.isNaN(n)) ? "—" : Number(n).toFixed(d);
}
function el(tag, attrs, children) {
  const e = document.createElement(tag);
  if (attrs) for (const [k, v] of Object.entries(attrs)) {
    if (k === "class") e.className = v;
    else if (k === "html") e.innerHTML = v;
    else e.setAttribute(k, v);
  }
  if (children) for (const c of [].concat(children)) {
    e.appendChild(typeof c === "string" ? document.createTextNode(c) : c);
  }
  return e;
}

// ---- 各セクション描画 ------------------------------------------------------

function renderInvariants(invariants) {
  const root = $("invariants");
  root.innerHTML = "";
  if (!invariants || !invariants.length) { root.appendChild(el("span", { class: "empty" }, "—")); return; }
  for (const inv of invariants) {
    const chip = el("span", { class: "chip " + (inv.ok ? "ok" : "bad") }, [
      el("span", { class: "dot" }),
      el("b", null, inv.name),
      el("span", null, `${inv.actual}B` + (inv.ok ? "" : ` ≠ ${inv.expected}B`)),
      el("span", { class: "note" }, inv.note || ""),
    ]);
    root.appendChild(chip);
  }
}

function renderMemory(memory) {
  const sum = $("mem_summary");
  sum.innerHTML = "";
  const fragCls = memory.fragmentationPct > 25 ? "util-bad" : "";
  sum.append(
    el("span", null, ["cache line ", el("b", null, String(memory.cacheLineSize) + "B")]),
    el("span", null, ["SoA used ", el("b", null, fmtBytes(memory.soaUsedBytes))]),
    el("span", null, ["reserved ", el("b", null, fmtBytes(memory.soaReservedBytes))]),
    el("span", null, ["allocator total ", el("b", null, fmtBytes(memory.poolAllocatorTotalBytes))]),
    el("span", { class: fragCls }, ["断片化 ", el("b", { class: fragCls }, fmtNum(memory.fragmentationPct, 1) + "%")]),
  );

  const pools = $("pools");
  pools.innerHTML = "";
  for (const p of memory.pools || []) {
    pools.appendChild(el("div", { class: "pool-title" },
      `${p.pool} — ${p.objectCount} obj · live ${fmtBytes(p.liveBytes)} / reserved ${fmtBytes(p.reservedBytes)}`));
    if (!p.streams || !p.streams.length || p.objectCount === 0) {
      pools.appendChild(el("div", { class: "empty" }, "オブジェクト無し"));
      continue;
    }
    const tbl = el("table");
    tbl.appendChild(el("tr", null, [
      el("th", null, "stream"), el("th", null, "group"), el("th", null, "elem"),
      el("th", null, "count"), el("th", null, "used"),
      el("th", null, "align"), el("th", null, "跨ぎ"), el("th", null, "line利用%"),
    ]));
    for (const s of p.streams) {
      const misaligned = !s.baseCacheAligned;
      const flag = misaligned || s.straddlesLine;
      const tr = el("tr", flag ? { class: "flag" } : null, [
        el("td", null, s.name),
        el("td", null, s.group),
        el("td", null, s.elementSize + "B"),
        el("td", null, String(s.elementCount)),
        el("td", null, fmtBytes(s.usedBytes)),
        el("td", null, misaligned ? el("span", { class: "badge warn" }, s.baseAlignment + "B") : el("span", { class: "badge yes" }, "64B")),
        el("td", null, s.straddlesLine ? el("span", { class: "badge warn" }, "跨ぐ") : el("span", { class: "badge no" }, "—")),
        el("td", { class: s.lineUtilizationPct < 90 ? "util-bad" : "" }, fmtNum(s.lineUtilizationPct, 1)),
      ]);
      tbl.appendChild(tr);
    }
    pools.appendChild(tbl);
  }
}

function renderTraffic(traffic) {
  const root = $("traffic");
  root.innerHTML = "";
  if (!traffic || !traffic.length) { root.appendChild(el("div", { class: "empty" }, "—")); return; }
  const maxG = Math.max(0.001, ...traffic.map((t) => t.gbps || 0));
  for (const t of traffic) {
    const pct = Math.min(100, ((t.gbps || 0) / maxG) * 100);
    root.appendChild(el("div", { class: "bar-row" }, [
      el("div", null, t.pass),
      el("div", { class: "bar-track" }, [el("div", { class: "bar-fill", style: `width:${pct}%` })]),
      el("div", { class: "bar-meta" },
        `${fmtNum(t.gbps, 2)} GB/s · ${fmtNum(t.cpuMs, 3)} ms · ${fmtBytes(t.streamedBytes)}`),
    ]));
  }
}

function renderBatches(batches) {
  const sum = $("batch_summary");
  sum.innerHTML = "";
  const effCls = batches.batchEfficiencyPct < 80 ? "util-bad" : "";
  sum.append(
    el("span", null, ["objects ", el("b", null, String(batches.totalObjects))]),
    el("span", null, ["batches ", el("b", null, String(batches.totalBatches))]),
    el("span", null, ["ideal ", el("b", null, String(batches.idealBatches))]),
    el("span", { class: effCls }, ["効率 ", el("b", { class: effCls }, fmtNum(batches.batchEfficiencyPct, 1) + "%")]),
  );

  const root = $("batches");
  root.innerHTML = "";
  if (!batches.groups || !batches.groups.length) { root.appendChild(el("div", { class: "empty" }, "オブジェクト無し")); return; }
  const tbl = el("table");
  tbl.appendChild(el("tr", null, [
    el("th", null, "pool"), el("th", null, "mesh"), el("th", null, "material"),
    el("th", null, "objects"), el("th", null, "actual"), el("th", null, "ideal"),
    el("th", null, "GPUバッチ可"), el("th", null, "理由"),
  ]));
  // gpuBatchable を上に、object数の多い順。
  const groups = [...batches.groups].sort((a, b) =>
    (b.gpuBatchable - a.gpuBatchable) || (b.objectCount - a.objectCount));
  for (const g of groups) {
    tbl.appendChild(el("tr", null, [
      el("td", null, g.pool),
      el("td", null, "#" + g.mesh),
      el("td", null, "#" + g.materialKey),
      el("td", null, String(g.objectCount)),
      el("td", null, String(g.actualBatches)),
      el("td", null, String(g.idealBatches)),
      el("td", null, g.gpuBatchable
        ? el("span", { class: "badge yes" }, "可")
        : el("span", { class: "badge no" }, "不可")),
      el("td", null, (g.reasons || []).filter((r) => r !== "Mergeable").join(", ") || "—"),
    ]));
  }
  root.appendChild(tbl);
}

function renderBatchTimings(timings) {
  const root = $("batch_timings");
  root.innerHTML = "";
  if (!timings || !timings.length) {
    root.appendChild(el("div", { class: "empty" },
      "per-batch 計測なし (host が BatchGpuTimer で draw を bracket すると表示)"));
    return;
  }
  const tbl = el("table");
  tbl.appendChild(el("tr", null, [
    el("th", null, "batch"), el("th", null, "pool"), el("th", null, "mesh"),
    el("th", null, "objects"), el("th", null, "GPU ms"), el("th", null, "per-obj µs"), el("th", null, "計測"),
  ]));
  for (const t of timings) {
    tbl.appendChild(el("tr", null, [
      el("td", null, "#" + t.batchIndex),
      el("td", null, t.pool),
      el("td", null, "#" + t.mesh),
      el("td", null, String(t.objectCount)),
      el("td", null, fmtNum(t.gpuMs, 4)),
      el("td", null, fmtNum(t.perObjectUs, 3)),
      el("td", null, t.measured
        ? el("span", { class: "badge yes" }, "実測")
        : el("span", { class: "badge no" }, "未計測")),
    ]));
  }
  root.appendChild(tbl);
}

function renderHw(hw) {
  const root = $("hw");
  root.innerHTML = "";
  if (!hw) { root.appendChild(el("div", { class: "empty" }, "—")); return; }
  if (!hw.available) {
    root.appendChild(el("div", { class: "mem-summary" }, [
      el("span", null, [el("span", { class: "badge no" }, "利用不可")]),
      el("span", null, ["source ", el("b", null, hw.source || "—")]),
      el("span", { class: "util-bad" }, hw.reason || "(理由なし)"),
    ]));
    return;
  }
  root.appendChild(el("div", { class: "mem-summary" }, [
    el("span", null, [el("span", { class: "badge yes" }, "実測")]),
    el("span", null, ["source ", el("b", null, hw.source)]),
    el("span", null, ["L2 hit ", el("b", null, fmtNum(hw.l2HitRatio * 100, 1) + "%")]),
    el("span", null, ["L3 hit ", el("b", null, fmtNum(hw.l3HitRatio * 100, 1) + "%")]),
    el("span", null, ["DRAM read ", el("b", null, fmtBytes(hw.bytesReadMC))]),
    el("span", null, ["DRAM write ", el("b", null, fmtBytes(hw.bytesWriteMC))]),
    el("span", null, ["IPC ", el("b", null, fmtNum(hw.ipc, 2))]),
  ]));
}

// ---- スナップショット適用 --------------------------------------------------

function applySnapshot(snap, label) {
  if (!snap || typeof snap !== "object" || !snap.memory) {
    setStatus("無効なスナップショット", "off");
    return;
  }
  renderInvariants(snap.memory.invariants);
  renderMemory(snap.memory);
  renderTraffic(snap.cacheTraffic);
  renderBatches(snap.batches || { groups: [], totalObjects: 0, totalBatches: 0, idealBatches: 0, batchEfficiencyPct: 0 });
  renderBatchTimings(snap.batchTimings);
  renderHw(snap.hwCounters);

  const objs = snap.batches ? snap.batches.totalObjects : 0;
  $("summary").textContent = `${objs} objects · ${(snap.batches || {}).totalBatches || 0} batches`;
  if (label) setStatus(label, "live");
}

function loadText(text, label) {
  let json;
  try { json = JSON.parse(text); }
  catch (e) { setStatus("JSON 解析失敗: " + e.message, "off"); return; }
  applySnapshot(json, label);
}

// ---- 入力: ファイル --------------------------------------------------------

$("file_btn").addEventListener("click", () => $("file_input").click());
$("file_input").addEventListener("change", (e) => {
  const f = e.target.files && e.target.files[0];
  if (!f) return;
  const r = new FileReader();
  r.onload = () => loadText(String(r.result), f.name);
  r.readAsText(f);
});
document.addEventListener("dragover", (e) => e.preventDefault());
document.addEventListener("drop", (e) => {
  e.preventDefault();
  const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
  if (!f) return;
  const r = new FileReader();
  r.onload = () => loadText(String(r.result), f.name);
  r.readAsText(f);
});

// ---- 入力: WS (ライブ) -----------------------------------------------------

function connectWS() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const ws = new WebSocket(proto + "://" + location.host + "/pictor_perf/ws");
  ws.addEventListener("open", () => setStatus("接続済 — スナップショット待ち", "live"));
  ws.addEventListener("close", () => {
    setStatus("切断 — 再接続中", "off");
    setTimeout(connectWS, 1500);
  });
  ws.addEventListener("message", (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (msg && msg.op === "snapshot" && msg.snapshot) {
      loadText(msg.snapshot, `ライブ (${msg.clients} client)`);
    }
  });
}

setStatus("未接続", "off");
connectWS();
