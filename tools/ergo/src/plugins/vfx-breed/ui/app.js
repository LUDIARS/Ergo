// vfx-breed UI: show a generation, let the human pick, breed the next one.

import { createPreview } from "/particle/preview.js";
import { fetchSeeds, fetchHealth, fetchParticleCurrent, requestGeneration } from "./api.js";
import { connectParticle, sendToParticle, downloadConfig, downloadKsEmitter } from "./exporters.js";
import { createHistory } from "./history.js";

const $ = (id) => document.getElementById(id);
const history = createHistory();
const selected = new Set();
let previews = [];
let seeds = [];
let busy = false;

// ── status / badges ─────────────────────────────────────────────────────────
function setStatus(msg, isErr = false) {
  $("status").textContent = msg;
  $("status").className = isErr ? "err" : "";
}
function setBadge(id, text, ok) {
  $(id).textContent = text;
  $(id).className = "badge " + (ok ? "ok" : "bad");
}

// ── seeds ───────────────────────────────────────────────────────────────────
const SOURCE_PARTICLE = "particle-current";
const SOURCE_JSON = "json";

async function loadSeeds() {
  try { seeds = await fetchSeeds(); } catch (e) { setStatus("起点の取得に失敗: " + e.message, true); }
  const sel = $("seed-source");
  sel.innerHTML = "";
  for (const s of seeds) sel.add(new Option(s.source === "catalog" ? `カタログ: ${s.name}` : s.name, s.id));
  sel.add(new Option("particle エディタの現在値", SOURCE_PARTICLE));
  sel.add(new Option("JSON を貼り付け", SOURCE_JSON));
  sel.onchange = () => { $("seed-json").hidden = sel.value !== SOURCE_JSON; };
}

async function resolveSeed() {
  const id = $("seed-source").value;
  if (id === SOURCE_PARTICLE) return fetchParticleCurrent();
  if (id === SOURCE_JSON) {
    const parsed = JSON.parse($("seed-json").value);
    return parsed?.config && typeof parsed.config === "object" ? parsed.config : parsed;
  }
  const s = seeds.find((x) => x.id === id);
  if (!s) throw new Error("起点が選ばれていません");
  return s.config;
}

// ── grid ────────────────────────────────────────────────────────────────────
function cellButton(label, title, fn) {
  const b = document.createElement("button");
  b.textContent = label;
  b.title = title;
  b.onclick = async (ev) => {
    ev.stopPropagation();
    try { const msg = await fn(); if (msg) setStatus(msg); } catch (e) { setStatus(e.message, true); }
  };
  return b;
}

function renderGrid() {
  for (const pv of previews) pv.dispose();
  previews = [];
  const grid = $("grid");
  grid.innerHTML = "";
  const entry = history.current;
  if (!entry) return;

  entry.configs.forEach((cfg, i) => {
    const cell = document.createElement("div");
    cell.className = "cell" + (selected.has(i) ? " sel" : "");
    const canvas = document.createElement("canvas");
    canvas.width = 220; canvas.height = 220;
    const name = document.createElement("div");
    name.className = "cell-name";
    name.textContent = cfg.name;
    const actions = document.createElement("div");
    actions.className = "cell-actions";
    actions.append(
      cellButton("▷ engine", "particle プラグインへ送りエンジン側でプレビュー",
        () => { sendToParticle(cfg); return `"${cfg.name}" を particle に送りました`; }),
      cellButton("↓ JSON", "ParticleEffectConfig JSON を書き出す",
        () => { downloadConfig(cfg); return `"${cfg.name}" を書き出しました`; }),
      cellButton("↗ PrivateGame", "GPU EmitterDescriptor JSON を書き出す (vfx-suggest の ks_export)",
        async () => `"${await downloadKsEmitter(cfg)}" を EmitterDescriptor として書き出しました`),
    );
    cell.append(canvas, name, actions);
    cell.onclick = () => {
      if (selected.has(i)) selected.delete(i); else selected.add(i);
      cell.classList.toggle("sel", selected.has(i));
      updateNextButton();
    };
    grid.appendChild(cell);
    previews.push(createPreview(canvas, cfg, { scale: 0.5 }));
  });
}

function renderHistory() {
  const box = $("history");
  box.innerHTML = "";
  history.entries.forEach((e, i) => {
    const b = document.createElement("button");
    b.textContent = e.generation === 0 ? "起点" : `第${e.generation}世代`;
    b.title = e.note;
    b.className = i === history.index ? "on" : "";
    b.onclick = () => { history.goTo(i); selected.clear(); render(); };
    box.appendChild(b);
  });
}

function updateNextButton() {
  $("next-btn").disabled = busy || selected.size === 0;
}

function render() {
  renderHistory();
  renderGrid();
  updateNextButton();
}

// ── breeding ────────────────────────────────────────────────────────────────
function readSeedField() {
  const v = $("seed").value.trim();
  return v === "" ? undefined : Number(v);
}

async function breedFrom(parents, generation) {
  busy = true;
  updateNextButton();
  setStatus("生成中…");
  try {
    const direction = $("direction").value.trim();
    const res = await requestGeneration({
      parents,
      direction,
      strength: Number($("strength").value),
      count: Number($("count").value),
      seed: readSeedField(),
      generation,
    });
    const d = res.direction;
    const note = `seed ${res.seed}` + (direction ? ` / 方向「${direction}」: ${d.status}` : "");
    history.push({ generation: res.generation, configs: res.children.map((c) => c.config), seed: res.seed, note });
    selected.clear();
    render();
    const dirMsg = !direction ? ""
      : d.status === "applied" ? ` · 方向を反映 (${Object.keys(d.weights ?? {}).join(", ") || "変化なし"})`
      : ` · 方向は反映されませんでした (${d.reason ?? d.status})`;
    setStatus(`第${res.generation}世代 · ${res.children.length} 体 · seed ${res.seed}${dirMsg}`);
  } catch (e) {
    setStatus("生成に失敗: " + e.message, true);
  } finally {
    busy = false;
    updateNextButton();
  }
}

async function start() {
  let seed;
  try { seed = await resolveSeed(); } catch (e) { setStatus("起点を読めません: " + e.message, true); return; }
  history.reset({ generation: 0, configs: [seed], seed: null, note: "起点" });
  selected.clear();
  render();
  await breedFrom([seed], 0);
}

function next() {
  const entry = history.current;
  if (!entry || selected.size === 0) return;
  const parents = [...selected].sort((a, b) => a - b).map((i) => entry.configs[i]);
  breedFrom(parents, entry.generation);
}

// ── init ────────────────────────────────────────────────────────────────────
$("start-btn").onclick = start;
$("next-btn").onclick = next;
$("strength").oninput = () => { $("strength-val").textContent = Number($("strength").value).toFixed(2); };

connectParticle((on) => setBadge("badge-particle", "particle: " + (on ? "on" : "off"), on));
fetchHealth()
  .then((h) => setBadge("badge-advisor",
    h.advisor.ok ? `方向指示: ${h.advisor.backend} ${h.advisor.model}` : `方向指示: 無効 (${h.advisor.reason ?? "?"})`,
    h.advisor.ok))
  .catch(() => setBadge("badge-advisor", "方向指示: 不明", false));
loadSeeds();
