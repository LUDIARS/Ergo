// Render Pipeline — two modes:
//
//   Scanner (現状ビュー, read-only)   — how Pictor's hard-coded Vulkan code
//                                       actually renders today (系統B).
//   Profile Editor (編集ビュー, R/W)  — *.profile.json files = how you want
//                                       Pictor to render (系統A).
//
// The modes share nothing: separate API namespaces, separate artifacts on
// disk. The header mode-switch toggles which <main> is visible.

const $ = (id) => document.getElementById(id);
const statusEl = $("status");

// ════════════════════════════════════════════════════════════════════
//  Mode switch
// ════════════════════════════════════════════════════════════════════
const viewScanner = $("view_scanner");
const viewEditor  = $("view_editor");
const btnModeScanner = $("mode_scanner");
const btnModeEditor  = $("mode_editor");

let scannerLoaded = false;
let editorLoaded  = false;

function setMode(mode) {
    const editing = mode === "editor";
    viewScanner.style.display = editing ? "none" : "";
    viewEditor.style.display  = editing ? "" : "none";
    btnModeScanner.classList.toggle("active", !editing);
    btnModeEditor.classList.toggle("active", editing);
    if (editing) {
        if (!editorLoaded) { editorLoaded = true; loadProfileList(); }
        else updateStatusEditor();
    } else {
        if (!scannerLoaded) { scannerLoaded = true; loadScanner(); }
        else updateStatusScanner();
    }
}
btnModeScanner.addEventListener("click", () => setMode("scanner"));
btnModeEditor.addEventListener("click", () => setMode("editor"));

// ════════════════════════════════════════════════════════════════════
//  MODE A — Scanner view (read-only). Logic preserved from Phase 1.
// ════════════════════════════════════════════════════════════════════
const btnRescan   = $("btn_rescan");
const dagMeta     = $("dag_meta");
const dagEl       = $("dag");
const detailTitle = $("detail_title");
const detailBody  = $("detail_body");

document.querySelectorAll(".tabs button").forEach((b) => {
    b.addEventListener("click", () => {
        document.querySelectorAll(".tabs button").forEach((x) => x.classList.remove("active"));
        b.classList.add("active");
        document.querySelectorAll(".tab-body").forEach((t) => t.style.display = "none");
        const tab = $(`tab_${b.dataset.tab}`);
        if (tab) tab.style.display = "";
    });
});

let SNAPSHOT = null;
let DAG_NETWORK = null;

function updateStatusScanner() {
    if (!SNAPSHOT) { statusEl.textContent = "scanner: snapshot 未取得"; return; }
    statusEl.textContent =
        `scanner · passes=${SNAPSHOT.passes.length} pipelines=${SNAPSHOT.pipelines.length} ` +
        `shaders=${SNAPSHOT.shaders.length} attachments=${SNAPSHOT.attachments.length} ` +
        `· scanned ${SNAPSHOT.scanned_at}`;
}

async function loadScanner() {
    statusEl.textContent = "fetching snapshot…";
    try {
        let r = await fetch("./api/snapshot", { cache: "no-cache" });
        if (!r.ok) r = await fetch("./snapshot.json", { cache: "no-cache" });
        if (!r.ok) throw new Error("snapshot 404 — run scanner");
        SNAPSHOT = await r.json();
    } catch (e) {
        statusEl.textContent = "load 失敗: " + e.message;
        return;
    }
    updateStatusScanner();
    dagMeta.textContent = `Pictor: ${SNAPSHOT.pictor_root} · ergo: ${SNAPSHOT.ergo_root}`;
    renderDag(SNAPSHOT);
    renderPipelineTable(SNAPSHOT);
    renderShaders(SNAPSHOT);
    renderAttachments(SNAPSHOT);
}

btnRescan.addEventListener("click", async () => {
    statusEl.textContent = "rescanning…";
    try {
        const r = await fetch("./api/rescan", { method: "POST" });
        const data = await r.json();
        if (data && data.snapshot) {
            SNAPSHOT = data.snapshot;
            renderDag(SNAPSHOT);
            renderPipelineTable(SNAPSHOT);
            renderShaders(SNAPSHOT);
            renderAttachments(SNAPSHOT);
        }
        statusEl.textContent = data.ok ? `rescan 完了 (${data.stdout?.trim() ?? ""})` : `rescan 失敗: ${data.err ?? ""}`;
    } catch (e) {
        statusEl.textContent = "rescan 失敗: " + e.message;
    }
});

function renderDag(snap) {
    const nodes = [];
    const edges = [];
    const passById = {};
    snap.passes.forEach((p) => { passById[p.id] = p; });

    snap.passes.forEach((p, i) => {
        nodes.push({
            id: `pass:${p.id}`,
            label: p.label,
            shape: "box",
            color: { background: "#1c2530", border: "#5aa9ff", highlight: { background: "#243240", border: "#7fbfff" } },
            font: { color: "#e6e8eb", face: "ui-monospace, Consolas" },
            margin: 10,
            level: i,
        });
    });

    snap.passes.forEach((p) => {
        (p.consumes || []).forEach((att) => {
            const producers = snap.passes.filter((q) => (q.produces || []).includes(att));
            producers.forEach((src) => {
                edges.push({
                    from: `pass:${src.id}`,
                    to:   `pass:${p.id}`,
                    label: att,
                    font: { color: "#9aa3ad", size: 10, face: "ui-monospace" },
                    color: { color: "#4a5160", highlight: "#5aa9ff" },
                    arrows: "to",
                });
            });
        });
    });

    const options = {
        layout: { hierarchical: { direction: "UD", sortMethod: "directed", nodeSpacing: 180, levelSeparation: 90 } },
        physics: false,
        interaction: { hover: true },
        edges: { smooth: { type: "cubicBezier", roundness: 0.4 } },
    };
    if (DAG_NETWORK) DAG_NETWORK.destroy();
    DAG_NETWORK = new vis.Network(dagEl, { nodes: new vis.DataSet(nodes), edges: new vis.DataSet(edges) }, options);

    DAG_NETWORK.on("selectNode", (ev) => {
        const id = ev.nodes[0];
        if (!id) return;
        if (id.startsWith("pass:")) showPassDetail(passById[id.slice(5)]);
    });
}

function showPassDetail(p) {
    if (!p) return;
    detailTitle.textContent = `Pass — ${p.label}`;
    const drawsHtml = (p.draws || []).map((d) => `<code>${d}</code>`).join(", ") || "<em>(none)</em>";
    const consumes = (p.consumes || []).map((a) => `<code>${a}</code>`).join(", ") || "<em>(none)</em>";
    const produces = (p.produces || []).map((a) => `<code>${a}</code>`).join(", ") || "<em>(none)</em>";
    detailBody.innerHTML =
        `<table>
           <tr><th>id</th>          <td>${p.id}</td></tr>
           <tr><th>kind</th>        <td>${p.kind}</td></tr>
           <tr><th>consumes</th>    <td>${consumes}</td></tr>
           <tr><th>produces</th>    <td>${produces}</td></tr>
           <tr><th>draws</th>       <td>${drawsHtml}</td></tr>
           <tr><th>description</th> <td>${p.description || ""}</td></tr>
         </table>`;
}

function renderPipelineTable(snap) {
    const tbody = $("pipeline_rows");
    const draw = (filter = "") => {
        const f = filter.toLowerCase();
        tbody.innerHTML = "";
        for (const pl of snap.pipelines) {
            const blob = (pl.function + " " + (pl.shaders || []).join(" ")).toLowerCase();
            if (f && !blob.includes(f)) continue;
            const blend = pl.blend?.enabled
                ? `<span class="tag on">on</span> ${pl.blend.op || ""} <br><span class="small">${pl.blend.src_factor || ""} / ${pl.blend.dst_factor || ""}</span>`
                : `<span class="tag off">off</span>`;
            const cull   = pl.raster?.cull   ? `<code>${pl.raster.cull}</code>`   : "—";
            const depth  = `${pl.depth?.test ? `<span class="tag on">test</span>` : `<span class="tag off">test</span>`}
                            ${pl.depth?.write ? `<span class="tag on">write</span>` : `<span class="tag off">write</span>`}
                            <br><span class="small">${pl.depth?.compare || ""}</span>`;
            const topo   = pl.raster?.topology ? `<code>${pl.raster.topology}</code>` : "—";
            const shaders= (pl.shaders || []).map((s) => `<code>${s}</code>`).join("<br>") || "—";
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><code>${pl.function}</code><br><span class="small">${pl.source}</span></td>
                <td>${blend}</td>
                <td>${cull}</td>
                <td>${depth}</td>
                <td>${topo}</td>
                <td>${shaders}</td>`;
            tbody.appendChild(tr);
        }
    };
    draw();
    $("pipeline_filter").addEventListener("input", (e) => draw(e.target.value));
}

function renderShaders(snap) {
    const listEl  = $("shader_list");
    const titleEl = $("shader_view_title");
    const metaEl  = $("shader_view_meta");
    const codeEl  = $("shader_view_code");

    function paint(filter = "") {
        const f = filter.toLowerCase();
        listEl.innerHTML = "";
        for (const sh of snap.shaders) {
            const blob = (sh.rel_path + " " + sh.stage + " " + sh.label).toLowerCase();
            if (f && !blob.includes(f)) continue;
            const li = document.createElement("li");
            li.innerHTML = `<span>${sh.rel_path}</span>
                            <span class="sub">${sh.stage} · ${sh.lines}L</span>`;
            li.addEventListener("click", () => show(sh, li));
            listEl.appendChild(li);
        }
        const first = listEl.firstChild;
        if (first) first.click();
    }
    function show(sh, li) {
        listEl.querySelectorAll("li").forEach((x) => x.classList.remove("active"));
        li.classList.add("active");
        titleEl.textContent = `${sh.label}/${sh.rel_path}`;
        const sum = sh.summary || {};
        const layoutInfo = (label, list) => list && list.length
            ? `${label}: ` + list.map((e) => `${e.layout ? `[${e.layout}]` : ""}${e.type ? ` ${e.type}` : ""}${e.name ? ` ${e.name}` : ""}`.trim()).join("; ")
            : "";
        metaEl.textContent =
            `stage=${sh.stage} GLSL ${sum.version || "—"} · ` +
            `${sh.size}B · ${sh.lines}L · ` + [
                layoutInfo("in",      sum.ins),
                layoutInfo("out",     sum.outs),
                layoutInfo("uniform", sum.uniforms),
                layoutInfo("buffer",  sum.buffers),
            ].filter(Boolean).join(" | ");
        codeEl.textContent = sh.source || "";
        codeEl.className = "language-glsl";
        if (window.hljs) hljs.highlightElement(codeEl);
    }
    paint();
    $("shader_filter").addEventListener("input", (e) => paint(e.target.value));
}

function renderAttachments(snap) {
    const tbody = $("attachment_rows");
    tbody.innerHTML = "";
    for (const a of snap.attachments) {
        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td><code>${a.id}</code></td>
            <td>${a.label}</td>
            <td><code>${a.format}</code></td>
            <td><code>${a.usage}</code></td>
            <td>${a.owner || "—"}<br><span class="small">${a.note || ""}</span></td>`;
        tbody.appendChild(tr);
    }
}

// ════════════════════════════════════════════════════════════════════
//  MODE B — Profile editor (read/write, disk-persisted)
// ════════════════════════════════════════════════════════════════════
//
// Schema mirror of Pictor's PipelineProfileDef. Canonical source:
//   Pictor/spec/pipeline-profile-config.md  (schema v1)
// Server-side authority is src/plugins/render_pipeline/profile_schema.ts;
// this block must stay in sync with it.

const ENUMS = {
    rendering_path: ["FORWARD", "FORWARD_PLUS", "DEFERRED", "HYBRID"],
    pass_type:      ["DEPTH_ONLY", "OPAQUE", "TRANSPARENT", "SHADOW", "POST_PROCESS", "COMPUTE", "CUSTOM"],
    sort_mode:      ["FRONT_TO_BACK", "BACK_TO_FRONT", "NONE"],
    filter_mode:    ["NONE", "PCF", "PCSS"],
    overlay_mode:   ["OFF", "MINIMAL", "STANDARD", "DETAILED", "TIMELINE"],
    msaa_samples:   [0, 2, 4, 8],
};

// A blank profile used by "+ new". Mirrors profile_schema.ts DEFAULT_PROFILE.
function blankProfile() {
    return {
        version: 1,
        profile_name: "",
        rendering_path: "FORWARD_PLUS",
        max_lights: 256,
        msaa_samples: 0,
        gpu_driven_enabled: true,
        compute_update_enabled: true,
        render_passes: [],
        post_process: [],
        shadow: { cascade_count: 3, resolution: 2048, filter_mode: "PCF" },
        gi: {
            shadow_enabled: true, ssao_enabled: true, gi_probes_enabled: false,
            shadow: {
                cascade_count: 3, resolution: 2048, depth_bias: 0, normal_bias: 0,
                slope_scale_bias: 0, cascade_lambda: 0.5, max_shadow_dist: 150,
                cascade_blend_width: 0, filter_mode: "PCF", shadow_strength: 1,
                pcss_light_size: 0.05, pcss_min_penumbra: 1, pcss_max_penumbra: 16,
                pcss_blocker_search_radius: 8,
            },
            ssao: {
                sample_count: 32, radius: 0.5, bias: 0.025, intensity: 1,
                falloff_start: 0, falloff_end: 1, blur_enabled: true,
            },
            probes: {
                grid_origin: [0, 0, 0], grid_spacing: [1, 1, 1],
                grid_x: 8, grid_y: 8, grid_z: 8, gi_intensity: 1, max_probe_distance: 10,
            },
        },
        memory: {
            frame_allocator_size: 16777216, flight_count: 3, pool_chunk_size: 65536,
            use_large_pages: false,
            gpu: {
                mesh_pool_size: 268435456, ssbo_pool_size: 134217728,
                instance_buffer_size: 67108864, indirect_buffer_size: 16777216,
                staging_buffer_size: 67108864,
            },
        },
        gpu_driven: {
            max_triangle_count: 50000, min_instance_count: 32, workgroup_size: 256,
            two_phase_culling: true, compute_update: true,
        },
        update: { chunk_size: 16384, worker_threads: 0, nt_store_enabled: true, nt_store_threshold: 10000 },
        profiler: { enabled: true, overlay_mode: "STANDARD", max_queries: 64 },
    };
}

const RENDER_PASS_DEFAULT = {
    pass_name: "", pass_type: "OPAQUE", shader_override: "none",
    render_targets: [], input_textures: [], sort_mode: "FRONT_TO_BACK",
    filter_mask: 65535, gpu_driven_pass: false, required_streams: [],
};

// ── editor state ──────────────────────────────────────────────────────
let PROFILE_LIST = [];      // [{file, profile_name, size, mtime}, ...]
let CURRENT = null;         // working PipelineProfileDef (edited copy)
let CURRENT_FILE = null;    // file name on disk, or null for an unsaved new one
let DIRTY = false;

const profileListEl   = $("profile_list");
const profileListNote = $("profile_list_note");
const profileForm     = $("profile_form");
const profileEditTitle= $("profile_edit_title");
const profileEditNote = $("profile_edit_note");
const dirtyFlag       = $("dirty_flag");
const btnSave         = $("btn_save_profile");

function markDirty() {
    DIRTY = true;
    dirtyFlag.style.display = "";
    btnSave.disabled = false;
    refreshJsonPreview();
    updateStatusEditor();
}
function clearDirty() {
    DIRTY = false;
    dirtyFlag.style.display = "none";
    btnSave.disabled = CURRENT == null;
}

function updateStatusEditor() {
    if (!CURRENT) { statusEl.textContent = `editor · ${PROFILE_LIST.length} profiles`; return; }
    statusEl.textContent =
        `editor · ${CURRENT_FILE ?? "(unsaved)"} · ${CURRENT.render_passes.length} passes` +
        (DIRTY ? " · 未保存" : "");
}

// ── profile list ──────────────────────────────────────────────────────
async function loadProfileList(selectFile) {
    statusEl.textContent = "loading profiles…";
    try {
        const r = await fetch("./api/profiles", { cache: "no-cache" });
        const data = await r.json();
        if (!data.ok) throw new Error(data.err || "list failed");
        PROFILE_LIST = data.profiles || [];
        $("profile_dir").textContent = data.dir || "(unknown)";
        profileListNote.textContent = `${PROFILE_LIST.length} files · schema v${data.version}`;
    } catch (e) {
        profileListNote.textContent = "load 失敗: " + e.message;
        statusEl.textContent = "profiles load 失敗";
        return;
    }
    renderProfileList();
    updateStatusEditor();
    if (selectFile) {
        const hit = PROFILE_LIST.find((p) => p.file === selectFile);
        if (hit) openProfile(hit.file);
    }
}

function renderProfileList() {
    profileListEl.innerHTML = "";
    for (const p of PROFILE_LIST) {
        const li = document.createElement("li");
        li.innerHTML = `<div class="pl-name">${escapeHtml(p.profile_name)}</div>
                        <div class="pl-file">${escapeHtml(p.file)}</div>`;
        if (CURRENT_FILE === p.file) li.classList.add("active");
        li.addEventListener("click", () => {
            if (DIRTY && !confirm("未保存の変更があります。破棄して切り替えますか?")) return;
            openProfile(p.file);
        });
        profileListEl.appendChild(li);
    }
}

$("btn_reload_profiles").addEventListener("click", () => {
    if (DIRTY && !confirm("未保存の変更があります。一覧を再読込しますか?")) return;
    loadProfileList(CURRENT_FILE);
});

$("btn_new_profile").addEventListener("click", () => {
    if (DIRTY && !confirm("未保存の変更があります。破棄して新規作成しますか?")) return;
    CURRENT = blankProfile();
    CURRENT.profile_name = "NewProfile";
    CURRENT_FILE = null;     // unsaved — server derives the file name on first save
    renderProfileList();
    renderForm();
    profileEditTitle.textContent = "新規プロファイル (未保存)";
    profileEditNote.textContent = "profile_name を設定して保存すると <name>.profile.json が作られる。";
    markDirty();
});

async function openProfile(file) {
    statusEl.textContent = `loading ${file}…`;
    try {
        const r = await fetch(`./api/profile/${encodeURIComponent(file)}`, { cache: "no-cache" });
        const data = await r.json();
        if (!data.ok) throw new Error(data.err || "load failed");
        CURRENT = data.profile;
        CURRENT_FILE = data.file;
    } catch (e) {
        statusEl.textContent = `load 失敗: ${e.message}`;
        return;
    }
    clearDirty();
    renderProfileList();
    renderForm();
    profileEditTitle.textContent = CURRENT.profile_name || CURRENT_FILE;
    profileEditNote.textContent = `${CURRENT_FILE} を編集中。保存でディスクへ書き込み。`;
    updateStatusEditor();
}

btnSave.addEventListener("click", async () => {
    if (!CURRENT) return;
    statusEl.textContent = "saving…";
    try {
        const body = { profile: CURRENT };
        if (CURRENT_FILE) body.file = CURRENT_FILE;   // else server derives it
        const r = await fetch("./api/profile", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body),
        });
        const data = await r.json();
        if (!data.ok) throw new Error(data.err || "save failed");
        CURRENT = data.profile;
        CURRENT_FILE = data.file;
        clearDirty();
        profileEditTitle.textContent = CURRENT.profile_name || CURRENT_FILE;
        profileEditNote.textContent = `保存しました → ${CURRENT_FILE}`;
        await loadProfileList(CURRENT_FILE);
    } catch (e) {
        statusEl.textContent = `save 失敗: ${e.message}`;
    }
});

// ── form rendering ────────────────────────────────────────────────────
function renderForm() {
    profileForm.style.display = "";
    renderScalarFields();
    renderPasses();
    renderPostProcess();
    renderObjectFields("shadow_fields", CURRENT.shadow, [
        { key: "cascade_count", type: "int" },
        { key: "resolution",    type: "int" },
        { key: "filter_mode",   type: "enum", enum: "filter_mode" },
    ]);
    renderGiFields();
    renderObjectFields("memory_fields", CURRENT.memory, [
        { key: "frame_allocator_size", type: "int", hint: "bytes" },
        { key: "flight_count",         type: "int" },
        { key: "pool_chunk_size",      type: "int", hint: "bytes" },
        { key: "use_large_pages",      type: "bool" },
    ]);
    renderObjectFields("memory_gpu_fields", CURRENT.memory.gpu, [
        { key: "mesh_pool_size",       type: "int", hint: "bytes" },
        { key: "ssbo_pool_size",       type: "int", hint: "bytes" },
        { key: "instance_buffer_size", type: "int", hint: "bytes" },
        { key: "indirect_buffer_size", type: "int", hint: "bytes" },
        { key: "staging_buffer_size",  type: "int", hint: "bytes" },
    ]);
    renderObjectFields("gpu_driven_fields", CURRENT.gpu_driven, [
        { key: "max_triangle_count", type: "int" },
        { key: "min_instance_count", type: "int" },
        { key: "workgroup_size",     type: "int" },
        { key: "two_phase_culling",  type: "bool" },
        { key: "compute_update",     type: "bool" },
    ]);
    renderObjectFields("update_fields", CURRENT.update, [
        { key: "chunk_size",         type: "int" },
        { key: "worker_threads",     type: "int", hint: "0 = auto" },
        { key: "nt_store_enabled",   type: "bool" },
        { key: "nt_store_threshold", type: "int" },
    ]);
    renderObjectFields("profiler_fields", CURRENT.profiler, [
        { key: "enabled",      type: "bool" },
        { key: "overlay_mode", type: "enum", enum: "overlay_mode" },
        { key: "max_queries",  type: "int" },
    ]);
    refreshJsonPreview();
}

function renderScalarFields() {
    const host = $("scalar_fields");
    host.innerHTML = "";
    host.appendChild(fieldText("profile_name", CURRENT.profile_name, (v) => {
        CURRENT.profile_name = v; markDirty();
    }, "マネージャのキー。実用上必須"));
    host.appendChild(fieldEnum("rendering_path", ENUMS.rendering_path, CURRENT.rendering_path, (v) => {
        CURRENT.rendering_path = v; markDirty();
    }));
    host.appendChild(fieldInt("max_lights", CURRENT.max_lights, (v) => {
        CURRENT.max_lights = v; markDirty();
    }));
    host.appendChild(fieldEnumNum("msaa_samples", ENUMS.msaa_samples, CURRENT.msaa_samples, (v) => {
        CURRENT.msaa_samples = v; markDirty();
    }, "0 / 2 / 4 / 8"));
    host.appendChild(fieldBool("gpu_driven_enabled", CURRENT.gpu_driven_enabled, (v) => {
        CURRENT.gpu_driven_enabled = v; markDirty();
    }));
    host.appendChild(fieldBool("compute_update_enabled", CURRENT.compute_update_enabled, (v) => {
        CURRENT.compute_update_enabled = v; markDirty();
    }));
}

// generic: render a fixed list of typed fields for an object
function renderObjectFields(hostId, obj, fields) {
    const host = $(hostId);
    host.innerHTML = "";
    for (const f of fields) {
        if (f.type === "int" || f.type === "float") {
            host.appendChild(fieldNum(f.key, obj[f.key], (v) => { obj[f.key] = v; markDirty(); },
                f.type === "float", f.hint));
        } else if (f.type === "bool") {
            host.appendChild(fieldBool(f.key, obj[f.key], (v) => { obj[f.key] = v; markDirty(); }));
        } else if (f.type === "enum") {
            host.appendChild(fieldEnum(f.key, ENUMS[f.enum], obj[f.key], (v) => { obj[f.key] = v; markDirty(); }));
        } else if (f.type === "vec3") {
            host.appendChild(fieldVec3(f.key, obj[f.key], (v) => { obj[f.key] = v; markDirty(); }));
        }
    }
}

function renderGiFields() {
    renderObjectFields("gi_fields", CURRENT.gi, [
        { key: "shadow_enabled",    type: "bool" },
        { key: "ssao_enabled",      type: "bool" },
        { key: "gi_probes_enabled", type: "bool" },
    ]);
    renderObjectFields("gi_shadow_fields", CURRENT.gi.shadow, [
        { key: "cascade_count",              type: "int" },
        { key: "resolution",                 type: "int" },
        { key: "depth_bias",                 type: "float" },
        { key: "normal_bias",                type: "float" },
        { key: "slope_scale_bias",           type: "float" },
        { key: "cascade_lambda",             type: "float" },
        { key: "max_shadow_dist",            type: "float" },
        { key: "cascade_blend_width",        type: "float" },
        { key: "filter_mode",                type: "enum", enum: "filter_mode" },
        { key: "shadow_strength",            type: "float" },
        { key: "pcss_light_size",            type: "float" },
        { key: "pcss_min_penumbra",          type: "float" },
        { key: "pcss_max_penumbra",          type: "float" },
        { key: "pcss_blocker_search_radius", type: "float" },
    ]);
    renderObjectFields("gi_ssao_fields", CURRENT.gi.ssao, [
        { key: "sample_count",  type: "int" },
        { key: "radius",        type: "float" },
        { key: "bias",          type: "float" },
        { key: "intensity",     type: "float" },
        { key: "falloff_start", type: "float" },
        { key: "falloff_end",   type: "float" },
        { key: "blur_enabled",  type: "bool" },
    ]);
    renderObjectFields("gi_probes_fields", CURRENT.gi.probes, [
        { key: "grid_origin",        type: "vec3" },
        { key: "grid_spacing",       type: "vec3" },
        { key: "grid_x",             type: "int" },
        { key: "grid_y",             type: "int" },
        { key: "grid_z",             type: "int" },
        { key: "gi_intensity",       type: "float" },
        { key: "max_probe_distance", type: "float" },
    ]);
}

// ── render_passes editor ──────────────────────────────────────────────
$("btn_add_pass").addEventListener("click", () => {
    if (!CURRENT) return;
    CURRENT.render_passes.push(JSON.parse(JSON.stringify(RENDER_PASS_DEFAULT)));
    renderPasses();
    markDirty();
});

function renderPasses() {
    const host = $("passes_list");
    host.innerHTML = "";
    const passes = CURRENT.render_passes;
    if (!passes.length) {
        host.innerHTML = `<p class="pep-note">pass 無し — preset の pass 列がそのまま使われる (空配列でも置換扱い)。</p>`;
    }
    passes.forEach((pass, i) => {
        const card = document.createElement("div");
        card.className = "pass-card";

        const head = document.createElement("div");
        head.className = "pc-head";
        head.innerHTML = `<span class="pc-idx">#${i}</span>
                          <strong>${escapeHtml(pass.pass_name || "(unnamed)")}</strong>
                          <span class="spacer"></span>`;
        head.appendChild(makeBtn("▲", "pc-btn", i === 0, () => movePass(i, -1)));
        head.appendChild(makeBtn("▼", "pc-btn", i === passes.length - 1, () => movePass(i, 1)));
        head.appendChild(makeBtn("✕", "pc-btn del", false, () => {
            passes.splice(i, 1); renderPasses(); markDirty();
        }));
        card.appendChild(head);

        const grid = document.createElement("div");
        grid.className = "field-grid";
        grid.appendChild(fieldText("pass_name", pass.pass_name, (v) => {
            pass.pass_name = v; head.querySelector("strong").textContent = v || "(unnamed)"; markDirty();
        }, "ICustomRenderPass::name() と一致で custom dispatch"));
        grid.appendChild(fieldEnum("pass_type", ENUMS.pass_type, pass.pass_type, (v) => {
            pass.pass_type = v; markDirty();
        }));
        grid.appendChild(fieldEnum("sort_mode", ENUMS.sort_mode, pass.sort_mode, (v) => {
            pass.sort_mode = v; markDirty();
        }));
        grid.appendChild(fieldText("shader_override", pass.shader_override, (v) => {
            pass.shader_override = v; markDirty();
        }, '"none" / "handle:<u32>" — scheduler 未消費'));
        grid.appendChild(fieldInt("filter_mask", pass.filter_mask, (v) => {
            pass.filter_mask = v; markDirty();
        }));
        grid.appendChild(fieldBool("gpu_driven_pass", pass.gpu_driven_pass, (v) => {
            pass.gpu_driven_pass = v; markDirty();
        }));
        grid.appendChild(fieldStrList("render_targets", pass.render_targets, () => markDirty(),
            "framebuffer 未配線 (系統B)"));
        grid.appendChild(fieldStrList("input_textures", pass.input_textures, () => markDirty(),
            "未配線 (系統B)"));
        grid.appendChild(fieldStrList("required_streams", pass.required_streams, () => markDirty(),
            "SoA prefetch ヒント"));
        card.appendChild(grid);
        host.appendChild(card);
    });
}

function movePass(i, dir) {
    const passes = CURRENT.render_passes;
    const j = i + dir;
    if (j < 0 || j >= passes.length) return;
    [passes[i], passes[j]] = [passes[j], passes[i]];
    renderPasses();
    markDirty();
}

// ── post_process editor ───────────────────────────────────────────────
$("btn_add_pp").addEventListener("click", () => {
    if (!CURRENT) return;
    CURRENT.post_process.push({ name: "", enabled: true });
    renderPostProcess();
    markDirty();
});

function renderPostProcess() {
    const host = $("pp_list");
    host.innerHTML = "";
    const stack = CURRENT.post_process;
    if (!stack.length) {
        host.innerHTML = `<p class="pep-note">effect 無し — preset の post-process スタックがそのまま使われる。</p>`;
    }
    stack.forEach((pp, i) => {
        const card = document.createElement("div");
        card.className = "pp-card";

        const head = document.createElement("div");
        head.className = "pc-head";
        head.innerHTML = `<span class="pc-idx">#${i}</span>`;
        const nameInput = document.createElement("input");
        nameInput.type = "text";
        nameInput.value = pp.name;
        nameInput.placeholder = "Bloom / SSAO / Tonemapping / TAA / FXAA …";
        nameInput.style.flex = "1";
        nameInput.addEventListener("input", () => { pp.name = nameInput.value; markDirty(); });
        head.appendChild(nameInput);
        head.appendChild(fieldBoolInline("enabled", pp.enabled, (v) => { pp.enabled = v; markDirty(); }));
        head.appendChild(makeBtn("▲", "pc-btn", i === 0, () => movePp(i, -1)));
        head.appendChild(makeBtn("▼", "pc-btn", i === stack.length - 1, () => movePp(i, 1)));
        head.appendChild(makeBtn("✕", "pc-btn del", false, () => {
            stack.splice(i, 1); renderPostProcess(); markDirty();
        }));
        card.appendChild(head);

        // editor-only params (spec §3.4) — round-tripped on disk, ignored by C++
        const paramKeys = pp.params ? Object.keys(pp.params) : [];
        if (paramKeys.length) {
            const grid = document.createElement("div");
            grid.className = "field-grid";
            for (const k of paramKeys) {
                const val = pp.params[k];
                if (typeof val === "number") {
                    grid.appendChild(fieldNum(`params.${k}`, val,
                        (v) => { pp.params[k] = v; markDirty(); }, true, "editor 専用"));
                } else if (typeof val === "boolean") {
                    grid.appendChild(fieldBool(`params.${k}`, val,
                        (v) => { pp.params[k] = v; markDirty(); }));
                } else {
                    grid.appendChild(fieldText(`params.${k}`, String(val),
                        (v) => { pp.params[k] = v; markDirty(); }, "editor 専用"));
                }
            }
            card.appendChild(grid);
        }
        host.appendChild(card);
    });
}

function movePp(i, dir) {
    const stack = CURRENT.post_process;
    const j = i + dir;
    if (j < 0 || j >= stack.length) return;
    [stack[i], stack[j]] = [stack[j], stack[i]];
    renderPostProcess();
    markDirty();
}

// ── JSON preview ──────────────────────────────────────────────────────
function refreshJsonPreview() {
    const el = $("json_preview");
    if (!el) return;
    if (!CURRENT) { el.textContent = ""; return; }
    el.textContent = JSON.stringify(serializeForPreview(CURRENT), null, 2);
    el.className = "language-json";
    if (window.hljs) hljs.highlightElement(el);
}

// Mirror profile_schema.ts serializeProfile(): version first,
// post_process params spread back onto each effect.
function serializeForPreview(p) {
    return {
        version: 1,
        profile_name: p.profile_name,
        rendering_path: p.rendering_path,
        max_lights: p.max_lights,
        msaa_samples: p.msaa_samples,
        gpu_driven_enabled: p.gpu_driven_enabled,
        compute_update_enabled: p.compute_update_enabled,
        render_passes: p.render_passes.map((rp) => ({ ...rp })),
        post_process: p.post_process.map((pp) => {
            const { params, ...rest } = pp;
            return params ? { ...rest, ...params } : { ...rest };
        }),
        shadow: { ...p.shadow },
        gi: {
            shadow_enabled: p.gi.shadow_enabled,
            ssao_enabled: p.gi.ssao_enabled,
            gi_probes_enabled: p.gi.gi_probes_enabled,
            shadow: { ...p.gi.shadow },
            ssao: { ...p.gi.ssao },
            probes: { ...p.gi.probes },
        },
        memory: { ...p.memory, gpu: { ...p.memory.gpu } },
        gpu_driven: { ...p.gpu_driven },
        update: { ...p.update },
        profiler: { ...p.profiler },
    };
}

// ── field widget factories ────────────────────────────────────────────
function escapeHtml(s) {
    return String(s ?? "").replace(/[&<>"]/g, (c) =>
        ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

function fieldWrap(key, hint) {
    const f = document.createElement("div");
    f.className = "field";
    const label = document.createElement("label");
    label.textContent = key;
    f.appendChild(label);
    if (hint) {
        const h = document.createElement("span");
        h.className = "field-hint";
        h.textContent = hint;
        f._hint = h;
    }
    return f;
}

function fieldText(key, value, onChange, hint) {
    const f = fieldWrap(key, hint);
    const input = document.createElement("input");
    input.type = "text";
    input.value = value ?? "";
    input.addEventListener("input", () => onChange(input.value));
    f.appendChild(input);
    if (f._hint) f.appendChild(f._hint);
    return f;
}

function fieldNum(key, value, onChange, isFloat, hint) {
    const f = fieldWrap(key, hint);
    const input = document.createElement("input");
    input.type = "number";
    if (!isFloat) input.step = "1";
    input.value = value ?? 0;
    input.addEventListener("input", () => {
        const n = isFloat ? parseFloat(input.value) : parseInt(input.value, 10);
        if (Number.isFinite(n)) onChange(n);
    });
    f.appendChild(input);
    if (f._hint) f.appendChild(f._hint);
    return f;
}
const fieldInt = (k, v, cb, hint) => fieldNum(k, v, cb, false, hint);

function fieldEnum(key, options, value, onChange, hint) {
    const f = fieldWrap(key, hint);
    const sel = document.createElement("select");
    for (const opt of options) {
        const o = document.createElement("option");
        o.value = opt; o.textContent = opt;
        if (opt === value) o.selected = true;
        sel.appendChild(o);
    }
    sel.addEventListener("change", () => onChange(sel.value));
    f.appendChild(sel);
    if (f._hint) f.appendChild(f._hint);
    return f;
}

function fieldEnumNum(key, options, value, onChange, hint) {
    const f = fieldWrap(key, hint);
    const sel = document.createElement("select");
    for (const opt of options) {
        const o = document.createElement("option");
        o.value = String(opt); o.textContent = String(opt);
        if (opt === value) o.selected = true;
        sel.appendChild(o);
    }
    sel.addEventListener("change", () => onChange(parseInt(sel.value, 10)));
    f.appendChild(sel);
    if (f._hint) f.appendChild(f._hint);
    return f;
}

function fieldBool(key, value, onChange) {
    const f = document.createElement("div");
    f.className = "field bool";
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = !!value;
    cb.addEventListener("change", () => onChange(cb.checked));
    const label = document.createElement("label");
    label.textContent = key;
    f.appendChild(cb);
    f.appendChild(label);
    return f;
}

function fieldBoolInline(key, value, onChange) {
    const wrap = document.createElement("label");
    wrap.style.cssText = "font-size:.74rem;color:var(--text-2);display:flex;gap:4px;align-items:center;";
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = !!value;
    cb.addEventListener("change", () => onChange(cb.checked));
    wrap.appendChild(cb);
    wrap.appendChild(document.createTextNode(key));
    return wrap;
}

function fieldVec3(key, value, onChange) {
    const f = document.createElement("div");
    f.className = "field vec3";
    const label = document.createElement("label");
    label.textContent = `${key} [x,y,z]`;
    f.appendChild(label);
    const box = document.createElement("div");
    box.className = "vec3-inputs";
    const arr = Array.isArray(value) ? value.slice(0, 3) : [0, 0, 0];
    [0, 1, 2].forEach((idx) => {
        const input = document.createElement("input");
        input.type = "number";
        input.value = arr[idx] ?? 0;
        input.addEventListener("input", () => {
            const n = parseFloat(input.value);
            if (Number.isFinite(n)) { arr[idx] = n; onChange(arr); }
        });
        box.appendChild(input);
    });
    f.appendChild(box);
    return f;
}

// string-list editor: chips + an inline "add" input
function fieldStrList(key, arr, onChange, hint) {
    const f = fieldWrap(key, hint);
    const box = document.createElement("div");
    box.className = "strlist";

    function repaint() {
        box.innerHTML = "";
        arr.forEach((s, i) => {
            const chip = document.createElement("span");
            chip.className = "chip";
            chip.appendChild(document.createTextNode(s));
            const x = document.createElement("button");
            x.type = "button"; x.textContent = "✕";
            x.addEventListener("click", () => { arr.splice(i, 1); repaint(); onChange(); });
            chip.appendChild(x);
            box.appendChild(chip);
        });
        const adder = document.createElement("input");
        adder.type = "text";
        adder.placeholder = "+ add";
        adder.addEventListener("keydown", (e) => {
            if (e.key === "Enter" && adder.value.trim()) {
                arr.push(adder.value.trim());
                repaint();
                onChange();
                const next = box.querySelector("input");
                if (next) next.focus();
            }
        });
        box.appendChild(adder);
    }
    repaint();
    f.appendChild(box);
    if (f._hint) f.appendChild(f._hint);
    return f;
}

function makeBtn(label, cls, disabled, onClick) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = cls;
    b.textContent = label;
    b.disabled = !!disabled;
    b.addEventListener("click", onClick);
    return b;
}

// ── WS: react to profile changes from other clients ───────────────────
function connectWs() {
    let url;
    try {
        const base = new URL("./ws", location.href);
        base.protocol = base.protocol === "https:" ? "wss:" : "ws:";
        url = base.toString();
    } catch { return; }
    let ws;
    try { ws = new WebSocket(url); } catch { return; }
    ws.addEventListener("message", (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (msg && msg.op === "profiles-changed") {
            // Refresh the list silently; never clobber unsaved edits.
            if (editorLoaded && !DIRTY) loadProfileList(CURRENT_FILE);
        }
    });
    ws.addEventListener("close", () => setTimeout(connectWs, 3000));
    ws.addEventListener("error", () => {});
}

// ════════════════════════════════════════════════════════════════════
//  Boot — scanner is the default mode.
// ════════════════════════════════════════════════════════════════════
connectWs();
setMode("scanner");
