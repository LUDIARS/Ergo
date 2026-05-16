// Render Pipeline Viewer (Phase 1 静的 scan ベース)
//
// /api/snapshot から JSON を取得し、 vis.js Network で pass DAG を描画、
// 別ペインで pipeline spec table / shader source viewer / attachment table を提供する。
//
// 将来 Phase 2 で `{op:"timing"}` WS イベントを受けて各 pass ノードに
// 実 GPU 時間オーバーレイを表示する想定 (今は ack だけ)。

const $ = (id) => document.getElementById(id);

const statusEl     = $("status");
const btnRescan    = $("btn_rescan");
const dagMeta      = $("dag_meta");
const dagEl        = $("dag");
const detailTitle  = $("detail_title");
const detailBody   = $("detail_body");

// Tabs
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

async function load() {
    statusEl.textContent = "fetching snapshot…";
    try {
        // サーバ提供 (Hono プラグイン) → fail なら ./snapshot.json fallback (静的サイト用)
        let r = await fetch("./api/snapshot", { cache: "no-cache" });
        if (!r.ok) r = await fetch("./snapshot.json", { cache: "no-cache" });
        if (!r.ok) throw new Error("snapshot 404 — run scanner");
        SNAPSHOT = await r.json();
    } catch (e) {
        statusEl.textContent = "load 失敗: " + e.message;
        return;
    }
    statusEl.textContent =
        `passes=${SNAPSHOT.passes.length} pipelines=${SNAPSHOT.pipelines.length} ` +
        `shaders=${SNAPSHOT.shaders.length} attachments=${SNAPSHOT.attachments.length} ` +
        `· scanned ${SNAPSHOT.scanned_at}`;
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

// ─── DAG ────────────────────────────────────────────────────────────
function renderDag(snap) {
    const nodes = [];
    const edges = [];
    const passById = {};
    snap.passes.forEach((p) => { passById[p.id] = p; });

    // pass node
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

    // attachment edges
    snap.passes.forEach((p) => {
        (p.consumes || []).forEach((att) => {
            // どの上流 pass が att を produce するか
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

// ─── Pipeline table ─────────────────────────────────────────────────
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

// ─── Shaders ────────────────────────────────────────────────────────
function renderShaders(snap) {
    const listEl   = $("shader_list");
    const titleEl  = $("shader_view_title");
    const metaEl   = $("shader_view_meta");
    const codeEl   = $("shader_view_code");

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
        // initial selection: first
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
                layoutInfo("in",       sum.ins),
                layoutInfo("out",      sum.outs),
                layoutInfo("uniform",  sum.uniforms),
                layoutInfo("buffer",   sum.buffers),
            ].filter(Boolean).join(" | ");
        codeEl.textContent = sh.source || "";
        codeEl.className = "language-glsl";
        if (window.hljs) hljs.highlightElement(codeEl);
    }
    paint();
    $("shader_filter").addEventListener("input", (e) => paint(e.target.value));
}

// ─── Attachments ────────────────────────────────────────────────────
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

load();
