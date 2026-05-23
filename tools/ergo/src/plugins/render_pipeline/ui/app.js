// Render Pipeline — single NodeGraph editor (Phase 3).
//
// `spec/tool/render_pipeline_system_b.md`。 旧 3 モード (Scanner / Timeline /
// Profile Editor) を廃止し、 *.profile.json を唯一の真理として単一の
// vis-network グラフで表示・編集する。
//
//   nodes  = render_passes[]                  (1 pass = 1 node)
//   edges  = attachment-mediated dependencies (A.render_targets ∩ B.input_textures)
//   left   = AttachmentPanel (attachments[] 編集)
//   right  = InspectorPanel  (選択 pass のフィールド編集)
//   overlay= Phase 2 §6.1 GPU timing を node 着色 + ラベル
//
// vis-network は physics:false / smooth:false でアニメ無効。 hierarchical
// レイアウト (LR) を既定とし、 ノードはドラッグで再配置可能 (永続化なし)。

(() => {
"use strict";

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------

const state = {
    profiles:    [],          // [{file, profile_name, ...}]
    file:        null,        // currently loaded file
    profile:     null,        // PipelineProfileDef
    dirty:       false,
    timing:      Object.create(null), // pass_name -> microseconds (latest frame)
    timingFrame: -1,
    timingEnabled: true,
    selectedPass: null,       // pass_name
    selectedAttachment: null, // name
    selectedPostProcess: -1,  // index into profile.post_process[], or -1
    ws:          null,
};

const $ = (id) => document.getElementById(id);

// --------------------------------------------------------------------------
// API
// --------------------------------------------------------------------------

async function api(path, opts) {
    const r = await fetch(`/render_pipeline/api${path}`, opts || {});
    const j = await r.json().catch(() => ({}));
    if (!r.ok || j.ok === false) {
        throw new Error(j.err || `HTTP ${r.status}`);
    }
    return j;
}

async function reloadProfileList() {
    const r = await api("/profiles");
    state.profiles = r.profiles || [];
    renderProfileSelect();
}

async function loadProfile(file) {
    const r = await api(`/profile/${encodeURIComponent(file)}`);
    state.file    = r.file;
    state.profile = r.profile;
    state.dirty   = false;
    state.selectedPass = null;
    state.selectedAttachment = null;
    syncDirty();
    renderAll();
    setStatus(`loaded ${r.file}`, "ok");
}

async function saveProfile() {
    if (!state.profile) return;
    try {
        const r = await api("/profile", {
            method:  "POST",
            headers: { "Content-Type": "application/json" },
            body:    JSON.stringify({ profile: state.profile, file: state.file }),
        });
        state.dirty = false;
        state.file  = r.file;
        syncDirty();
        await reloadProfileList();
        setStatus(`saved ${r.file}`, "ok");
    } catch (e) {
        setStatus(`save failed: ${e.message}`, "err");
    }
}

// --------------------------------------------------------------------------
// Status helpers
// --------------------------------------------------------------------------

function setStatus(text, level) {
    const el = $("status-msg");
    el.textContent = text;
    el.className = "status-msg" + (level ? " " + level : "");
    if (text && level === "ok") {
        setTimeout(() => {
            if (el.textContent === text) { el.textContent = ""; el.className = "status-msg"; }
        }, 3500);
    }
}

function setWs(text, level) {
    const el = $("ws-status");
    el.textContent = text;
    el.className = "ws-status" + (level ? " " + level : "");
}

function setDirty() {
    if (!state.dirty) {
        state.dirty = true;
        syncDirty();
    }
}

function syncDirty() {
    $("dirty-indicator").classList.toggle("hidden", !state.dirty);
    $("save-btn").disabled = !state.dirty;
}

// --------------------------------------------------------------------------
// Profile <select>
// --------------------------------------------------------------------------

function renderProfileSelect() {
    const sel = $("profile-select");
    sel.innerHTML = "";
    if (!state.profiles.length) {
        const o = document.createElement("option");
        o.value = "";
        o.textContent = "(no profiles)";
        sel.appendChild(o);
        sel.disabled = true;
        return;
    }
    sel.disabled = false;
    for (const p of state.profiles) {
        const o = document.createElement("option");
        o.value = p.file;
        o.textContent = `${p.profile_name} (${p.file})`;
        if (p.file === state.file) o.selected = true;
        sel.appendChild(o);
    }
}

$("profile-select").addEventListener("change", async (e) => {
    if (state.dirty && !confirm("未保存の変更があります。 破棄しますか?")) {
        $("profile-select").value = state.file;
        return;
    }
    await loadProfile(e.target.value);
});

$("save-btn").addEventListener("click", saveProfile);
$("reload-btn").addEventListener("click", async () => {
    if (state.dirty && !confirm("未保存の変更があります。 破棄しますか?")) return;
    if (state.file) await loadProfile(state.file);
});

document.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "s") {
        e.preventDefault();
        saveProfile();
    }
});

// --------------------------------------------------------------------------
// AttachmentPanel
// --------------------------------------------------------------------------

const ATTACHMENT_KINDS  = ["COLOR", "DEPTH", "SWAPCHAIN_COLOR"];
const ATTACHMENT_FORMATS = [
    "R16G16B16A16_SFLOAT", "R32G32B32A32_SFLOAT", "R11G11B10_UFLOAT",
    "R8G8B8A8_UNORM", "R8G8B8A8_SRGB", "B8G8R8A8_UNORM", "B8G8R8A8_SRGB",
    "D16_UNORM", "D24_UNORM_S8_UINT", "D32_SFLOAT", "D32_SFLOAT_S8_UINT",
];

function renderAttachmentPanel() {
    const list = $("attachment-list");
    list.innerHTML = "";
    if (!state.profile) return;
    for (let i = 0; i < state.profile.attachments.length; ++i) {
        const a = state.profile.attachments[i];
        const li = document.createElement("li");
        li.className = `attachment-item kind-${a.kind}`;
        li.innerHTML = `
          <span class="name" title="${escapeAttr(a.format)}">${escapeText(a.name)}</span>
          <span class="badge">${a.kind === "SWAPCHAIN_COLOR" ? "swap" : a.kind.toLowerCase()}</span>
          <button class="delete-btn" title="削除" data-i="${i}">×</button>
        `;
        li.querySelector(".delete-btn").addEventListener("click", () => removeAttachment(i));
        list.appendChild(li);
    }
}

function removeAttachment(i) {
    const a = state.profile.attachments[i];
    if (!a) return;
    if (!confirm(`attachment "${a.name}" を削除しますか?\n参照している pass の render_targets / input_textures からも除去されます。`)) return;
    const name = a.name;
    state.profile.attachments.splice(i, 1);
    // Strip references in render_passes.
    for (const rp of state.profile.render_passes) {
        rp.render_targets = rp.render_targets.filter((n) => n !== name);
        rp.input_textures = rp.input_textures.filter((n) => n !== name);
        rp.attachment_ops = (rp.attachment_ops || []).filter((o) => o.attachment !== name);
    }
    setDirty();
    renderAll();
}

$("att-add-btn").addEventListener("click", () => {
    if (!state.profile) return;
    let name = "new_attachment";
    let n = 1;
    while (state.profile.attachments.some((a) => a.name === name)) {
        n++;
        name = `new_attachment_${n}`;
    }
    state.profile.attachments.push({
        name,
        kind:   "COLOR",
        format: "R16G16B16A16_SFLOAT",
        sizing: "SWAPCHAIN_RELATIVE",
        scale:  1.0,
        usage:  ["COLOR_ATTACHMENT", "SAMPLED"],
        clear_color: [0, 0, 0, 1],
    });
    setDirty();
    renderAll();
});

// --------------------------------------------------------------------------
// GraphView (vis-network)
// --------------------------------------------------------------------------

let network = null;
let nodesDS = null;
let edgesDS = null;

function passNodeColor(pass, latestUs, maxUs) {
    // Base color by pass_type, modulated by timing (red shift if slow).
    const base = {
        OPAQUE:       "#1c2a40",
        TRANSPARENT:  "#1c4040",
        DEPTH_ONLY:   "#2a1c40",
        SHADOW:       "#402a1c",
        POST_PROCESS: "#40341c",
        COMPUTE:      "#3a1c40",
        CUSTOM:       "#1c4030",
    }[pass.pass_type] || "#202833";

    if (!state.timingEnabled || !maxUs || latestUs == null) return { background: base, border: "#465264" };
    const ratio = Math.min(1, latestUs / Math.max(1, maxUs));
    // Mix base toward warm color when slow.
    const r = Math.round(60 + ratio * 195);
    const g = Math.round(60 + (1 - ratio) * 100);
    return { background: `rgb(${r},${g},80)`, border: "#9ece6a" };
}

function makeNodeLabel(pass) {
    const us = state.timing[pass.pass_name];
    const tag = (us != null) ? `\n${(us/1000).toFixed(2)} ms` : "";
    return `${pass.pass_name}\n[${pass.pass_type}]${tag}`;
}

function computeMaxTiming() {
    let max = 0;
    for (const v of Object.values(state.timing)) if (v > max) max = v;
    return max;
}

/// post_process[] の各 effect に対応する synthetic node id を返す。
/// 衝突回避のため `__pp/` プレフィックス付き (pass_name は ASCII slash を含まない前提)。
function postProcessSubNodeId_(index, effectName) {
    return `__pp/${index}:${effectName || "effect"}`;
}

/// post_process[] の sub-node を nodes 配列に追加する。
/// アンカーは render_passes[] 中の最初の POST_PROCESS pass。 アンカーが
/// 無い場合は何もしない。 effect の `enabled` が false なら dim 表示。
function appendPostProcessSubNodes_(nodes) {
    const pp = state.profile.post_process || [];
    if (!pp.length) return;
    const anchor = state.profile.render_passes.find((p) => p.pass_type === "POST_PROCESS");
    if (!anchor) return;

    // kind ごとに色味を変える (vis-network)。 dim 色は enabled=false 用。
    const kindColor = {
        Bloom:        { bg: "#3c2a4a", bd: "#bb9af7" },
        ToneMapping:  { bg: "#2a3c4a", bd: "#7aa2f7" },
        Vignette:     { bg: "#2a4a3c", bd: "#9ece6a" },
        ColorGrading: { bg: "#4a3c2a", bd: "#e0af68" },
        DepthOfField: { bg: "#4a2a3c", bd: "#f7768e" },
        Unknown:      { bg: "#2a2a2a", bd: "#565f89" },
    };

    pp.forEach((effect, i) => {
        const id      = postProcessSubNodeId_(i, effect.name);
        const enabled = effect.enabled !== false;
        const col     = kindColor[effect.kind] || kindColor.Unknown;
        nodes.push({
            id,
            label: `${effect.name || "(unnamed)"}\n[${effect.kind || "Unknown"}]${enabled ? "" : " · off"}`,
            shape: "ellipse",
            color: {
                background: enabled ? col.bg : "#1f1f1f",
                border:     enabled ? col.bd : "#3a3a3a",
                highlight:  { background: col.bg, border: "#7aa2f7" },
            },
            font: {
                color: enabled ? "#e6edf3" : "#7c7466",
                size:  11, face: "monospace",
            },
            margin: 6,
            // Drag 位置の永続化対象から外すための marker (dragEnd で見る)。
            _pp_sub: true,
        });
    });
}

/// post_process[] の sub-node を chain で結ぶ edges を追加する。
///   POST_PROCESS pass → pp[0] → pp[1] → ... → pp[N-1]
function appendPostProcessSubEdges_(edges) {
    const pp = state.profile.post_process || [];
    if (!pp.length) return;
    const anchor = state.profile.render_passes.find((p) => p.pass_type === "POST_PROCESS");
    if (!anchor) return;

    let prevId = anchor.pass_name;
    for (let i = 0; i < pp.length; ++i) {
        const id = postProcessSubNodeId_(i, pp[i].name);
        edges.push({
            id:     `e_pp_chain_${i}`,
            from:   prevId,
            to:     id,
            dashes: true,
            color:  { color: "#bb9af7", highlight: "#e0af68" },
            arrows: "to",
            smooth: false,
        });
        prevId = id;
    }
}

function buildGraph() {
    if (!state.profile) {
        if (network) { network.destroy(); network = null; }
        return;
    }
    const passes = state.profile.render_passes;
    const maxUs = computeMaxTiming();

    const nodes = passes.map((pass) => {
        const col = passNodeColor(pass, state.timing[pass.pass_name], maxUs);
        return {
            id:    pass.pass_name,
            label: makeNodeLabel(pass),
            shape: "box",
            color: { background: col.background, border: col.border, highlight: { background: col.background, border: "#7aa2f7" } },
            font:  { color: "#e6edf3", size: 12, multi: false, face: "monospace" },
            margin: 8,
            shapeProperties: { borderRadius: 4 },
        };
    });

    // post_process[] の各エフェクトを sub-node として PostProcess pass に
    // ぶら下げる (Phase 4 editor improvement)。 sub-node は visualization のみで
    // render_passes[] には書き戻さない (Pictor 側は post_process[] を独自に消費)。
    appendPostProcessSubNodes_(nodes);

    // Edges: producer -> consumer per shared attachment name.
    const edges = [];
    let eid = 0;
    for (const consumer of passes) {
        for (const inAtt of consumer.input_textures) {
            const producer = passes.find((p) => p.render_targets.includes(inAtt));
            if (!producer || producer.pass_name === consumer.pass_name) continue;
            // Color by attachment kind.
            const att = (state.profile.attachments || []).find((a) => a.name === inAtt);
            const stroke = att && att.kind === "DEPTH"           ? "#bb9af7"
                         : att && att.kind === "SWAPCHAIN_COLOR" ? "#9ece6a"
                         : "#7aa2f7";
            edges.push({
                id:    `e${++eid}`,
                from:  producer.pass_name,
                to:    consumer.pass_name,
                label: inAtt,
                font:  { color: "#99a3ad", size: 10, strokeWidth: 0, align: "horizontal" },
                color: { color: stroke, highlight: "#e0af68" },
                arrows: "to",
                smooth: false,
            });
        }
    }
    appendPostProcessSubEdges_(edges);

    // Phase 4 editor: profile._editor.nodePositions から位置を復元。
    //   1 つでも node に保存位置があれば hierarchical を切り、 全 node を free
    //   placement で表示する (vis-network 仕様)。 保存位置の無い node は自動
    //   配置に任せる (vis が空 x/y を自動で動かす)。
    const savedPositions = (state.profile._editor && state.profile._editor.nodePositions) || {};
    const hasAnyPositions = Object.keys(savedPositions).some((n) =>
        passes.some((p) => p.pass_name === n));
    if (hasAnyPositions) {
        for (const n of nodes) {
            const pos = savedPositions[n.id];
            if (pos) { n.x = pos.x; n.y = pos.y; n.fixed = false; }
        }
    }

    if (!network) {
        nodesDS = new vis.DataSet(nodes);
        edgesDS = new vis.DataSet(edges);
        network = new vis.Network($("graph"), { nodes: nodesDS, edges: edgesDS }, {
            physics:  { enabled: false },
            edges:    { smooth: false },
            nodes:    { borderWidth: 1 },
            layout:   hasAnyPositions
                ? { hierarchical: { enabled: false } }
                : {
                    hierarchical: {
                        enabled: true,
                        direction: "LR",
                        sortMethod: "directed",
                        nodeSpacing: 150,
                        levelSeparation: 220,
                    },
                },
            interaction: {
                dragNodes:  true,
                dragView:   true,
                zoomView:   true,
                hover:      true,
                selectable: true,
            },
            manipulation: {
                enabled: false,
                addEdge: (edgeData, callback) => {
                    onEdgeDrawn(edgeData);
                    callback(null); // we apply via state, not vis-network's edges
                },
            },
        });
        network.on("selectNode", (params) => {
            const id = params.nodes[0];
            if (id && id.startsWith("__pp/")) {
                // post_process sub-node — index を取り出して PP inspector を出す。
                const m = id.match(/^__pp\/(\d+):/);
                state.selectedPass = null;
                state.selectedPostProcess = m ? parseInt(m[1], 10) : -1;
                renderInspector();
                return;
            }
            state.selectedPostProcess = -1;
            if (id) showInspector(id);
        });
        network.on("deselectNode", () => {
            state.selectedPass = null;
            state.selectedPostProcess = -1;
            renderInspector();
        });
        network.on("doubleClick", (params) => {
            if (params.nodes.length && !params.nodes[0].startsWith("__pp/")) {
                network.editEdgeMode();
            }
        });
        // Drag 完了で位置を profile._editor.nodePositions に保存。 setDirty
        // して通常の保存フロー (Ctrl+S / 保存ボタン) で disk へ。
        // post_process sub-node の位置は保存しない (chain 配置で十分)。
        network.on("dragEnd", (params) => {
            if (!params.nodes || !params.nodes.length) return;
            const realNodes = params.nodes.filter((id) => !id.startsWith("__pp/"));
            if (!realNodes.length) return;
            const pos = network.getPositions(realNodes);
            if (!state.profile._editor) state.profile._editor = {};
            if (!state.profile._editor.nodePositions) state.profile._editor.nodePositions = {};
            for (const id of realNodes) {
                const p = pos[id];
                if (p) state.profile._editor.nodePositions[id] = { x: p.x, y: p.y };
            }
            setDirty();
        });
    } else {
        nodesDS.clear();
        nodesDS.add(nodes);
        edgesDS.clear();
        edgesDS.add(edges);
    }
}

function onEdgeDrawn(edge) {
    if (!edge.from || !edge.to || edge.from === edge.to) return;
    const producer = state.profile.render_passes.find((p) => p.pass_name === edge.from);
    const consumer = state.profile.render_passes.find((p) => p.pass_name === edge.to);
    if (!producer || !consumer) return;
    let att = producer.render_targets[0];
    if (!att) {
        setStatus(`'${producer.pass_name}' has no render_targets — set one first`, "warn");
        return;
    }
    if (!state.profile.attachments.some((a) => a.name === att)) {
        setStatus(`attachment "${att}" not declared`, "warn");
        return;
    }
    if (!consumer.input_textures.includes(att)) {
        consumer.input_textures.push(att);
        setDirty();
        buildGraph();
        if (state.selectedPass === consumer.pass_name) renderInspector();
        setStatus(`linked ${producer.pass_name} → ${consumer.pass_name} (${att})`, "ok");
    }
    network.disableEditMode();
}

$("pass-add-btn").addEventListener("click", () => {
    if (!state.profile) return;
    let name = "NewPass";
    let n = 1;
    while (state.profile.render_passes.some((p) => p.pass_name === name)) {
        n++;
        name = `NewPass_${n}`;
    }
    state.profile.render_passes.push({
        pass_name:        name,
        pass_type:        "OPAQUE",
        shader_override:  "none",
        render_targets:   [],
        input_textures:   [],
        sort_mode:        "FRONT_TO_BACK",
        filter_mask:      65535,
        gpu_driven_pass:  false,
        required_streams: [],
        attachment_ops:   [],
    });
    setDirty();
    renderAll();
});

$("pp-add-btn").addEventListener("click", () => {
    if (!state.profile) return;
    // 既存 PostProcess pass が無ければ警告 (sub-node は anchor 必須)。
    if (!state.profile.render_passes.some((p) => p.pass_type === "POST_PROCESS")) {
        setStatus("POST_PROCESS の pass が無いと post-process は連結できません (pass を 1 つ追加してから)", "warn");
        return;
    }
    // 名前重複回避。
    let name = "NewEffect";
    let n = 1;
    while ((state.profile.post_process || []).some((pp) => pp.name === name)) {
        n++; name = `NewEffect_${n}`;
    }
    state.profile.post_process.push({
        name,
        enabled: true,
        kind:    "Bloom",
        bloom:          { threshold: 1.0, soft_threshold: 0.5, intensity: 0.8, radius: 5.0, mip_levels: 5, scatter: 0.7 },
        tone_mapping:   { op: "ACES_FILMIC", exposure: 1.0, gamma: 2.2, white_point: 4.0, saturation: 1.0 },
        vignette:       { intensity: 0.35, radius: 0.75, softness: 0.45, color: [0, 0, 0] },
        color_grading:  { lut_path: "", lut_intensity: 1.0, lut_size: 16 },
        depth_of_field: { focus_distance: 10, focus_range: 5, bokeh_radius: 4, near_start: 0, near_end: 3, far_start: 15, far_end: 50, sample_count: 16 },
    });
    setDirty();
    renderAll();
    state.selectedPostProcess = state.profile.post_process.length - 1;
    renderInspector();
});

$("auto-layout-btn").addEventListener("click", () => {
    if (network) {
        network.setOptions({ layout: { hierarchical: { enabled: false } } });
        network.setOptions({ layout: { hierarchical: {
            enabled: true, direction: "LR", sortMethod: "directed",
            nodeSpacing: 150, levelSeparation: 220,
        }}});
        network.fit({ animation: false });
    }
});

// --------------------------------------------------------------------------
// InspectorPanel
// --------------------------------------------------------------------------

const PASS_TYPES = ["DEPTH_ONLY", "OPAQUE", "TRANSPARENT", "SHADOW",
                    "POST_PROCESS", "COMPUTE", "CUSTOM"];
const SORT_MODES = ["FRONT_TO_BACK", "BACK_TO_FRONT", "NONE"];
const LOAD_OPS   = ["LOAD", "CLEAR", "DONT_CARE", "NONE"];
const STORE_OPS  = ["STORE", "DONT_CARE", "NONE"];
const LAYOUTS    = [
    "UNDEFINED", "GENERAL",
    "COLOR_ATTACHMENT_OPTIMAL",
    "DEPTH_STENCIL_ATTACHMENT_OPTIMAL", "DEPTH_STENCIL_READ_ONLY_OPTIMAL",
    "SHADER_READ_ONLY_OPTIMAL",
    "TRANSFER_SRC_OPTIMAL", "TRANSFER_DST_OPTIMAL",
    "PRESENT_SRC_KHR",
];

function showInspector(passName) {
    state.selectedPass = passName;
    renderInspector();
}

function renderInspector() {
    const title = $("inspector-title");
    const body  = $("inspector-body");
    if (!state.profile) {
        title.textContent = "Inspector";
        body.className = "panel-hint";
        body.innerHTML = "プロファイルをロードするとここに表示されます。";
        return;
    }
    if (state.selectedPostProcess >= 0) {
        renderPostProcessInspector_(state.selectedPostProcess, title, body);
        return;
    }
    if (!state.selectedPass) {
        title.textContent = "Inspector";
        body.className = "panel-hint";
        body.innerHTML = "ノードをクリックすると pass / post-process プロパティをここで編集できます。";
        return;
    }
    const pass = state.profile.render_passes.find((p) => p.pass_name === state.selectedPass);
    if (!pass) {
        state.selectedPass = null;
        renderInspector();
        return;
    }
    title.textContent = `Pass: ${pass.pass_name}`;
    body.className = "";

    const attNames = state.profile.attachments.map((a) => a.name);

    body.innerHTML = `
      <div class="field">
        <label>pass_name</label>
        <input type="text" id="inp-name" value="${escapeAttr(pass.pass_name)}">
      </div>
      <div class="field">
        <label>pass_type</label>
        <select id="inp-type">${PASS_TYPES.map((t) =>
          `<option value="${t}" ${t === pass.pass_type ? "selected" : ""}>${t}</option>`).join("")}
        </select>
      </div>
      <div class="field">
        <label>sort_mode</label>
        <select id="inp-sort">${SORT_MODES.map((s) =>
          `<option value="${s}" ${s === pass.sort_mode ? "selected" : ""}>${s}</option>`).join("")}
        </select>
      </div>
      <div class="field">
        <label>filter_mask</label>
        <input type="number" id="inp-mask" value="${pass.filter_mask}">
      </div>
      <div class="field">
        <label>shader_override</label>
        <input type="text" id="inp-shader" value="${escapeAttr(pass.shader_override)}">
      </div>

      <h4>render_targets</h4>
      <div id="targets-list"></div>
      <button id="targets-add" type="button">+ target</button>

      <h4>input_textures</h4>
      <div id="inputs-list"></div>
      <button id="inputs-add" type="button">+ input</button>

      <div id="usage-warnings"></div>

      <h4>attachment_ops</h4>
      <table class="ops-grid"><tbody id="ops-body"></tbody></table>
      <div class="panel-hint" style="margin:0;">
        空のとき Pictor が既定で推論 (color=CLEAR/STORE/SHADER_READ_ONLY、
        depth=CLEAR/DONT_CARE/DSV、 swapchain=CLEAR/STORE/PRESENT_SRC_KHR)。
      </div>
      <button id="ops-fill" type="button" style="margin-top:6px;">Auto-fill from targets</button>

      <div class="danger-zone">
        <button id="pass-delete" type="button">この pass を削除</button>
      </div>
    `;

    // Bind basic fields.
    $("inp-name").addEventListener("change", (e) => {
        const newName = e.target.value.trim();
        if (!newName || newName === pass.pass_name) return;
        if (state.profile.render_passes.some((p) => p.pass_name === newName)) {
            setStatus(`pass name '${newName}' duplicates an existing pass`, "warn");
            e.target.value = pass.pass_name;
            return;
        }
        pass.pass_name = newName;
        state.selectedPass = newName;
        setDirty();
        renderAll();
    });
    $("inp-type").addEventListener("change",  (e) => { pass.pass_type = e.target.value; setDirty(); buildGraph(); });
    $("inp-sort").addEventListener("change",  (e) => { pass.sort_mode = e.target.value; setDirty(); });
    $("inp-mask").addEventListener("change",  (e) => { pass.filter_mask = parseInt(e.target.value, 10) || 0; setDirty(); });
    $("inp-shader").addEventListener("change", (e) => { pass.shader_override = e.target.value.trim() || "none"; setDirty(); });

    // render_targets / input_textures lists.
    const tgtList = $("targets-list");
    pass.render_targets.forEach((name, i) => {
        const row = document.createElement("div");
        row.className = "target-row";
        row.innerHTML = `<select>${attNames.map((n) =>
            `<option value="${escapeAttr(n)}" ${n === name ? "selected" : ""}>${escapeText(n)}</option>`).join("")}
          </select><button type="button" data-i="${i}">×</button>`;
        row.querySelector("select").addEventListener("change", (e) => {
            pass.render_targets[i] = e.target.value;
            setDirty(); renderInspector(); buildGraph();
        });
        row.querySelector("button").addEventListener("click", () => {
            pass.render_targets.splice(i, 1);
            setDirty(); renderInspector(); buildGraph();
        });
        tgtList.appendChild(row);
    });
    $("targets-add").addEventListener("click", () => {
        if (!attNames.length) { setStatus("先に Attachments を追加してください", "warn"); return; }
        pass.render_targets.push(attNames[0]);
        setDirty(); renderInspector(); buildGraph();
    });

    const inList = $("inputs-list");
    pass.input_textures.forEach((name, i) => {
        const row = document.createElement("div");
        row.className = "target-row";
        row.innerHTML = `<select>${attNames.map((n) =>
            `<option value="${escapeAttr(n)}" ${n === name ? "selected" : ""}>${escapeText(n)}</option>`).join("")}
          </select><button type="button" data-i="${i}">×</button>`;
        row.querySelector("select").addEventListener("change", (e) => {
            pass.input_textures[i] = e.target.value;
            setDirty(); renderInspector(); buildGraph();
        });
        row.querySelector("button").addEventListener("click", () => {
            pass.input_textures.splice(i, 1);
            setDirty(); renderInspector(); buildGraph();
        });
        inList.appendChild(row);
    });
    $("inputs-add").addEventListener("click", () => {
        if (!attNames.length) { setStatus("先に Attachments を追加してください", "warn"); return; }
        pass.input_textures.push(attNames[0]);
        setDirty(); renderInspector(); buildGraph();
    });

    // attachment_ops grid.
    const opsBody = $("ops-body");
    const headerRow = document.createElement("tr");
    headerRow.innerHTML = "<th>attachment</th><th>load</th><th>store</th><th>initial</th><th>final</th>";
    opsBody.appendChild(headerRow);
    (pass.attachment_ops || []).forEach((op, i) => {
        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td><select data-k="attachment">${attNames.map((n) =>
            `<option value="${escapeAttr(n)}" ${n === op.attachment ? "selected" : ""}>${escapeText(n)}</option>`).join("")}</select></td>
          <td><select data-k="load">${LOAD_OPS.map((v) =>
            `<option value="${v}" ${v === op.load ? "selected" : ""}>${v}</option>`).join("")}</select></td>
          <td><select data-k="store">${STORE_OPS.map((v) =>
            `<option value="${v}" ${v === op.store ? "selected" : ""}>${v}</option>`).join("")}</select></td>
          <td><select data-k="initial_layout">${LAYOUTS.map((v) =>
            `<option value="${v}" ${v === op.initial_layout ? "selected" : ""}>${v}</option>`).join("")}</select></td>
          <td><select data-k="final_layout">${LAYOUTS.map((v) =>
            `<option value="${v}" ${v === op.final_layout ? "selected" : ""}>${v}</option>`).join("")}</select></td>
        `;
        tr.querySelectorAll("select").forEach((sel) => {
            sel.addEventListener("change", () => {
                op[sel.dataset.k] = sel.value;
                setDirty();
            });
        });
        opsBody.appendChild(tr);
    });

    $("ops-fill").addEventListener("click", () => {
        pass.attachment_ops = pass.render_targets.map((tgt) => {
            const att = state.profile.attachments.find((a) => a.name === tgt);
            if (att && att.kind === "DEPTH") {
                return { attachment: tgt, load: "CLEAR", store: "DONT_CARE",
                         initial_layout: "UNDEFINED",
                         final_layout: "DEPTH_STENCIL_ATTACHMENT_OPTIMAL" };
            }
            if (att && att.kind === "SWAPCHAIN_COLOR") {
                return { attachment: tgt, load: "CLEAR", store: "STORE",
                         initial_layout: "UNDEFINED",
                         final_layout: "PRESENT_SRC_KHR" };
            }
            return { attachment: tgt, load: "CLEAR", store: "STORE",
                     initial_layout: "UNDEFINED",
                     final_layout: "SHADER_READ_ONLY_OPTIMAL" };
        });
        setDirty(); renderInspector();
    });

    $("pass-delete").addEventListener("click", () => {
        if (!confirm(`pass "${pass.pass_name}" を削除しますか?`)) return;
        const idx = state.profile.render_passes.indexOf(pass);
        if (idx >= 0) state.profile.render_passes.splice(idx, 1);
        state.selectedPass = null;
        setDirty(); renderAll();
    });

    // attachment usage validation (Phase 4 editor improvement) — pass の
    // render_targets / input_textures が参照する attachment の usage bit が
    // 用法と整合するかチェックして warning を出す。 実行時 validation layer
    // を待たずに編集時に気付ける。
    const warnings = validateAttachmentUsage_(pass, state.profile);
    const warnBox = $("usage-warnings");
    if (warnings.length) {
        warnBox.innerHTML = `<h4>⚠ usage 整合性</h4>` + warnings.map((w) =>
            `<div class="panel-hint warn">• ${escapeText(w)}</div>`).join("");
    } else {
        warnBox.innerHTML = "";
    }
}

// --------------------------------------------------------------------------
// PostProcessInspector — sub-node クリックで開く kind 別パラメータエディタ
// --------------------------------------------------------------------------

const TONE_MAP_OPS = [
    "ACES_FILMIC", "REINHARD", "REINHARD_EXT", "UNCHARTED2", "LINEAR_CLAMP",
];
const PP_KINDS = [
    "Unknown", "Bloom", "ToneMapping", "Vignette", "ColorGrading", "DepthOfField",
];

/// kind ごとのパラメータ定義 (UI 自動生成用)。
///   ranges は number input の min/max/step ヒント (validation はしない、
///   ユーザが手で外れた値を入れることも許す)。
const PP_PARAM_SPECS = {
    Bloom: {
        block: "bloom",
        fields: [
            { key: "threshold",      label: "Threshold",       type: "number", step: 0.01, min: 0,  max: 5 },
            { key: "soft_threshold", label: "Soft Threshold",  type: "number", step: 0.01, min: 0,  max: 1 },
            { key: "intensity",      label: "Intensity",       type: "number", step: 0.01, min: 0,  max: 3 },
            { key: "radius",         label: "Radius",          type: "number", step: 0.1,  min: 0,  max: 20 },
            { key: "mip_levels",     label: "Mip Levels",      type: "int",    step: 1,    min: 1,  max: 8 },
            { key: "scatter",        label: "Scatter",         type: "number", step: 0.01, min: 0,  max: 1 },
        ],
    },
    ToneMapping: {
        block: "tone_mapping",
        fields: [
            { key: "op",          label: "Operator",   type: "enum",   options: TONE_MAP_OPS },
            { key: "exposure",    label: "Exposure",   type: "number", step: 0.01, min: 0,  max: 10 },
            { key: "gamma",       label: "Gamma",      type: "number", step: 0.01, min: 1,  max: 3 },
            { key: "white_point", label: "White Point",type: "number", step: 0.1,  min: 1,  max: 20 },
            { key: "saturation",  label: "Saturation", type: "number", step: 0.01, min: 0,  max: 3 },
        ],
    },
    Vignette: {
        block: "vignette",
        fields: [
            { key: "intensity", label: "Intensity", type: "number", step: 0.01, min: 0, max: 1 },
            { key: "radius",    label: "Radius",    type: "number", step: 0.01, min: 0, max: 2 },
            { key: "softness",  label: "Softness",  type: "number", step: 0.01, min: 0, max: 1 },
            { key: "color",     label: "Color",     type: "vec3",   step: 0.01, min: 0, max: 1 },
        ],
    },
    ColorGrading: {
        block: "color_grading",
        fields: [
            { key: "lut_path",      label: "LUT Path",      type: "text" },
            { key: "lut_intensity", label: "LUT Intensity", type: "number", step: 0.01, min: 0, max: 1 },
            { key: "lut_size",      label: "LUT Size",      type: "int",    step: 1,    min: 4, max: 64 },
        ],
    },
    DepthOfField: {
        block: "depth_of_field",
        fields: [
            { key: "focus_distance", label: "Focus Distance", type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "focus_range",    label: "Focus Range",    type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "bokeh_radius",   label: "Bokeh Radius",   type: "number", step: 0.1,  min: 0,    max: 50 },
            { key: "near_start",     label: "Near Start",     type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "near_end",       label: "Near End",       type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "far_start",      label: "Far Start",      type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "far_end",        label: "Far End",        type: "number", step: 0.1,  min: 0,    max: 1000 },
            { key: "sample_count",   label: "Sample Count",   type: "int",    step: 1,    min: 1,    max: 128 },
        ],
    },
    Unknown: {
        block: null,
        fields: [],
    },
};

function renderPostProcessInspector_(index, title, body) {
    const pp = (state.profile.post_process || [])[index];
    if (!pp) {
        // インデックス更新 (削除等) で stale: クリアして normal inspector へ。
        state.selectedPostProcess = -1;
        renderInspector();
        return;
    }
    title.textContent = `Post-Process [${index}]: ${pp.name || "(unnamed)"}`;
    body.className = "";

    const spec = PP_PARAM_SPECS[pp.kind] || PP_PARAM_SPECS.Unknown;
    const block = spec.block;

    // ── ヘッダ部 (name / kind / enabled) ────────────────────────────────
    const header = `
      <div class="field">
        <label>name</label>
        <input type="text" id="pp-name" value="${escapeAttr(pp.name)}">
      </div>
      <div class="field">
        <label>kind</label>
        <select id="pp-kind">${PP_KINDS.map((k) =>
          `<option value="${k}" ${k === pp.kind ? "selected" : ""}>${k}</option>`).join("")}
        </select>
      </div>
      <div class="field">
        <label><input type="checkbox" id="pp-enabled" ${pp.enabled ? "checked" : ""}> enabled</label>
      </div>
    `;

    // ── パラメータブロック (kind 別) ─────────────────────────────────────
    let paramsHtml = "";
    if (block) {
        paramsHtml += `<h4>${pp.kind} parameters</h4>`;
        for (const f of spec.fields) {
            const v = (pp[block] || {})[f.key];
            if (f.type === "enum") {
                paramsHtml += `<div class="field">
                    <label>${escapeText(f.label)}</label>
                    <select data-k="${escapeAttr(f.key)}">${f.options.map((o) =>
                      `<option value="${o}" ${o === v ? "selected" : ""}>${o}</option>`).join("")}
                    </select></div>`;
            } else if (f.type === "text") {
                paramsHtml += `<div class="field">
                    <label>${escapeText(f.label)}</label>
                    <input type="text" data-k="${escapeAttr(f.key)}" value="${escapeAttr(v ?? "")}"></div>`;
            } else if (f.type === "vec3") {
                const a = Array.isArray(v) ? v : [0, 0, 0];
                paramsHtml += `<div class="field">
                    <label>${escapeText(f.label)} (RGB)</label>
                    <div class="vec3-row">
                      <input type="number" data-k="${escapeAttr(f.key)}" data-i="0"
                             step="${f.step}" min="${f.min}" max="${f.max}" value="${a[0]}">
                      <input type="number" data-k="${escapeAttr(f.key)}" data-i="1"
                             step="${f.step}" min="${f.min}" max="${f.max}" value="${a[1]}">
                      <input type="number" data-k="${escapeAttr(f.key)}" data-i="2"
                             step="${f.step}" min="${f.min}" max="${f.max}" value="${a[2]}">
                    </div></div>`;
            } else {
                // number / int 共通
                paramsHtml += `<div class="field">
                    <label>${escapeText(f.label)}</label>
                    <input type="number" data-k="${escapeAttr(f.key)}"
                           step="${f.step}" min="${f.min}" max="${f.max}" value="${v ?? 0}"></div>`;
            }
        }
    } else {
        paramsHtml = `<div class="panel-hint">kind=Unknown のためパラメータは編集不可。 kind を切り替えると該当パラメータ編集が出ます。</div>`;
    }

    body.innerHTML = header + paramsHtml + `
      <div class="danger-zone">
        <button id="pp-delete" type="button">この post-process を削除</button>
      </div>
    `;

    // ── 値の wiring (input → state) ─────────────────────────────────────
    $("pp-name").addEventListener("change", (e) => {
        pp.name = e.target.value.trim();
        setDirty(); buildGraph(); renderInspector();
    });
    $("pp-kind").addEventListener("change", (e) => {
        pp.kind = e.target.value;
        setDirty(); buildGraph(); renderInspector();
    });
    $("pp-enabled").addEventListener("change", (e) => {
        pp.enabled = e.target.checked;
        setDirty(); buildGraph();
    });

    if (block) {
        body.querySelectorAll(`[data-k]`).forEach((el) => {
            const key = el.dataset.k;
            const f = spec.fields.find((x) => x.key === key);
            if (!f) return;
            el.addEventListener("change", () => {
                if (!pp[block]) pp[block] = {};
                if (f.type === "vec3") {
                    const arr = Array.isArray(pp[block][key]) ? pp[block][key].slice() : [0, 0, 0];
                    arr[parseInt(el.dataset.i, 10)] = parseFloat(el.value) || 0;
                    pp[block][key] = arr;
                } else if (f.type === "int") {
                    pp[block][key] = parseInt(el.value, 10) || 0;
                } else if (f.type === "number") {
                    pp[block][key] = parseFloat(el.value) || 0;
                } else {
                    pp[block][key] = el.value;
                }
                setDirty();
            });
        });
    }

    $("pp-delete").addEventListener("click", () => {
        if (!confirm(`post-process [${index}] "${pp.name}" を削除しますか?`)) return;
        state.profile.post_process.splice(index, 1);
        state.selectedPostProcess = -1;
        setDirty(); renderAll();
    });
}

/// pass の attachment 参照と AttachmentDef.usage が整合するか検査する。
/// 戻り値は human-readable な警告メッセージ配列 (空 = 問題なし)。
///
///   - render_targets[i]: 参照先の usage に COLOR_ATTACHMENT (DEPTH の場合は
///     DEPTH_STENCIL_ATTACHMENT) が無ければ warning。 SWAPCHAIN_COLOR は
///     COLOR_ATTACHMENT で OK。
///   - input_textures[i]: 参照先の usage に SAMPLED が無ければ warning。
///   - 該当 attachment 名が attachments[] に無い → warning (未登録)
function validateAttachmentUsage_(pass, profile) {
    const out = [];
    const attsByName = new Map(profile.attachments.map((a) => [a.name, a]));

    for (const tgt of pass.render_targets) {
        const att = attsByName.get(tgt);
        if (!att) { out.push(`render_targets[${tgt}] が attachments[] に未登録`); continue; }
        const usage = att.usage || [];
        if (att.kind === "DEPTH") {
            if (!usage.includes("DEPTH_STENCIL_ATTACHMENT")) {
                out.push(`render_targets[${tgt}] は DEPTH attachment だが usage に DEPTH_STENCIL_ATTACHMENT が無い`);
            }
        } else {
            // COLOR / SWAPCHAIN_COLOR とも COLOR_ATTACHMENT が必要
            if (!usage.includes("COLOR_ATTACHMENT")) {
                out.push(`render_targets[${tgt}] は ${att.kind} attachment だが usage に COLOR_ATTACHMENT が無い`);
            }
        }
    }

    for (const inp of pass.input_textures) {
        const att = attsByName.get(inp);
        if (!att) { out.push(`input_textures[${inp}] が attachments[] に未登録`); continue; }
        const usage = att.usage || [];
        if (!usage.includes("SAMPLED")) {
            out.push(`input_textures[${inp}] は usage に SAMPLED が無い (descriptor binding 不可)`);
        }
    }

    return out;
}

// --------------------------------------------------------------------------
// Timing overlay
// --------------------------------------------------------------------------

$("timing-overlay-toggle").addEventListener("change", (e) => {
    state.timingEnabled = e.target.checked;
    buildGraph();
});

function applyTiming(msg) {
    if (msg.frame != null) state.timingFrame = msg.frame;
    if (Array.isArray(msg.passes)) {
        for (const p of msg.passes) {
            if (p && p.id != null && p.us != null) state.timing[p.id] = p.us;
        }
    } else if (msg.pass && msg.us != null) {
        state.timing[msg.pass] = msg.us;
    }
    buildGraph();
}

// --------------------------------------------------------------------------
// WebSocket
// --------------------------------------------------------------------------

function connectWs() {
    const proto = location.protocol === "https:" ? "wss" : "ws";
    const url = `${proto}://${location.host}/render_pipeline/ws`;
    setWs("…");
    state.ws = new WebSocket(url);
    state.ws.addEventListener("open",  () => setWs("connected", "ok"));
    state.ws.addEventListener("close", () => { setWs("disconnected", "err"); setTimeout(connectWs, 3000); });
    state.ws.addEventListener("error", () => setWs("error", "err"));
    state.ws.addEventListener("message", (ev) => {
        let msg; try { msg = JSON.parse(ev.data); } catch { return; }
        if (msg.op === "timing") {
            applyTiming(msg);
        } else if (msg.op === "profiles-changed") {
            if (msg.file && msg.file === state.file && !state.dirty) {
                // Re-pull silently.
                loadProfile(state.file).catch((e) => setStatus(e.message, "err"));
            } else {
                reloadProfileList().catch(() => {});
            }
        }
    });
}

// --------------------------------------------------------------------------
// Utilities
// --------------------------------------------------------------------------

function escapeText(s) {
    return String(s).replace(/[&<>]/g, (c) => ({ "&":"&amp;", "<":"&lt;", ">":"&gt;" }[c]));
}
function escapeAttr(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({ "&":"&amp;", "<":"&lt;", ">":"&gt;", '"':"&quot;", "'":"&#39;" }[c]));
}

// --------------------------------------------------------------------------
// Render all
// --------------------------------------------------------------------------

function renderAll() {
    renderAttachmentPanel();
    buildGraph();
    renderInspector();
    renderProfileSelect();
}

// --------------------------------------------------------------------------
// Boot
// --------------------------------------------------------------------------

(async function boot() {
    try {
        await reloadProfileList();
        if (state.profiles.length) {
            await loadProfile(state.profiles[0].file);
        } else {
            setStatus("profile directory is empty", "warn");
        }
        connectWs();
    } catch (e) {
        setStatus(`boot failed: ${e.message}`, "err");
        connectWs();
    }
})();

})();
