// Terrain plugin UI.
//
// Minimal imperative controller over the REST API. Model of the day:
//   - snapshot: the whole store loaded once at startup and mutated in
//     place between saves.
//   - a WS reload notification forces a refetch (other UIs mutated state).
//
// The rendered SVG patterns are purely a preview aid — the runtime picks
// one deterministically via `pickPattern(category, stageId, fieldId)`.

const API_BASE = "/terrain/api";

const statusEl      = document.getElementById("status");
const storePathEl   = document.getElementById("store-path");
const stageListEl   = document.getElementById("stage-list");
const stageAddBtn   = document.getElementById("stage-add");
const editorEl      = document.getElementById("stage-editor");
const previewGridEl = document.getElementById("preview-grid");
const previewHintEl = document.getElementById("preview-hint");

const CATEGORY_META = {
    grass:  { label: "草原", accent: "#6aa84f" },
    soil:   { label: "土",   accent: "#8b6239" },
    ice:    { label: "氷",   accent: "#a3cfe6" },
    cobble: { label: "石畳", accent: "#6e6e76" },
    sand:   { label: "砂",   accent: "#d4b074" },
    water:  { label: "水",   accent: "#3d6fa3" },
    lava:   { label: "溶岩", accent: "#d14f2b" },
    wood:   { label: "木",   accent: "#a07a44" },
};

const state = {
    meta: null,          // { categories, patterns, storePath, version }
    stages: [],          // Stage[]
    selectedStageId: null,
    selectedFieldIndex: null,
    saveTimer: null,
};

// ─── tiny helpers ─────────────────────────────────────────────

function setStatus(text, kind = "") {
    statusEl.textContent = text;
    statusEl.className = "status" + (kind ? " " + kind : "");
}
function el(tag, attrs = {}, ...children) {
    const n = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
        if (k === "class")     n.className = v;
        else if (k === "text") n.textContent = v;
        else if (k.startsWith("on") && typeof v === "function") {
            n.addEventListener(k.slice(2).toLowerCase(), v);
        } else {
            n.setAttribute(k, v);
        }
    }
    for (const c of children) {
        if (c == null) continue;
        n.appendChild(typeof c === "string" ? document.createTextNode(c) : c);
    }
    return n;
}
async function fetchJson(method, path, body) {
    const opts = { method, headers: { "content-type": "application/json" } };
    if (body !== undefined) opts.body = JSON.stringify(body);
    const res = await fetch(API_BASE + path, opts);
    if (!res.ok) {
        const text = await res.text().catch(() => "");
        throw new Error(`${method} ${path}: ${res.status} ${text}`);
    }
    return res.json();
}

// ─── data load / save ─────────────────────────────────────────

async function loadAll() {
    setStatus("loading…");
    try {
        const [meta, store] = await Promise.all([
            fetchJson("GET", "/meta"),
            fetchJson("GET", "/store"),
        ]);
        state.meta = meta;
        state.stages = Array.isArray(store.stages) ? store.stages : [];
        storePathEl.textContent = meta.storePath || "";
        renderStageList();
        if (!state.selectedStageId && state.stages.length > 0) {
            state.selectedStageId = state.stages[0].id;
        }
        renderEditor();
        setStatus("ready", "ok");
    } catch (err) {
        setStatus("load failed: " + err.message, "err");
        console.error(err);
    }
}

function scheduleSave(stage) {
    if (state.saveTimer) clearTimeout(state.saveTimer);
    state.saveTimer = setTimeout(() => saveStage(stage), 220);
}
async function saveStage(stage) {
    try {
        setStatus("saving…");
        await fetchJson("PUT", `/stages/${encodeURIComponent(stage.id)}`, stage);
        setStatus("saved", "ok");
    } catch (err) {
        setStatus("save failed: " + err.message, "err");
    }
}

// ─── render: stage list ───────────────────────────────────────

function renderStageList() {
    stageListEl.innerHTML = "";
    if (state.stages.length === 0) {
        stageListEl.appendChild(el("li", { class: "hint" }, "まだ Stage がありません。＋ で追加してください。"));
        return;
    }
    const tpl = document.getElementById("tpl-stage-row");
    for (const stage of state.stages) {
        const li = tpl.content.firstElementChild.cloneNode(true);
        li.querySelector(".stage-label").textContent = stage.name || stage.id;
        li.querySelector(".stage-count").textContent =
            stage.fields.length + " field" + (stage.fields.length === 1 ? "" : "s");
        if (stage.id === state.selectedStageId) li.classList.add("selected");
        li.addEventListener("click", () => {
            state.selectedStageId    = stage.id;
            state.selectedFieldIndex = null;
            renderStageList();
            renderEditor();
        });
        stageListEl.appendChild(li);
    }
}

// ─── render: editor for selected stage ────────────────────────

function currentStage() {
    return state.stages.find((s) => s.id === state.selectedStageId) ?? null;
}

function renderEditor() {
    const stage = currentStage();
    editorEl.innerHTML = "";
    if (!stage) {
        editorEl.appendChild(el("p", { class: "hint" }, "左から Stage を選択してください。"));
        renderPreview(null);
        return;
    }

    // header: rename + id + delete
    const header = el("div", { class: "editor-header" },
        el("input", {
            type: "text",
            class: "stage-name",
            value: stage.name,
            onInput: (e) => { stage.name = e.target.value; scheduleSave(stage); renderStageList(); },
        }),
        el("span", { class: "stage-id", text: `id = ${stage.id}` }),
        el("button", {
            class: "danger",
            onClick: async () => {
                if (!confirm(`Stage "${stage.name}" を削除しますか?`)) return;
                await fetchJson("DELETE", `/stages/${encodeURIComponent(stage.id)}`);
                state.stages = state.stages.filter((s) => s.id !== stage.id);
                state.selectedStageId = state.stages[0]?.id ?? null;
                renderStageList();
                renderEditor();
            },
        }, "削除"),
    );
    editorEl.appendChild(header);

    // notes
    editorEl.appendChild(el("label", {},
        el("span", { class: "hint", text: "Stage notes" }),
        el("input", {
            type: "text",
            value: stage.notes ?? "",
            style: "width: 100%; margin-top: 4px;",
            placeholder: "(optional)",
            onInput: (e) => {
                stage.notes = e.target.value || undefined;
                scheduleSave(stage);
            },
        })
    ));

    // fields head
    const fieldsHead = el("div", { class: "fields-head" },
        el("h3", { text: "Fields (left → right)" }),
        el("button", {
            onClick: async () => {
                const f = await fetchJson("POST", "/new/field", {
                    id: `f_${(stage.fields.length + 1).toString().padStart(2, "0")}`,
                    category: "grass",
                });
                stage.fields.push(f);
                scheduleSave(stage);
                renderEditor();
                renderStageList();
            },
        }, "＋ Field"),
    );
    editorEl.appendChild(fieldsHead);

    // field list
    const listEl = el("ul", { class: "field-list" });
    const tpl = document.getElementById("tpl-field-row");
    stage.fields.forEach((field, idx) => {
        const li = tpl.content.firstElementChild.cloneNode(true);
        li.dataset.category = field.category;
        if (idx === state.selectedFieldIndex) li.classList.add("selected");
        li.querySelector(".field-index").textContent = `#${idx + 1}`;
        const selEl = li.querySelector(".field-category");
        selEl.value = field.category;
        selEl.addEventListener("change", () => {
            field.category = selEl.value;
            li.dataset.category = field.category;
            scheduleSave(stage);
            renderSequenceBar(stage);
            renderPreview(field);
        });
        const noteEl = li.querySelector(".field-note");
        noteEl.value = field.note ?? "";
        noteEl.addEventListener("input", () => {
            field.note = noteEl.value || undefined;
            scheduleSave(stage);
        });
        li.querySelector(".field-up").addEventListener("click", (e) => {
            e.stopPropagation();
            if (idx === 0) return;
            [stage.fields[idx - 1], stage.fields[idx]] = [stage.fields[idx], stage.fields[idx - 1]];
            scheduleSave(stage);
            state.selectedFieldIndex = idx - 1;
            renderEditor();
        });
        li.querySelector(".field-down").addEventListener("click", (e) => {
            e.stopPropagation();
            if (idx >= stage.fields.length - 1) return;
            [stage.fields[idx + 1], stage.fields[idx]] = [stage.fields[idx], stage.fields[idx + 1]];
            scheduleSave(stage);
            state.selectedFieldIndex = idx + 1;
            renderEditor();
        });
        li.querySelector(".field-remove").addEventListener("click", (e) => {
            e.stopPropagation();
            stage.fields.splice(idx, 1);
            scheduleSave(stage);
            if (state.selectedFieldIndex === idx) state.selectedFieldIndex = null;
            renderEditor();
            renderStageList();
        });
        li.addEventListener("click", () => {
            state.selectedFieldIndex = idx;
            renderEditor();
        });
        listEl.appendChild(li);
    });
    if (stage.fields.length === 0) {
        listEl.appendChild(el("li", { class: "hint" }, "まだ Field がありません。＋ Field で追加してください。"));
    }
    editorEl.appendChild(listEl);

    renderSequenceBar(stage);
    renderPreview(state.selectedFieldIndex != null ? stage.fields[state.selectedFieldIndex] : null);
}

function renderSequenceBar(stage) {
    const existing = editorEl.querySelector(".stage-sequence-preview");
    if (existing) existing.remove();
    const bar = el("div", { class: "stage-sequence-preview" });
    if (stage.fields.length === 0) return;
    stage.fields.forEach((f, idx) => {
        const c = el("div", { class: `cell ${f.category}` },
            el("span", { class: "idx", text: String(idx + 1) })
        );
        c.title = `${CATEGORY_META[f.category].label}${f.note ? " — " + f.note : ""}`;
        bar.appendChild(c);
    });
    editorEl.appendChild(bar);
}

// ─── preview (right pane) ────────────────────────────────────

function renderPreview(field) {
    previewGridEl.innerHTML = "";
    if (!field || !state.meta) {
        previewHintEl.textContent = "Field を選ぶと、その category の全パターンが表示されます。ゲーム実行時は seed から自動選択されます。";
        return;
    }
    const stage = currentStage();
    const patterns = state.meta.patterns[field.category] ?? [];
    const pickIdx  = deterministicPick(field.category, stage?.id ?? "", field.id ?? "");
    previewHintEl.textContent =
        `${CATEGORY_META[field.category].label} (category=${field.category}) — ` +
        `${patterns.length} pattern${patterns.length === 1 ? "" : "s"}. ` +
        `ランタイムでは seed から 1 つ選ばれます (現在のシードだと #${pickIdx + 1}).`;
    patterns.forEach((fname, i) => {
        const img = el("img", { src: "./patterns/" + fname, alt: fname });
        const item = el("div", { class: "preview-item" + (i === pickIdx ? " highlight" : "") },
            img,
            el("div", { class: "caption", text: fname })
        );
        previewGridEl.appendChild(item);
    });
}

/** Mirror of schema.pickPattern (kept in sync by copying the formula). */
function deterministicPick(category, stageId, fieldId, seed = 0) {
    const patterns = state.meta?.patterns[category] ?? [];
    if (patterns.length === 0) return 0;
    let h = (2166136261 ^ seed) >>> 0;
    const mix = (s) => {
        for (let i = 0; i < s.length; ++i) {
            h ^= s.charCodeAt(i);
            h = Math.imul(h, 16777619) >>> 0;
        }
    };
    mix(stageId); mix("|"); mix(fieldId); mix("|"); mix(category);
    return h % patterns.length;
}

// ─── create buttons ──────────────────────────────────────────

stageAddBtn.addEventListener("click", async () => {
    try {
        const skeleton = await fetchJson("POST", "/new/stage", {});
        await fetchJson("PUT", `/stages/${encodeURIComponent(skeleton.id)}`, skeleton);
        state.stages.push(skeleton);
        state.selectedStageId = skeleton.id;
        state.selectedFieldIndex = null;
        renderStageList();
        renderEditor();
        setStatus("new stage created", "ok");
    } catch (err) {
        setStatus("create failed: " + err.message, "err");
    }
});

// ─── WS live reload ──────────────────────────────────────────

function connectWs() {
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    const ws = new WebSocket(`${proto}//${location.host}/terrain/ws`);
    ws.addEventListener("message", (ev) => {
        try {
            const msg = JSON.parse(ev.data);
            if (msg.op === "reload") {
                // Refetch in the background so external edits propagate.
                loadAll();
            }
        } catch { /* ignore */ }
    });
    ws.addEventListener("close", () => {
        setTimeout(connectWs, 1500);
    });
}

// ─── boot ────────────────────────────────────────────────────

loadAll();
connectWs();
