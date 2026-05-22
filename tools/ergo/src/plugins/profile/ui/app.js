// Profile Timeline — Chrome Trace Event JSON を canvas でタイムライン可視化する。
//
// ergo_profile (C++) が dump した trace.json を読み、 スレッド別レーンに
// フレームグラフ (X event のネスト) + 瞬間マーカー (i) を、 下段にカウンタ
// トラック (C event) を描く。 ホイールでズーム、 ドラッグでパン。

const $ = (id) => document.getElementById(id);

const wrap    = $("canvas_wrap");
const canvas  = $("timeline");
const ctx     = canvas.getContext("2d");
const tooltip = $("tooltip");

// レイアウト定数 (CSS px)
const GUTTER  = 152;  // 左ガター (ラベル)
const RULER_H = 24;   // 時間ルーラ高
const TH_HEAD = 18;   // スレッドレーンの見出し高
const ROW_H   = 15;   // フレームグラフ 1 段の高さ
const CTR_H   = 48;   // カウンタトラック高
const PAD     = 8;

let model     = null;             // buildModel() の結果
let view      = { t0: 0, t1: 1 }; // 可視時間窓 (µs)
let drawnBars = [];               // ヒットテスト用 {x,y,w,h,ev,thName}
let dpr       = window.devicePixelRatio || 1;

// ---- 読み込み --------------------------------------------------------------

function setStatus(s) { $("status").textContent = s; }

function loadTraceText(text, label) {
  let json;
  try {
    json = JSON.parse(text);
  } catch (e) {
    setStatus("JSON 解析失敗: " + e.message);
    return;
  }
  const events = Array.isArray(json) ? json : json && json.traceEvents;
  if (!Array.isArray(events)) {
    setStatus("traceEvents が見つかりません");
    return;
  }
  const m = buildModel(events);
  if (!m) {
    setStatus("有効なイベントがありません");
    return;
  }
  model = m;
  fitView();
  $("fit_btn").disabled = false;
  setStatus(label + " — " + events.length + " events / " + model.threads.length + " threads");
  $("summary").textContent =
    "範囲 " + fmtDur(model.maxTs - model.minTs) +
    " · カウンタ " + model.counters.length;
  resize();
}

// ---- モデル構築 ------------------------------------------------------------

function getThread(map, key, e) {
  let th = map.get(key);
  if (!th) {
    th = { key, tid: e.tid, pid: e.pid, name: "tid " + e.tid,
           completes: [], instants: [], maxDepth: 0 };
    map.set(key, th);
  }
  return th;
}

function buildModel(events) {
  const threads  = new Map();
  const counters = new Map();
  let minTs = Infinity, maxTs = -Infinity;

  for (const e of events) {
    if (!e || typeof e.ph !== "string") continue;
    const key = e.pid + ":" + e.tid;

    if (e.ph === "M" && e.name === "thread_name") {
      const th = getThread(threads, key, e);
      if (e.args && e.args.name) th.name = String(e.args.name);
    } else if (e.ph === "X") {
      const th = getThread(threads, key, e);
      const ev = { name: e.name || "", ts: +e.ts || 0, dur: +e.dur || 0, depth: 0 };
      th.completes.push(ev);
      minTs = Math.min(minTs, ev.ts);
      maxTs = Math.max(maxTs, ev.ts + ev.dur);
    } else if (e.ph === "i") {
      const th = getThread(threads, key, e);
      const ts = +e.ts || 0;
      th.instants.push({ name: e.name || "", ts });
      minTs = Math.min(minTs, ts);
      maxTs = Math.max(maxTs, ts);
    } else if (e.ph === "C") {
      const args = e.args || {};
      const k = Object.keys(args)[0] || e.name || "counter";
      const v = +args[k] || 0, ts = +e.ts || 0;
      let c = counters.get(k);
      if (!c) { c = { name: k, samples: [], min: Infinity, max: -Infinity }; counters.set(k, c); }
      c.samples.push({ t: ts, v });
      c.min = Math.min(c.min, v);
      c.max = Math.max(c.max, v);
      minTs = Math.min(minTs, ts);
      maxTs = Math.max(maxTs, ts);
    }
  }
  if (!isFinite(minTs)) return null;

  for (const th of threads.values()) {
    // フレームグラフの段 (ネスト深さ) を割り当てる
    th.completes.sort((a, b) => a.ts - b.ts || b.dur - a.dur);
    const stack = [];
    for (const ev of th.completes) {
      while (stack.length) {
        const top = stack[stack.length - 1];
        if (top.ts + top.dur <= ev.ts) stack.pop();
        else break;
      }
      ev.depth = stack.length;
      stack.push(ev);
    }
    th.maxDepth = th.completes.reduce((m, e) => Math.max(m, e.depth), 0);
    th.instants.sort((a, b) => a.ts - b.ts);
  }
  for (const c of counters.values()) c.samples.sort((a, b) => a.t - b.t);

  // スレッドは tid 順で安定させる
  const thArr = [...threads.values()].sort((a, b) => a.tid - b.tid);
  return { threads: thArr, counters: [...counters.values()], minTs, maxTs };
}

// ---- レイアウト ------------------------------------------------------------

function layout() {
  let y = RULER_H;
  model.threadLayout = [];
  for (const th of model.threads) {
    const rows = th.maxDepth + 1;
    const h = TH_HEAD + rows * ROW_H;
    model.threadLayout.push({ th, top: y, rows, h });
    y += h + PAD;
  }
  model.counterLayout = [];
  for (const c of model.counters) {
    model.counterLayout.push({ c, top: y, h: CTR_H });
    y += CTR_H + PAD;
  }
  model.contentH = y + PAD;
}

// ---- ビュー変換 ------------------------------------------------------------

function plotW() { return canvas.clientWidth - GUTTER; }
function xOf(t)  { return GUTTER + (t - view.t0) / (view.t1 - view.t0) * plotW(); }
function tOf(x)  { return view.t0 + (x - GUTTER) / plotW() * (view.t1 - view.t0); }

function fitView() {
  const span = Math.max(1, model.maxTs - model.minTs);
  view = { t0: model.minTs - span * 0.02, t1: model.maxTs + span * 0.02 };
}

// ---- 描画 ------------------------------------------------------------------

function fmtDur(us) {
  if (us < 1000) return us.toFixed(0) + "µs";
  if (us < 1e6)  return (us / 1000).toFixed(2) + "ms";
  return (us / 1e6).toFixed(3) + "s";
}

function colorFor(name) {
  let h = 0;
  for (let i = 0; i < name.length; i++) h = (h * 31 + name.charCodeAt(i)) & 0xffff;
  return "hsl(" + (h % 360) + ",46%,52%)";
}

function render() {
  if (!model) return;
  const W = canvas.clientWidth, H = canvas.clientHeight;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#15171c";
  ctx.fillRect(0, 0, W, H);
  drawnBars = [];

  drawRuler(W);

  ctx.textBaseline = "middle";
  ctx.font = "11px 'Segoe UI', sans-serif";

  for (const L of model.threadLayout) drawThread(L, W);
  for (const L of model.counterLayout) drawCounter(L, W);

  // ガター境界線
  ctx.strokeStyle = "#343845";
  ctx.beginPath();
  ctx.moveTo(GUTTER + 0.5, 0);
  ctx.lineTo(GUTTER + 0.5, H);
  ctx.stroke();
}

function drawRuler(W) {
  ctx.fillStyle = "#23262f";
  ctx.fillRect(0, 0, W, RULER_H);
  ctx.fillStyle = "#8b8fa0";
  ctx.font = "10px 'Segoe UI', sans-serif";
  ctx.textBaseline = "middle";
  const span = view.t1 - view.t0;
  const ticks = 8;
  for (let i = 0; i <= ticks; i++) {
    const t = view.t0 + span * (i / ticks);
    const x = xOf(t);
    if (x < GUTTER) continue;
    ctx.strokeStyle = "#2b2f3a";
    ctx.beginPath();
    ctx.moveTo(x + 0.5, RULER_H);
    ctx.lineTo(x + 0.5, canvas.clientHeight);
    ctx.stroke();
    ctx.fillText(fmtDur(t - model.minTs), x + 3, RULER_H / 2);
  }
}

function drawThread(L, W) {
  const th = L.th;
  // 見出し
  ctx.fillStyle = "#9aa0b0";
  ctx.font = "11px 'Segoe UI', sans-serif";
  ctx.fillText(th.name, 8, L.top + TH_HEAD / 2);
  ctx.fillStyle = "#6b7080";
  ctx.font = "10px 'Segoe UI', sans-serif";
  ctx.fillText(th.completes.length + " scopes", 8, L.top + TH_HEAD / 2 + 0);

  const rowsTop = L.top + TH_HEAD;
  // フレームグラフ
  for (const ev of th.completes) {
    const x = xOf(ev.ts);
    const w = Math.max(1, xOf(ev.ts + ev.dur) - x);
    if (x + w < GUTTER || x > W) continue;
    const cx = Math.max(GUTTER, x);
    const cw = Math.min(W, x + w) - cx;
    if (cw <= 0) continue;
    const y = rowsTop + ev.depth * ROW_H;
    ctx.fillStyle = colorFor(ev.name);
    ctx.fillRect(cx, y + 1, cw, ROW_H - 2);
    drawnBars.push({ x: cx, y: y + 1, w: cw, h: ROW_H - 2, ev, thName: th.name });
    if (cw > 26) {
      ctx.save();
      ctx.beginPath();
      ctx.rect(cx + 2, y, cw - 4, ROW_H);
      ctx.clip();
      ctx.fillStyle = "#0d0e12";
      ctx.font = "10px 'Segoe UI', sans-serif";
      ctx.fillText(ev.name, cx + 4, y + ROW_H / 2);
      ctx.restore();
    }
  }
  // 瞬間マーカー
  ctx.strokeStyle = "#e8c14a";
  for (const m of th.instants) {
    const x = xOf(m.ts);
    if (x < GUTTER || x > W) continue;
    ctx.beginPath();
    ctx.moveTo(x + 0.5, rowsTop);
    ctx.lineTo(x + 0.5, L.top + L.h);
    ctx.stroke();
  }
}

function drawCounter(L, W) {
  const c = L.c;
  ctx.fillStyle = "#222633";
  ctx.fillRect(GUTTER, L.top, W - GUTTER, L.h);
  ctx.fillStyle = "#9aa0b0";
  ctx.font = "11px 'Segoe UI', sans-serif";
  ctx.fillText(c.name, 8, L.top + 12);
  ctx.fillStyle = "#6b7080";
  ctx.font = "10px 'Segoe UI', sans-serif";
  ctx.fillText(c.min.toFixed(0) + " – " + c.max.toFixed(0), 8, L.top + 28);

  const range = Math.max(1e-9, c.max - c.min);
  const yOf = (v) => L.top + L.h - 4 - (v - c.min) / range * (L.h - 12);
  ctx.strokeStyle = "#5b9bd5";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  let started = false;
  for (const s of c.samples) {
    const x = Math.max(GUTTER, Math.min(W, xOf(s.t)));
    const y = yOf(s.v);
    if (!started) { ctx.moveTo(x, y); started = true; }
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
  ctx.lineWidth = 1;
}

// ---- ヒットテスト / ツールチップ -------------------------------------------

function onMove(e) {
  if (!model) return;
  const r = canvas.getBoundingClientRect();
  const mx = e.clientX - r.left, my = e.clientY - r.top;
  let hit = null;
  for (const b of drawnBars) {
    if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) { hit = b; break; }
  }
  if (!hit) { tooltip.hidden = true; return; }
  tooltip.hidden = false;
  tooltip.innerHTML =
    "<b>" + escapeHtml(hit.ev.name) + "</b><br>" +
    '<span class="dim">' + escapeHtml(hit.thName) + " · 深さ " + hit.ev.depth + "</span><br>" +
    "所要 " + fmtDur(hit.ev.dur) + ' <span class="dim">@ ' +
    fmtDur(hit.ev.ts - model.minTs) + "</span>";
  const tx = Math.min(e.clientX - r.left + 14, wrap.clientWidth - 320);
  tooltip.style.left = Math.max(4, tx) + "px";
  tooltip.style.top  = (e.clientY - r.top + 14) + "px";
}

function escapeHtml(s) {
  return String(s).replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]));
}

// ---- ズーム / パン ---------------------------------------------------------

function onWheel(e) {
  if (!model) return;
  e.preventDefault();
  const r = canvas.getBoundingClientRect();
  const mx = e.clientX - r.left;
  if (mx < GUTTER) return;
  const tAt = tOf(mx);
  const factor = e.deltaY < 0 ? 0.8 : 1.25;
  view.t0 = tAt - (tAt - view.t0) * factor;
  view.t1 = tAt + (view.t1 - tAt) * factor;
  render();
}

let dragX = null;
function onDown(e) { if (e.clientX - canvas.getBoundingClientRect().left >= GUTTER) dragX = e.clientX; }
function onUp()     { dragX = null; }
function onDrag(e) {
  if (dragX === null || !model) return;
  const dx = e.clientX - dragX;
  dragX = e.clientX;
  const dt = dx / plotW() * (view.t1 - view.t0);
  view.t0 -= dt;
  view.t1 -= dt;
  render();
}

// ---- リサイズ --------------------------------------------------------------

function resize() {
  if (!model) {
    canvas.style.width = wrap.clientWidth + "px";
    canvas.style.height = wrap.clientHeight + "px";
    canvas.width = wrap.clientWidth * dpr;
    canvas.height = wrap.clientHeight * dpr;
    return;
  }
  layout();
  const W = wrap.clientWidth;
  const H = Math.max(wrap.clientHeight, model.contentH);
  canvas.style.width = W + "px";
  canvas.style.height = H + "px";
  canvas.width  = Math.floor(W * dpr);
  canvas.height = Math.floor(H * dpr);
  render();
}

// ---- 入力配線 --------------------------------------------------------------

function pickFile(file) {
  const reader = new FileReader();
  reader.onload = () => loadTraceText(String(reader.result), file.name);
  reader.readAsText(file);
}

const dz = $("dropzone");
dz.addEventListener("dragover", (e) => { e.preventDefault(); dz.classList.add("drag"); });
dz.addEventListener("dragleave", () => dz.classList.remove("drag"));
dz.addEventListener("drop", (e) => {
  e.preventDefault();
  dz.classList.remove("drag");
  if (e.dataTransfer.files[0]) pickFile(e.dataTransfer.files[0]);
});
$("file_btn").addEventListener("click", () => $("file_input").click());
$("file_input").addEventListener("change", (e) => {
  if (e.target.files[0]) pickFile(e.target.files[0]);
});
$("fit_btn").addEventListener("click", () => { if (model) { fitView(); render(); } });

canvas.addEventListener("mousemove", (e) => { onMove(e); onDrag(e); });
canvas.addEventListener("mouseleave", () => { tooltip.hidden = true; });
canvas.addEventListener("wheel", onWheel, { passive: false });
canvas.addEventListener("mousedown", onDown);
window.addEventListener("mouseup", onUp);
window.addEventListener("resize", () => { dpr = window.devicePixelRatio || 1; resize(); });

// ---- ライブ受信 (WS) -------------------------------------------------------
// engine 側 (将来) から publish された trace を受け取る。 file ドロップが主経路。
try {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const ws = new WebSocket(proto + "://" + location.host + "/profile/ws");
  ws.addEventListener("message", (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (msg && msg.op === "trace" && typeof msg.trace === "string") {
      loadTraceText(msg.trace, "live");
    }
  });
} catch { /* WS 不可でも file ドロップは使える */ }

resize();
