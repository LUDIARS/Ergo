// log browser UI — live ergo_log lines per app, with level / text / frame
// filters, auto-scroll and pause.
//
// The server sends `apps` + one `backlog` per app on connect, then live
// `lines` batches. This page mirrors each app's ring (same 10k bound) and
// filters locally; only the newest MAX_ROWS matching lines are in the DOM.

const RING_CAPACITY = 10000;   // mirrors the server ring (ring.ts)
const MAX_ROWS      = 2000;    // DOM rows kept for the selected app
const LEVELS        = ["ERR", "WRN", "INF", "DBG"];

let ws = null;

// state
const apps    = new Map();   // name -> AppInfo (from `apps`)
const lines   = new Map();   // name -> StoredLine[] (oldest first)
const dropped = new Map();   // name -> lines the engine reported dropped
const tids    = new Map();   // name -> Map(tid -> short label)
let selected  = null;
let paused    = false;
let pendingWhilePaused = 0;

// ── DOM refs ────────────────────────────────────────────────────────────────
const statusEl   = document.getElementById("status");
const appSelect  = document.getElementById("app-select");
const appStateEl = document.getElementById("app-state");
const textFilter = document.getElementById("text-filter");
const frameMinEl = document.getElementById("frame-min");
const frameMaxEl = document.getElementById("frame-max");
const autoScroll = document.getElementById("auto-scroll");
const pauseBtn   = document.getElementById("pause-btn");
const summaryEl  = document.getElementById("summary");
const scrollEl   = document.getElementById("log-scroll");
const bodyEl     = document.getElementById("log-body");
const emptyEl    = document.getElementById("empty");
const levelBoxes = [...document.querySelectorAll("#levels input[type=checkbox]")];
const countEls   = new Map([...document.querySelectorAll("#levels .count")].map((el) => [el.dataset.lvl, el]));

// ── WebSocket ───────────────────────────────────────────────────────────────
function connect() {
    if (ws) { try { ws.close(); } catch (_) {} }
    ws = new WebSocket(`ws://${location.host}/log/ws`);

    ws.onopen = () => {
        setStatus("connected");
        // A fresh snapshot follows: drop what the previous session held.
        apps.clear(); lines.clear(); dropped.clear(); tids.clear();
        ws.send(JSON.stringify({ op: "hello", role: "ui" }));
    };

    ws.onmessage = (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (!msg || !msg.op) return;

        switch (msg.op) {
            case "apps":    onApps(msg.apps); break;
            case "backlog": onBacklog(msg); break;
            case "lines":   onLines(msg); break;
        }
    };

    ws.onclose = () => {
        setStatus("disconnected");
        setTimeout(connect, 3000);
    };
    ws.onerror = () => { ws.close(); };
}

function setStatus(s) {
    statusEl.textContent = s === "connected" ? "● connected" : "○ disconnected";
    statusEl.className = s;
}

// ── Messages ────────────────────────────────────────────────────────────────
function onApps(list) {
    apps.clear();
    for (const a of list) apps.set(a.app, a);
    // Forget apps the server evicted.
    for (const name of [...lines.keys()]) {
        if (!apps.has(name)) { lines.delete(name); dropped.delete(name); tids.delete(name); }
    }
    if (!apps.has(selected)) {
        const live = list.find((a) => a.connected);
        selected = live ? live.app : (list[0]?.app ?? null);
        renderAll();
    }
    renderAppPicker();
}

function onBacklog(msg) {
    lines.set(msg.app, msg.lines.slice(-RING_CAPACITY));
    dropped.set(msg.app, msg.dropped);
    if (msg.app === selected) renderAll();
}

function onLines(msg) {
    const arr = lines.get(msg.app) ?? [];
    lines.set(msg.app, arr);
    arr.push(...msg.lines);
    if (arr.length > RING_CAPACITY) arr.splice(0, arr.length - RING_CAPACITY);
    dropped.set(msg.app, (dropped.get(msg.app) ?? 0) + msg.dropped);

    if (msg.app !== selected) return;
    if (paused) {
        pendingWhilePaused += msg.lines.length;
        renderSummary();
        return;
    }
    appendRows(msg.lines);
}

// ── Filters ─────────────────────────────────────────────────────────────────
function currentFilter() {
    const levels = new Set(levelBoxes.filter((b) => b.checked).map((b) => b.value));
    const q      = textFilter.value.trim().toLowerCase();
    const min    = frameMinEl.value === "" ? null : Number(frameMinEl.value);
    const max    = frameMaxEl.value === "" ? null : Number(frameMaxEl.value);
    return { levels, q, min, max };
}

function matches(line, f) {
    if (!f.levels.has(line.lvl)) return false;
    if (f.min !== null && line.frame < f.min) return false;
    if (f.max !== null && line.frame > f.max) return false;
    if (f.q && !line.msg.toLowerCase().includes(f.q)) return false;
    return true;
}

// ── Rendering ───────────────────────────────────────────────────────────────
function tidLabel(app, tid) {
    let m = tids.get(app);
    if (!m) { m = new Map(); tids.set(app, m); }
    let label = m.get(tid);
    if (!label) { label = `T${m.size + 1}`; m.set(tid, label); }
    return label;
}

function makeRow(line) {
    const tr = document.createElement("tr");
    tr.className = `row-${line.lvl}`;

    const frame = document.createElement("td");
    frame.className = "c-frame";
    frame.textContent = String(line.frame);
    frame.title = "Show only this frame (click again to clear)";
    frame.dataset.frame = String(line.frame);

    const time = document.createElement("td");
    time.className = "c-time";
    time.textContent = (line.ts_us / 1e6).toFixed(3);

    const lvl = document.createElement("td");
    lvl.className = `c-lvl lvl-${line.lvl}`;
    lvl.textContent = line.lvl;

    const tid = document.createElement("td");
    tid.className = "c-tid";
    tid.textContent = tidLabel(selected, line.tid);
    tid.title = `tid ${line.tid}`;

    const msg = document.createElement("td");
    msg.className = "c-msg";
    msg.textContent = line.msg;

    tr.append(frame, time, lvl, tid, msg);
    return tr;
}

function scrollToEnd() {
    if (autoScroll.checked) scrollEl.scrollTop = scrollEl.scrollHeight;
}

/// Rebuild the table for the selected app (filter change, app switch, resume).
function renderAll() {
    pendingWhilePaused = 0;
    const all = (selected !== null && lines.get(selected)) || [];
    const f = currentFilter();
    const shown = [];
    for (let i = all.length - 1; i >= 0 && shown.length < MAX_ROWS; --i) {
        if (matches(all[i], f)) shown.push(all[i]);
    }
    shown.reverse();

    const frag = document.createDocumentFragment();
    for (const line of shown) frag.appendChild(makeRow(line));
    bodyEl.replaceChildren(frag);
    emptyEl.classList.toggle("hidden", all.length > 0);
    renderCounts();
    renderSummary();
    scrollToEnd();
}

/// Append live lines of the selected app without rebuilding the table.
function appendRows(newLines) {
    const f = currentFilter();
    const frag = document.createDocumentFragment();
    for (const line of newLines) {
        if (matches(line, f)) frag.appendChild(makeRow(line));
    }
    bodyEl.appendChild(frag);
    while (bodyEl.childElementCount > MAX_ROWS) bodyEl.firstElementChild.remove();
    emptyEl.classList.toggle("hidden", (lines.get(selected)?.length ?? 0) > 0);
    renderCounts();
    renderSummary();
    scrollToEnd();
}

function renderCounts() {
    const counts = { ERR: 0, WRN: 0, INF: 0, DBG: 0 };
    for (const line of lines.get(selected) ?? []) counts[line.lvl] += 1;
    for (const lvl of LEVELS) countEls.get(lvl).textContent = String(counts[lvl]);
}

function renderSummary() {
    const total = lines.get(selected)?.length ?? 0;
    const parts = [`${bodyEl.childElementCount} shown / ${total} held`];
    const d = dropped.get(selected) ?? 0;
    if (d > 0) parts.push(`${d} dropped by the engine`);
    if (paused) parts.push(`paused (${pendingWhilePaused} new)`);
    summaryEl.textContent = parts.join(" · ");
}

function renderAppPicker() {
    appSelect.replaceChildren();
    if (apps.size === 0) {
        const opt = document.createElement("option");
        opt.textContent = "(no engines yet)";
        opt.disabled = true;
        appSelect.appendChild(opt);
    }
    for (const a of apps.values()) {
        const opt = document.createElement("option");
        opt.value = a.app;
        opt.textContent = a.connected ? a.app : `${a.app} (disconnected)`;
        opt.selected = a.app === selected;
        appSelect.appendChild(opt);
    }
    const cur = apps.get(selected);
    appStateEl.textContent = cur ? (cur.connected ? "live" : "disconnected") : "";
    appStateEl.className = cur ? `badge ${cur.connected ? "live" : "gone"}` : "badge";
}

// ── Controls ────────────────────────────────────────────────────────────────
appSelect.addEventListener("change", () => {
    selected = appSelect.value;
    renderAppPicker();
    renderAll();
});

for (const box of levelBoxes) box.addEventListener("change", renderAll);
for (const el of [textFilter, frameMinEl, frameMaxEl]) el.addEventListener("input", renderAll);

pauseBtn.addEventListener("click", () => {
    paused = !paused;
    pauseBtn.textContent = paused ? "Resume" : "Pause";
    pauseBtn.classList.toggle("active", paused);
    if (paused) renderSummary();
    else renderAll();
});

autoScroll.addEventListener("change", scrollToEnd);

// Click a frame number: show only that frame; click it again to clear.
bodyEl.addEventListener("click", (ev) => {
    const cell = ev.target.closest("td.c-frame");
    if (!cell) return;
    const frame = cell.dataset.frame;
    const same = frameMinEl.value === frame && frameMaxEl.value === frame;
    frameMinEl.value = same ? "" : frame;
    frameMaxEl.value = same ? "" : frame;
    renderAll();
});

renderAppPicker();
renderAll();
connect();
