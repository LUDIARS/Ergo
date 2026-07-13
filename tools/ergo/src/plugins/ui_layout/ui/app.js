"use strict";

const $ = (id) => document.getElementById(id);
const treeEl = $("tree"), propBody = $("propBody"), canvas = $("canvas"), ctx = canvas.getContext("2d");
const statusEl = $("status"), filePathEl = $("filePath");

let ws = null;
let selectedId = "root";
let drag = null;
let doc = {
  schema_version: 1,
  name: "layout",
  design_size: { w: 1280, h: 720 },
  root: {
    id: "root", type: "container", layout: "absolute",
    rect: { x: 0, y: 0, w: 1280, h: 720 },
    anchor: { h: "left", v: "top" },
    stretch: { left: null, right: null, top: null, bottom: null },
    binds: [], children: []
  }
};

// vector ノードの参照 SVG を <img> でロードしてキャンバスにサムネ描画する。
// src ごとに Image を 1 つだけ作り、ロード完了で再描画する。失敗時は呼び出し側
// が単色プレースホルダにフォールバックする。
const svgCache = new Map();
function svgImage(src) {
  if (!src) return null;
  let img = svgCache.get(src);
  if (img) return img;
  img = new Image();
  img.dataset.failed = "";
  img.onload = () => renderCanvas();
  img.onerror = () => { img.dataset.failed = "1"; };
  img.src = `/ui_layout/api/file/svg?path=${encodeURIComponent(src)}`;
  svgCache.set(src, img);
  return img;
}

function setStatus(text, cls) { statusEl.textContent = text; statusEl.className = cls || ""; }
function connectWs() {
  ws = new WebSocket(`ws://${location.host}/ui_layout/ws`);
  ws.onopen = () => setStatus("ws connected", "ok");
  ws.onclose = () => { setStatus("ws disconnected", "bad"); setTimeout(connectWs, 1200); };
  ws.onmessage = (ev) => {
    try {
      const m = JSON.parse(ev.data);
      if (m.op === "patch" && typeof m.payload === "string") applyPatchString(m.payload, true);
      if (m.op === "document" && typeof m.payload === "string") loadDocString(m.payload, true);
    } catch {}
  };
}
connectWs();

function eachNode(node, parent, fn) {
  fn(node, parent);
  (node.children || []).forEach(ch => eachNode(ch, node, fn));
}
function findNode(id) {
  let out = null, parent = null;
  eachNode(doc.root, null, (n, p) => { if (n.id === id) { out = n; parent = p; } });
  return { node: out, parent };
}
function genId(prefix) {
  return `${prefix}_${Math.random().toString(36).slice(2, 7)}`;
}
function defaultNode(type) {
  return {
    id: genId(type), type, layout: "absolute",
    rect: { x: 80, y: 80, w: 200, h: 80 },
    anchor: { h: "left", v: "top" },
    stretch: { left: null, right: null, top: null, bottom: null },
    binds: [], children: [],
    text: { value: type, size: 18, color: "#ffffff" },
    image: { src: "", fit: "stretch" },
    vector: { src: "", fit: "stretch", extrude: 0 },
    nine_slice: { src: "", left: 8, right: 8, top: 8, bottom: 8 }
  };
}

function renderTree() {
  treeEl.innerHTML = "";
  function mk(n, depth) {
    const d = document.createElement("div");
    d.className = "node" + (selectedId === n.id ? " sel" : "");
    d.style.paddingLeft = (8 + depth * 14) + "px";
    d.textContent = `${n.id} (${n.type})`;
    d.onclick = () => { selectedId = n.id; renderAll(); };
    treeEl.appendChild(d);
    (n.children || []).forEach(ch => mk(ch, depth + 1));
  }
  mk(doc.root, 0);
}

function drawNode(n, parentAbs) {
  const r = n.rect || { x:0,y:0,w:0,h:0 };
  const abs = { x: parentAbs.x + r.x, y: parentAbs.y + r.y, w: r.w, h: r.h };
  if (n.type === "container") { ctx.strokeStyle = "#2f4b66"; ctx.strokeRect(abs.x, abs.y, abs.w, abs.h); }
  if (n.type === "rect") { ctx.fillStyle = "#2664a3"; ctx.fillRect(abs.x, abs.y, abs.w, abs.h); }
  if (n.type === "text") { ctx.fillStyle = "#111"; ctx.fillRect(abs.x, abs.y, abs.w, abs.h); ctx.fillStyle = "#fff"; ctx.font = `${(n.text?.size||16)}px sans-serif`; ctx.fillText(n.text?.value||n.id, abs.x+6, abs.y+22); }
  if (n.type === "image") { ctx.fillStyle = "#3f515f"; ctx.fillRect(abs.x, abs.y, abs.w, abs.h); }
  if (n.type === "nine_slice") { ctx.fillStyle = "#485f3f"; ctx.fillRect(abs.x, abs.y, abs.w, abs.h); }
  if (n.type === "vector") {
    // 参照 SVG のサムネを描く。未ロード/失敗時は従来の単色プレースホルダ。
    const img = svgImage(n.vector?.src);
    let drawn = false;
    if (img && img.complete && !img.dataset.failed && img.naturalWidth > 0) {
      try { ctx.drawImage(img, abs.x, abs.y, abs.w, abs.h); drawn = true; } catch { drawn = false; }
    }
    if (!drawn) { ctx.fillStyle = "#5a405d"; ctx.fillRect(abs.x, abs.y, abs.w, abs.h); }
  }
  ctx.strokeStyle = "rgba(255,255,255,0.35)"; ctx.strokeRect(abs.x, abs.y, abs.w, abs.h);
  ctx.fillStyle = "#bcd7f0"; ctx.font = "11px monospace"; ctx.fillText(n.id, abs.x + 3, abs.y + 12);
  if (n.id === selectedId) {
    ctx.strokeStyle = "#46b0ff"; ctx.lineWidth = 2; ctx.strokeRect(abs.x, abs.y, abs.w, abs.h); ctx.lineWidth = 1;
    [[abs.x,abs.y],[abs.x+abs.w,abs.y],[abs.x+abs.w,abs.y+abs.h],[abs.x,abs.y+abs.h]].forEach(([x,y])=>{ ctx.fillStyle="#46b0ff"; ctx.fillRect(x-4,y-4,8,8); });
  }
  (n.children || []).forEach(ch => drawNode(ch, abs));
}

function renderCanvas() {
  const w = Number(doc.design_size?.w || 1280), h = Number(doc.design_size?.h || 720);
  canvas.width = w; canvas.height = h;
  ctx.clearRect(0,0,w,h);
  ctx.fillStyle = "#0f151c"; ctx.fillRect(0,0,w,h);
  drawNode(doc.root, { x:0,y:0,w,h });
}

function nodeAbsRect(targetId) {
  let out = null;
  function walk(n, base) {
    const r = n.rect || { x:0,y:0,w:0,h:0 };
    const abs = { x: base.x + r.x, y: base.y + r.y, w: r.w, h: r.h };
    if (n.id === targetId) out = abs;
    (n.children||[]).forEach(ch => walk(ch, abs));
  }
  walk(doc.root, { x:0,y:0,w:0,h:0 });
  return out;
}

function hitTest(x, y) {
  let hit = null;
  function walk(n, base) {
    const r = n.rect || { x:0,y:0,w:0,h:0 };
    const abs = { x: base.x + r.x, y: base.y + r.y, w: r.w, h: r.h };
    if (x >= abs.x && y >= abs.y && x <= abs.x + abs.w && y <= abs.y + abs.h) hit = n.id;
    (n.children||[]).forEach(ch => walk(ch, abs));
  }
  walk(doc.root, {x:0,y:0,w:0,h:0});
  return hit;
}

function createBlock(title) {
  const block = document.createElement("div");
  block.className = "block";
  const heading = document.createElement("h4");
  heading.textContent = title;
  block.appendChild(heading);
  return block;
}

function propRow(label, control) {
  const row = document.createElement("div");
  row.className = "row";
  const labelEl = document.createElement("label");
  labelEl.textContent = label;
  row.append(labelEl, control);
  return row;
}

function textInput(value, type = "text") {
  const input = document.createElement("input");
  input.type = type;
  input.value = value;
  return input;
}

function selectInput(options) {
  const select = document.createElement("select");
  options.forEach((value) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = value;
    select.appendChild(option);
  });
  return select;
}

function renderProps() {
  propBody.innerHTML = "";
  const { node, parent } = findNode(selectedId);
  if (!node) return;

  const info = createBlock(node.id);
  const idInput = textInput(node.id);
  idInput.onchange = () => { node.id = idInput.value.trim() || node.id; selectedId = node.id; changed(); };
  info.appendChild(propRow("id", idInput));
  const typeInput = selectInput(["container", "rect", "text", "image", "nine_slice", "vector"]);
  typeInput.value = node.type;
  typeInput.onchange = () => { node.type = typeInput.value; changed(); };
  info.appendChild(propRow("type", typeInput));
  propBody.appendChild(info);

  const rect = createBlock("rect");
  ["x","y","w","h"].forEach(k => {
    const input = textInput(node.rect?.[k] ?? 0, "number");
    input.onchange = () => { node.rect[k] = Number(input.value); changed(false); };
    rect.appendChild(propRow(k, input));
  });
  propBody.appendChild(rect);

  const layout = createBlock("anchor/stretch/layout");
  const layoutInput = selectInput(["absolute", "row", "column"]);
  layoutInput.value = node.layout || "absolute";
  layoutInput.onchange = () => { node.layout = layoutInput.value; changed(); };
  layout.appendChild(propRow("layout", layoutInput));
  const anchorHInput = selectInput(["left", "center", "right"]);
  anchorHInput.value = node.anchor?.h || "left";
  anchorHInput.onchange = () => { node.anchor = node.anchor || {}; node.anchor.h = anchorHInput.value; changed(); };
  layout.appendChild(propRow("anchor.h", anchorHInput));
  const anchorVInput = selectInput(["top", "middle", "bottom"]);
  anchorVInput.value = node.anchor?.v || "top";
  anchorVInput.onchange = () => { node.anchor = node.anchor || {}; node.anchor.v = anchorVInput.value; changed(); };
  layout.appendChild(propRow("anchor.v", anchorVInput));
  ["left","right","top","bottom"].forEach(k => {
    const input = textInput(node.stretch?.[k] ?? "");
    input.onchange = () => { node.stretch = node.stretch || {}; node.stretch[k] = input.value === "" ? null : Number(input.value); changed(); };
    layout.appendChild(propRow(`stretch.${k}`, input));
  });
  propBody.appendChild(layout);

  const typeBlock = createBlock("type-specific");
  if (node.type === "text") {
    node.text = node.text || { value:"", size:16, color:"#ffffff" };
    const textValueInput = textInput(node.text.value || "");
    textValueInput.onchange = () => { node.text.value = textValueInput.value; changed(false); };
    typeBlock.appendChild(propRow("text.value", textValueInput));
    const textSizeInput = textInput(node.text.size || 16, "number");
    textSizeInput.onchange = () => { node.text.size = Number(textSizeInput.value); changed(false); };
    typeBlock.appendChild(propRow("text.size", textSizeInput));
    const textColorInput = textInput(node.text.color || "#ffffff");
    textColorInput.onchange = () => { node.text.color = textColorInput.value; changed(false); };
    typeBlock.appendChild(propRow("text.color", textColorInput));
  }
  if (node.type === "image") {
    node.image = node.image || { src:"", fit:"stretch" };
    const imageSourceInput = textInput(node.image.src || "");
    imageSourceInput.onchange = () => { node.image.src = imageSourceInput.value; changed(); };
    typeBlock.appendChild(propRow("image.src", imageSourceInput));
    const imageFitInput = selectInput(["stretch", "contain", "cover"]);
    imageFitInput.value = node.image.fit || "stretch";
    imageFitInput.onchange = () => { node.image.fit = imageFitInput.value; changed(); };
    typeBlock.appendChild(propRow("image.fit", imageFitInput));
  }
  if (node.type === "vector") {
    node.vector = node.vector || { src:"", fit:"stretch", extrude:0 };
    const vectorSourceInput = textInput(node.vector.src || "");
    vectorSourceInput.onchange = () => { node.vector.src = vectorSourceInput.value; changed(); };
    typeBlock.appendChild(propRow("vector.src", vectorSourceInput));
    const vectorFitInput = selectInput(["stretch", "contain", "cover"]);
    vectorFitInput.value = node.vector.fit || "stretch";
    vectorFitInput.onchange = () => { node.vector.fit = vectorFitInput.value; changed(); };
    typeBlock.appendChild(propRow("vector.fit", vectorFitInput));
    const vectorExtrudeInput = textInput(Number(node.vector.extrude || 0), "number");
    vectorExtrudeInput.onchange = () => { node.vector.extrude = Number(vectorExtrudeInput.value); changed(); };
    typeBlock.appendChild(propRow("vector.extrude", vectorExtrudeInput));
  }
  if (node.type === "nine_slice") {
    node.nine_slice = node.nine_slice || { src:"", left:8,right:8,top:8,bottom:8 };
    const nineSliceSourceInput = textInput(node.nine_slice.src || "");
    nineSliceSourceInput.onchange = () => { node.nine_slice.src = nineSliceSourceInput.value; changed(); };
    typeBlock.appendChild(propRow("9slice.src", nineSliceSourceInput));
    ["left","right","top","bottom"].forEach(k => {
      const input = textInput(Number(node.nine_slice[k] || 0), "number");
      input.onchange = () => { node.nine_slice[k] = Number(input.value); changed(false); };
      typeBlock.appendChild(propRow(`9slice.${k}`, input));
    });
  }
  propBody.appendChild(typeBlock);

  const binds = createBlock("binds (JSON array)");
  const bindsInput = document.createElement("textarea");
  bindsInput.value = JSON.stringify(node.binds || [], null, 2);
  bindsInput.onchange = () => { try { node.binds = JSON.parse(bindsInput.value); changed(); } catch { setStatus("binds JSON parse error", "bad"); } };
  binds.appendChild(propRow("binds", bindsInput));
  propBody.appendChild(binds);

  const hierarchy = createBlock("hierarchy");
  const allNodes = [];
  eachNode(doc.root, null, (n)=>allNodes.push(n));
  const parentInput = document.createElement("select");
  const rootOption = document.createElement("option");
  rootOption.value = "";
  rootOption.textContent = "(none)";
  parentInput.appendChild(rootOption);
  allNodes.forEach(n => { if (n.id !== node.id) { const o = document.createElement("option"); o.value = n.id; o.textContent = n.id; parentInput.appendChild(o); } });
  parentInput.value = parent ? parent.id : "";
  parentInput.onchange = () => {
    if (!parent) return;
    const idx = parent.children.findIndex(ch => ch.id === node.id);
    if (idx >= 0) parent.children.splice(idx, 1);
    const p2 = parentInput.value ? findNode(parentInput.value).node : doc.root;
    if (p2) { p2.children = p2.children || []; p2.children.push(node); }
    changed();
  };
  hierarchy.appendChild(propRow("parent", parentInput));
  const orderButtons = document.createElement("div");
  orderButtons.className = "inlineBtns";
  const upButton = document.createElement("button");
  upButton.textContent = "Up";
  upButton.onclick = () => { if (!parent) return; const i = parent.children.findIndex(ch => ch.id===node.id); if (i>0){ [parent.children[i-1], parent.children[i]]=[parent.children[i], parent.children[i-1]]; changed(); } };
  const downButton = document.createElement("button");
  downButton.textContent = "Down";
  downButton.onclick = () => { if (!parent) return; const i = parent.children.findIndex(ch => ch.id===node.id); if (i>=0 && i<parent.children.length-1){ [parent.children[i+1], parent.children[i]]=[parent.children[i], parent.children[i+1]]; changed(); } };
  orderButtons.append(upButton, downButton);
  hierarchy.appendChild(propRow("order", orderButtons));
  propBody.appendChild(hierarchy);
}

function renderAll() { renderTree(); renderCanvas(); renderProps(); }

function fullDocPatch() {
  return JSON.stringify({
    schema_version: doc.schema_version,
    name: doc.name,
    design_size: doc.design_size,
    root: doc.root
  });
}

async function pushPatch() {
  const payload = fullDocPatch();
  await fetch("/ui_layout/api/bridge/patch", { method:"POST", headers:{"content-type":"application/json"}, body: payload });
  if (ws && ws.readyState === 1) ws.send(JSON.stringify({ op:"patch", payload }));
}

function changed(push = true) {
  renderAll();
  if (push) pushPatch().catch(()=>setStatus("patch push failed", "bad"));
}

function applyPatchString(s, fromRemote) {
  try {
    const p = JSON.parse(s);
    if (p.root) doc.root = p.root;
    if (p.design_size) doc.design_size = p.design_size;
    if (p.name) doc.name = p.name;
    if (!findNode(selectedId).node) selectedId = "root";
    renderAll();
    if (fromRemote) setStatus("remote patch applied", "ok");
  } catch { setStatus("patch parse error", "bad"); }
}
function loadDocString(s, fromRemote) {
  try {
    const d = JSON.parse(s);
    if (!d.root) throw new Error("missing root");
    doc = d;
    if (!findNode(selectedId).node) selectedId = doc.root.id;
    renderAll();
    setStatus(fromRemote ? "remote document loaded" : "file opened", "ok");
  } catch { setStatus("document parse error", "bad"); }
}

canvas.addEventListener("mousedown", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = (e.clientX - rect.left) * (canvas.width / rect.width);
  const y = (e.clientY - rect.top) * (canvas.height / rect.height);
  const hit = hitTest(x, y);
  if (!hit) return;
  selectedId = hit;
  const abs = nodeAbsRect(selectedId);
  if (!abs) return;
  const handles = [
    { k:"tl", x:abs.x, y:abs.y }, { k:"tr", x:abs.x+abs.w, y:abs.y },
    { k:"br", x:abs.x+abs.w, y:abs.y+abs.h }, { k:"bl", x:abs.x, y:abs.y+abs.h }
  ];
  const h = handles.find(h => Math.abs(x-h.x)<=6 && Math.abs(y-h.y)<=6);
  drag = { mode: h ? "resize" : "move", corner: h?.k || "", sx:x, sy:y, rect0:{...abs}, node0:{...findNode(selectedId).node.rect} };
  renderAll();
});

window.addEventListener("mousemove", (e) => {
  if (!drag) return;
  const rect = canvas.getBoundingClientRect();
  const x = (e.clientX - rect.left) * (canvas.width / rect.width);
  const y = (e.clientY - rect.top) * (canvas.height / rect.height);
  const dx = x - drag.sx, dy = y - drag.sy;
  const { node } = findNode(selectedId); if (!node) return;
  if (drag.mode === "move") {
    node.rect.x = Math.round(drag.node0.x + dx);
    node.rect.y = Math.round(drag.node0.y + dy);
  } else {
    if (drag.corner.includes("l")) { node.rect.x = Math.round(drag.node0.x + dx); node.rect.w = Math.max(8, Math.round(drag.node0.w - dx)); }
    if (drag.corner.includes("r")) { node.rect.w = Math.max(8, Math.round(drag.node0.w + dx)); }
    if (drag.corner.includes("t")) { node.rect.y = Math.round(drag.node0.y + dy); node.rect.h = Math.max(8, Math.round(drag.node0.h - dy)); }
    if (drag.corner.includes("b")) { node.rect.h = Math.max(8, Math.round(drag.node0.h + dy)); }
  }
  renderAll();
});
window.addEventListener("mouseup", () => { if (drag) { drag = null; pushPatch().catch(()=>{}); } });

$("addChildBtn").onclick = () => {
  const t = $("newType").value;
  const n = defaultNode(t);
  const target = findNode(selectedId).node || doc.root;
  target.children = target.children || [];
  target.children.push(n);
  selectedId = n.id;
  changed();
};
$("deleteBtn").onclick = () => {
  if (selectedId === doc.root.id) return;
  const { parent } = findNode(selectedId);
  if (!parent) return;
  parent.children = parent.children.filter(ch => ch.id !== selectedId);
  selectedId = doc.root.id;
  changed();
};
$("openBtn").onclick = async () => {
  const p = filePathEl.value.trim();
  const r = await fetch(`/ui_layout/api/file/open?path=${encodeURIComponent(p)}`);
  const j = await r.json();
  if (!j.ok) { setStatus(`open failed: ${j.error}`, "bad"); return; }
  filePathEl.value = j.path;
  loadDocString(j.json, false);
};
$("saveBtn").onclick = async () => {
  const path = filePathEl.value.trim();
  const json = JSON.stringify(doc, null, 2);
  const r = await fetch("/ui_layout/api/file/save", { method:"POST", headers:{"content-type":"application/json"}, body: JSON.stringify({ path, json }) });
  const j = await r.json();
  if (!j.ok) { setStatus(`save failed: ${j.error}`, "bad"); return; }
  setStatus(`saved: ${j.path}`, "ok");
};
$("pushPatchBtn").onclick = () => pushPatch().then(()=>setStatus("patch pushed", "ok")).catch(()=>setStatus("patch push failed", "bad"));
$("pullStateBtn").onclick = async () => {
  const r = await fetch("/ui_layout/api/bridge/pull", { method:"POST" });
  const j = await r.json();
  if (!j.ok) { setStatus("pull failed", "bad"); return; }
  loadDocString(j.payload, true);
};

renderAll();
