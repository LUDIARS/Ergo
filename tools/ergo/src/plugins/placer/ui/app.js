// Level Placer — block-based level designer front-end (v2).
//
// Tabs: Blocks / Stages / Enemy / SkillBlock.
// Cell placements are typed: Enemy / SkillBlock / SkillBox / Special.
// All state mutations round-trip through the server — clicking "保存"
// is deliberately absent: every change PUTs immediately so the JSON on
// disk always reflects the UI.

const state = {
    meta: null,
    blocks: [],
    stages: [],
    enemies: [],
    skillBlocks: [],
    /** Skill catalog fetched from /api/skills. */
    skillCatalog: [],
    mode: "blocks",
    selectedBlockId: null,
    selectedStageId: null,
    selectedEnemyId: null,
    selectedSbId:    null,
    /** Which Z-layer of the current SkillBlock is being edited. */
    selectedSbZ:     0,
    activeCell: null,
    // Staged "new object" being composed in the dialog.
    newObjDraft: null,
};

const $  = (sel) => document.querySelector(sel);
const $$ = (sel) => Array.from(document.querySelectorAll(sel));

async function api(method, path, body) {
    const res = await fetch(path, {
        method,
        headers: body ? { "Content-Type": "application/json" } : undefined,
        body:    body ? JSON.stringify(body) : undefined,
    });
    const text = await res.text();
    const data = text ? JSON.parse(text) : null;
    if (!res.ok) throw new Error((data && data.error) || `HTTP ${res.status}`);
    return data;
}

function setStatus(text, cls = "") {
    const el = $("#status");
    el.textContent = text;
    el.className   = "status" + (cls ? " " + cls : "");
}

function categoryLabel(cat) {
    return ({ attack: "攻撃", ranged: "遠隔", buff: "強化", special: "特殊" }[cat]) ?? cat;
}
function lookupSkill(id) {
    return state.skillCatalog.find((s) => s.id === id) ?? null;
}
function skillOptionLabel(s) {
    return `[${categoryLabel(s.category)}] ${s.name}  (${s.id})`;
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;",
    })[c]);
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------

async function boot() {
    setStatus("loading…");
    try {
        const [meta, store, skills] = await Promise.all([
            api("GET", "./api/meta"),
            api("GET", "./api/store"),
            api("GET", "./api/skills"),
        ]);
        state.meta         = meta;
        state.blocks       = store.blocks      || [];
        state.stages       = store.stages      || [];
        state.enemies      = store.enemies     || [];
        state.skillBlocks  = store.skillBlocks || [];
        state.skillCatalog = skills?.skills    || [];
        $("#store-path").textContent = meta.storePath;
        if (state.blocks[0])      state.selectedBlockId  = state.blocks[0].id;
        if (state.stages[0])      state.selectedStageId  = state.stages[0].id;
        if (state.enemies[0])     state.selectedEnemyId  = state.enemies[0].id;
        if (state.skillBlocks[0]) state.selectedSbId     = state.skillBlocks[0].id;
        render();
        setStatus(
            `${state.blocks.length} blocks / ${state.stages.length} stages / ${state.enemies.length} enemies / ${state.skillBlocks.length} skill-blocks`,
            "ok",
        );
    } catch (err) {
        setStatus(err.message, "err");
    }
}

// ---------------------------------------------------------------------------
// Rendering root
// ---------------------------------------------------------------------------

function render() {
    for (const m of ["blocks", "stages", "enemies", "skill-blocks"]) {
        $("#view-" + m).classList.toggle("hidden", state.mode !== m);
    }
    $$(".tab").forEach((t) => t.classList.toggle("active", t.dataset.mode === state.mode));

    if (state.mode === "blocks")            { renderBlockList();  renderBlockEditor();  }
    else if (state.mode === "stages")       { renderStageList();  renderStageEditor();  }
    else if (state.mode === "enemies")      { renderEnemyList();  renderEnemyEditor();  }
    else if (state.mode === "skill-blocks") { renderSbList();     renderSbEditor();     }
}

// ---------------------------------------------------------------------------
// Helpers for look-ups
// ---------------------------------------------------------------------------

function findEnemy(id)      { return state.enemies.find((e) => e.id === id)     ?? null; }
function findSkillBlock(id) { return state.skillBlocks.find((s) => s.id === id) ?? null; }

function placedObjectDisplay(o) {
    switch (o.type) {
        case "Enemy": {
            const e = findEnemy(o.enemyId);
            return e ? `${e.name} (Lv${e.level})` : `Enemy ?`;
        }
        case "SkillBlock": {
            const s = findSkillBlock(o.skillBlockId);
            return s ? s.name : `SkillBlock ?`;
        }
        case "SkillBox": {
            const lv = o.level;
            let lbl = "box";
            if (!lv || lv.kind === "random") lbl = "box/rand";
            else if (lv.kind === "random_in_set") lbl = `box/${(lv.levels ?? []).join(",") || "rand"}`;
            else if (lv.kind === "fixed") lbl = `box/L${lv.level}`;
            const ids   = o.skills ?? [];
            if (ids.length === 0) return `${lbl} × any`;
            // Show up to 2 resolved skill names then "+N more".
            const names = ids.slice(0, 2).map((id) => {
                const meta = lookupSkill(id);
                return meta ? meta.name : id;
            });
            const suffix = ids.length > 2 ? ` +${ids.length - 2}` : "";
            return `${lbl} ${names.join("/")}${suffix}`;
        }
        case "Special":
            return o.label ?? "special";
    }
    return o.label ?? "?";
}

function placedObjectTypeClass(t) {
    switch (t) {
        case "Enemy":      return "type-enemy";
        case "SkillBlock": return "type-skill";
        case "SkillBox":   return "type-item";
        case "Special":    return "type-marker";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Blocks mode
// ---------------------------------------------------------------------------

function renderBlockList() {
    const ul = $("#block-list");
    ul.innerHTML = "";
    for (const b of state.blocks) {
        const li = document.createElement("li");
        if (b.id === state.selectedBlockId) li.classList.add("sel");
        const cellCount = countCells(b);
        li.innerHTML = `
            <span class="nm">${escapeHtml(b.name || b.id)}</span>
            <span class="sub">${escapeHtml(b.id)} · ${b.rows}×10 · ${cellCount} cells</span>
        `;
        li.addEventListener("click", () => { state.selectedBlockId = b.id; render(); });
        ul.appendChild(li);
    }
}

function currentBlock() {
    return state.blocks.find((b) => b.id === state.selectedBlockId) || null;
}

function renderBlockEditor() {
    const root = $("#block-editor");
    root.innerHTML = "";
    const block = currentBlock();
    if (!block) {
        root.innerHTML = "<p class='hint'>Block を選択するか、サイドバーから追加してください。</p>";
        return;
    }

    const meta = document.createElement("div");
    meta.className = "block-meta";

    const idInp = fieldInput("id", block.id, (v) => {
        const trimmed = v.trim();
        if (!trimmed || trimmed === block.id) return;
        if (state.blocks.some((x) => x !== block && x.id === trimmed)) {
            setStatus(`id 重複: ${trimmed}`, "err"); return;
        }
        renameBlock(block, trimmed);
    });
    const nameInp = fieldInput("name", block.name, (v) => { block.name = v; saveBlock(block); });

    const rowsSel = document.createElement("select");
    for (const r of state.meta.allowedRows) {
        const opt = document.createElement("option");
        opt.value = String(r);
        opt.textContent = `${r}×${state.meta.cols}`;
        rowsSel.appendChild(opt);
    }
    rowsSel.value = String(block.rows);
    rowsSel.addEventListener("change", async () => {
        const next = Number(rowsSel.value);
        try {
            const r = await api("POST", `./api/blocks/${encodeURIComponent(block.id)}/resize`, { rows: next });
            const idx = state.blocks.findIndex((b) => b.id === block.id);
            if (idx >= 0) state.blocks[idx] = r.block;
            render();
            setStatus("resized", "ok");
        } catch (err) { setStatus(err.message, "err"); }
    });

    const contentsInp = fieldInput("contents", block.contents || "", (v) => {
        block.contents = v; saveBlock(block);
    });

    const trigKindSel = document.createElement("select");
    for (const k of ["start", "time", "after", "score", "manual"]) {
        const o = document.createElement("option");
        o.value = k; o.textContent = k;
        trigKindSel.appendChild(o);
    }
    trigKindSel.value = block.appears?.kind || "start";
    trigKindSel.addEventListener("change", () => {
        block.appears = { ...block.appears, kind: trigKindSel.value };
        saveBlock(block); renderBlockEditor();
    });

    const trigValInp = fieldInput("value", block.appears?.value ?? "", (v) => {
        block.appears = { ...block.appears, value: v === "" ? undefined : (isFiniteNum(v) ? Number(v) : v) };
        saveBlock(block);
    });
    if (trigKindSel.value === "start" || trigKindSel.value === "manual") {
        trigValInp.input.placeholder = "(不要)";
        trigValInp.input.disabled = true;
    }

    const specialLabel = document.createElement("label");
    specialLabel.className = "special";
    specialLabel.innerHTML = "<span>特殊設定 (TBD, JSON)</span>";
    const specialTa = document.createElement("textarea");
    specialTa.value = block.special ? JSON.stringify(block.special, null, 2) : "";
    specialTa.addEventListener("change", () => {
        const raw = specialTa.value.trim();
        if (!raw) { block.special = undefined; saveBlock(block); return; }
        try { block.special = JSON.parse(raw); saveBlock(block); }
        catch (e) { setStatus("JSON parse error: " + e.message, "err"); }
    });
    specialLabel.appendChild(specialTa);

    meta.appendChild(wrapLabel("id", idInp.input));
    meta.appendChild(wrapLabel("表示名", nameInp.input));
    meta.appendChild(wrapLabel("サイズ (行 × 列)", rowsSel));
    meta.appendChild(wrapLabel("contents (何が出るか)", contentsInp.input));
    meta.appendChild(wrapLabel("いつ出現 (kind)", trigKindSel));
    meta.appendChild(wrapLabel("value (秒 / id / …)", trigValInp.input));
    meta.appendChild(specialLabel);
    root.appendChild(meta);

    // --- grid ---
    const gridWrap = document.createElement("div");
    gridWrap.className = "block-grid-wrap";
    const grid = document.createElement("div");
    grid.className = "block-grid";
    grid.style.gridTemplateRows = `18px repeat(${block.rows}, 54px)`;

    grid.appendChild(cornerLabel());
    for (let c = 0; c < state.meta.cols; ++c) {
        const cl = document.createElement("div");
        cl.className = "col-label"; cl.textContent = `C${c + 1}`;
        grid.appendChild(cl);
    }
    for (let r = 0; r < block.rows; ++r) {
        const rl = document.createElement("div");
        rl.className = "row-label"; rl.textContent = `R${r + 1}`;
        grid.appendChild(rl);
        for (let c = 0; c < state.meta.cols; ++c) {
            grid.appendChild(renderCell(block, r, c));
        }
    }
    gridWrap.appendChild(grid);
    root.appendChild(gridWrap);

    // --- actions ---
    const actions = document.createElement("div");
    actions.className = "block-actions";
    const delBtn = document.createElement("button");
    delBtn.className = "danger"; delBtn.textContent = "この block を削除";
    delBtn.addEventListener("click", async () => {
        if (!confirm(`block "${block.id}" を削除しますか?`)) return;
        try {
            await api("DELETE", `./api/blocks/${encodeURIComponent(block.id)}`);
            state.blocks = state.blocks.filter((b) => b.id !== block.id);
            for (const s of state.stages) s.blocks = s.blocks.filter((bid) => bid !== block.id);
            state.selectedBlockId = state.blocks[0]?.id ?? null;
            render(); setStatus("deleted", "ok");
        } catch (err) { setStatus(err.message, "err"); }
    });
    actions.appendChild(delBtn);
    root.appendChild(actions);
}

function renderCell(block, r, c) {
    const cell = block.grid?.[r]?.[c] || { objects: [] };
    const el = document.createElement("div");
    el.className = "cell";
    if (cell.objects.length === 0) {
        el.classList.add("empty");
        el.innerHTML = "<span class='hint'>·</span>";
    } else {
        if (cell.objects.length > 1) {
            const flag = document.createElement("span");
            flag.className = "combo-flag";
            flag.textContent = `combo ×${cell.objects.length}`;
            el.appendChild(flag);
        }
        for (const o of cell.objects.slice(0, 3)) {
            const span = document.createElement("span");
            span.className = "obj " + placedObjectTypeClass(o.type);
            span.textContent = placedObjectDisplay(o);
            span.title = JSON.stringify(o);
            el.appendChild(span);
        }
        if (cell.objects.length > 3) {
            const more = document.createElement("span");
            more.className = "obj";
            more.textContent = `+${cell.objects.length - 3}`;
            el.appendChild(more);
        }
    }
    el.addEventListener("click", () => openCellDialog(block, r, c));
    return el;
}

function countCells(block) {
    let n = 0;
    for (const row of (block.grid || [])) for (const cell of row) if (cell.objects.length > 0) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// Cell dialog — typed object editor
// ---------------------------------------------------------------------------

function openCellDialog(block, r, c) {
    state.activeCell = { blockId: block.id, row: r, col: c };
    state.newObjDraft = makeDraft("Enemy");
    $("#cell-coord").textContent = `${block.id}  R${r + 1}, C${c + 1}`;
    $("#new-obj-type").value = "Enemy";
    renderCellDialogList();
    renderNewObjFields();
    $("#cell-dialog").showModal();
}

function renderCellDialogList() {
    const { blockId, row, col } = state.activeCell;
    const block = state.blocks.find((b) => b.id === blockId);
    const cell  = block?.grid?.[row]?.[col] || { objects: [] };
    const ul = $("#cell-objects");
    ul.innerHTML = "";
    if (cell.objects.length === 0) {
        const li = document.createElement("li");
        li.innerHTML = "<em style='color:var(--fg-dim)'>(空)</em>";
        ul.appendChild(li);
        return;
    }
    cell.objects.forEach((o, idx) => {
        const li = document.createElement("li");
        li.innerHTML = `
            <span class="o-type">${escapeHtml(o.type)}</span>
            <span class="o-label">${escapeHtml(placedObjectDisplay(o))}</span>
            <button type="button" class="ghost" data-idx="${idx}">✕</button>
        `;
        li.querySelector("button").addEventListener("click", async () => {
            cell.objects.splice(idx, 1);
            await saveBlock(block);
            renderCellDialogList(); renderBlockEditor();
        });
        ul.appendChild(li);
    });
}

function makeDraft(type) {
    switch (type) {
        case "Enemy":      return { type: "Enemy", enemyId: state.enemies[0]?.id ?? "" };
        case "SkillBlock": return { type: "SkillBlock", skillBlockId: state.skillBlocks[0]?.id ?? "" };
        case "SkillBox":   return { type: "SkillBox", skills: [], level: { kind: "random" } };
        case "Special":    return { type: "Special", payload: {}, label: "special" };
    }
    return null;
}

function renderNewObjFields() {
    const root = $("#new-obj-fields");
    root.innerHTML = "";
    const draft = state.newObjDraft;
    if (!draft) return;

    switch (draft.type) {
        case "Enemy": {
            const sel = buildReferenceSelect(state.enemies, draft.enemyId, (v) => { draft.enemyId = v; });
            if (state.enemies.length === 0) {
                sel.disabled = true;
                root.appendChild(note("Enemy タブで先に敵を作成してください。"));
            }
            root.appendChild(wrapLabel("Enemy", sel));
            break;
        }
        case "SkillBlock": {
            const sel = buildReferenceSelect(state.skillBlocks, draft.skillBlockId, (v) => { draft.skillBlockId = v; });
            if (state.skillBlocks.length === 0) {
                sel.disabled = true;
                root.appendChild(note("SkillBlock タブで先に作成してください。"));
            }
            root.appendChild(wrapLabel("SkillBlock", sel));
            break;
        }
        case "SkillBox": {
            // Skills multi-select (chips).
            const chipsWrap = renderSkillPicker(draft.skills, (next) => { draft.skills = next; });
            root.appendChild(wrapLabel("出現スキル (複数可)", chipsWrap));

            // Level mode.
            const lvKindSel = document.createElement("select");
            for (const k of ["random", "random_in_set", "fixed"]) {
                const o = document.createElement("option"); o.value = k;
                o.textContent = {
                    random: "ランダム (1〜5 均等)",
                    random_in_set: "選択ランダム (1〜5 から複数選択)",
                    fixed: "固定 (1〜5)",
                }[k];
                lvKindSel.appendChild(o);
            }
            lvKindSel.value = draft.level?.kind ?? "random";
            lvKindSel.addEventListener("change", () => {
                const k = lvKindSel.value;
                if (k === "random")         draft.level = { kind: "random" };
                if (k === "random_in_set")  draft.level = { kind: "random_in_set", levels: [1, 2, 3, 4, 5] };
                if (k === "fixed")          draft.level = { kind: "fixed", level: 1 };
                renderNewObjFields();
            });
            root.appendChild(wrapLabel("Level", lvKindSel));

            if (draft.level?.kind === "random_in_set") {
                const levelChipsWrap = document.createElement("div");
                levelChipsWrap.className = "chips";
                for (const n of [1, 2, 3, 4, 5]) {
                    const btn = document.createElement("button");
                    btn.type = "button";
                    const picked = (draft.level.levels ?? []).includes(n);
                    btn.className = "chip";
                    btn.style.background = picked ? "#3a5474" : "transparent";
                    btn.style.border = "1px solid var(--border)";
                    btn.textContent = `Lv${n}`;
                    btn.addEventListener("click", () => {
                        const set = new Set(draft.level.levels ?? []);
                        if (set.has(n)) set.delete(n); else set.add(n);
                        draft.level.levels = [...set].sort((a, b) => a - b);
                        renderNewObjFields();
                    });
                    levelChipsWrap.appendChild(btn);
                }
                root.appendChild(wrapLabel("対象レベル", levelChipsWrap));
            }
            if (draft.level?.kind === "fixed") {
                const lvInp = document.createElement("input");
                lvInp.type = "number"; lvInp.min = "1"; lvInp.max = "5";
                lvInp.value = String(draft.level.level ?? 1);
                lvInp.addEventListener("input", () => {
                    const n = Math.max(1, Math.min(5, Math.round(Number(lvInp.value) || 1)));
                    draft.level = { kind: "fixed", level: n };
                });
                root.appendChild(wrapLabel("レベル (1〜5)", lvInp));
            }
            break;
        }
        case "Special": {
            const ta = document.createElement("textarea");
            ta.rows = 4;
            ta.style.width = "100%";
            ta.style.background = "var(--bg)";
            ta.style.color = "var(--fg)";
            ta.style.border = "1px solid var(--border)";
            ta.style.borderRadius = "4px";
            ta.style.padding = "6px";
            ta.placeholder = `{"spawn": "boss", ...}`;
            ta.value = JSON.stringify(draft.payload ?? {}, null, 2);
            ta.addEventListener("change", () => {
                try { draft.payload = JSON.parse(ta.value); setStatus("", "ok"); }
                catch (e) { setStatus("JSON parse error: " + e.message, "err"); }
            });
            root.appendChild(wrapLabel("payload (TBD, JSON)", ta));

            const lbl = document.createElement("input");
            lbl.type = "text"; lbl.value = draft.label ?? "";
            lbl.addEventListener("input", () => { draft.label = lbl.value || undefined; });
            root.appendChild(wrapLabel("ラベル (セル表示用)", lbl));
            break;
        }
    }
}

function buildReferenceSelect(list, currentId, onChange) {
    const sel = document.createElement("select");
    if (list.length === 0) {
        const opt = document.createElement("option");
        opt.value = ""; opt.textContent = "(なし)";
        sel.appendChild(opt);
    } else {
        for (const item of list) {
            const opt = document.createElement("option");
            opt.value = item.id;
            const suffix = (item.level !== undefined) ? ` Lv${item.level}` : "";
            opt.textContent = `${item.name || item.id}${suffix}  (${item.id})`;
            sel.appendChild(opt);
        }
    }
    sel.value = currentId || list[0]?.id || "";
    onChange(sel.value);
    sel.addEventListener("change", () => onChange(sel.value));
    return sel;
}

/**
 * Catalog-driven skill picker.
 *
 * Renders a `<select>` that adds a skill on change, plus a row of chips
 * for the currently selected IDs (duplicates allowed — 攻撃系の
 * 「同じブロック集めるとレベルアップ」を表現).
 */
function renderSkillPicker(values, onChange) {
    const wrap = document.createElement("div");
    wrap.className = "skill-picker";

    // --- Chips ---
    const chipsRow = document.createElement("div");
    chipsRow.className = "chips";
    if (values.length === 0) {
        const empty = document.createElement("span");
        empty.className = "hint";
        empty.style.padding = "4px";
        empty.textContent = "(skill 未選択)";
        chipsRow.appendChild(empty);
    } else {
        values.forEach((id, idx) => {
            const meta = lookupSkill(id);
            const chip = document.createElement("span");
            chip.className = "chip";
            const label = meta ? `[${categoryLabel(meta.category)}] ${meta.name}` : id;
            chip.innerHTML = `${escapeHtml(label)}<button type="button" aria-label="remove">×</button>`;
            chip.title = meta ? `${meta.description}  (id=${meta.id})` : id;
            chip.querySelector("button").addEventListener("click", () => {
                const next = [...values];
                next.splice(idx, 1);
                onChange(next);
                const replaced = renderSkillPicker(next, onChange);
                wrap.replaceWith(replaced);
            });
            chipsRow.appendChild(chip);
        });
    }
    wrap.appendChild(chipsRow);

    // --- Add row: select categorised options + "add" button ---
    const addRow = document.createElement("div");
    addRow.style.display = "flex";
    addRow.style.gap = "6px";
    addRow.style.marginTop = "4px";

    const sel = document.createElement("select");
    sel.style.flex = "1";

    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = "skill を選択して追加";
    sel.appendChild(placeholder);

    for (const cat of ["attack", "ranged", "buff", "special"]) {
        const group = document.createElement("optgroup");
        group.label = categoryLabel(cat);
        for (const s of state.skillCatalog.filter((x) => x.category === cat)) {
            const opt = document.createElement("option");
            opt.value = s.id;
            opt.textContent = skillOptionLabel(s);
            group.appendChild(opt);
        }
        if (group.childNodes.length > 0) sel.appendChild(group);
    }

    const addBtn = document.createElement("button");
    addBtn.type = "button";
    addBtn.textContent = "＋";
    addBtn.addEventListener("click", () => {
        const id = sel.value;
        if (!id) return;
        const next = [...values, id];   // duplicates allowed
        onChange(next);
        const replaced = renderSkillPicker(next, onChange);
        wrap.replaceWith(replaced);
    });

    addRow.appendChild(sel);
    addRow.appendChild(addBtn);
    wrap.appendChild(addRow);

    if (state.skillCatalog.length === 0) {
        const note = document.createElement("p");
        note.className = "hint";
        note.textContent = "skill catalog が空です (サーバの /api/skills を確認)";
        wrap.appendChild(note);
    }

    return wrap;
}


function note(text) {
    const p = document.createElement("p");
    p.className = "hint";
    p.textContent = text;
    return p;
}

async function addNewObjToActiveCell() {
    const { blockId, row, col } = state.activeCell;
    const block = state.blocks.find((b) => b.id === blockId);
    if (!block) return;
    const draft = state.newObjDraft;
    if (!draft) return;

    // Validation per type.
    if (draft.type === "Enemy" && !draft.enemyId)           { setStatus("Enemy を選択してください", "warn"); return; }
    if (draft.type === "SkillBlock" && !draft.skillBlockId) { setStatus("SkillBlock を選択してください", "warn"); return; }

    const obj = { ...draft };
    block.grid[row][col] ||= { objects: [] };
    block.grid[row][col].objects.push(obj);
    await saveBlock(block);

    state.newObjDraft = makeDraft(obj.type);
    renderNewObjFields();
    renderCellDialogList();
    renderBlockEditor();
}

// ---------------------------------------------------------------------------
// Stages mode
// ---------------------------------------------------------------------------

function renderStageList() {
    const ul = $("#stage-list");
    ul.innerHTML = "";
    for (const s of state.stages) {
        const li = document.createElement("li");
        if (s.id === state.selectedStageId) li.classList.add("sel");
        li.innerHTML = `
            <span class="nm">${escapeHtml(s.name || s.id)}</span>
            <span class="sub">${escapeHtml(s.id)} · ${s.blocks.length} blocks</span>
        `;
        li.addEventListener("click", () => { state.selectedStageId = s.id; render(); });
        ul.appendChild(li);
    }
}

function currentStage() {
    return state.stages.find((s) => s.id === state.selectedStageId) || null;
}

function renderStageEditor() {
    const root = $("#stage-editor");
    root.innerHTML = "";
    const stage = currentStage();
    if (!stage) {
        root.innerHTML = "<p class='hint'>Stage を選択するか、サイドバーから追加してください。</p>";
        return;
    }

    const meta = document.createElement("div");
    meta.className = "stage-meta";

    const idInp = fieldInput("id", stage.id, (v) => {
        const trimmed = v.trim();
        if (!trimmed || trimmed === stage.id) return;
        if (state.stages.some((x) => x !== stage && x.id === trimmed)) {
            setStatus(`id 重複: ${trimmed}`, "err"); return;
        }
        renameStage(stage, trimmed);
    });
    const nameInp = fieldInput("name", stage.name || "", (v) => { stage.name = v; saveStage(stage); });
    meta.appendChild(wrapLabel("id", idInp.input));
    meta.appendChild(wrapLabel("表示名", nameInp.input));

    const appendBtn = document.createElement("button");
    appendBtn.className = "primary"; appendBtn.textContent = "＋ block";
    appendBtn.addEventListener("click", () => {
        if (state.blocks.length === 0) { setStatus("先に block を作成してください", "warn"); return; }
        stage.blocks.push(state.blocks[0].id); saveStage(stage); render();
    });
    meta.appendChild(appendBtn);
    root.appendChild(meta);

    const seq = document.createElement("div");
    seq.className = "stage-sequence";
    stage.blocks.forEach((bid, idx) => {
        const row = document.createElement("div");
        row.className = "stage-step";
        const i = document.createElement("span"); i.className = "step-idx"; i.textContent = `#${idx + 1}`;
        const sel = document.createElement("select");
        for (const b of state.blocks) {
            const opt = document.createElement("option");
            opt.value = b.id; opt.textContent = `${b.name || b.id}  (${b.rows}×10)`;
            sel.appendChild(opt);
        }
        sel.value = bid;
        sel.addEventListener("change", () => { stage.blocks[idx] = sel.value; saveStage(stage); });

        const nav = document.createElement("span"); nav.className = "nav";
        const up = document.createElement("button"); up.className = "ghost"; up.textContent = "↑";
        up.disabled = idx === 0;
        up.addEventListener("click", () => { swap(stage.blocks, idx, idx - 1); saveStage(stage); render(); });
        const dn = document.createElement("button"); dn.className = "ghost"; dn.textContent = "↓";
        dn.disabled = idx === stage.blocks.length - 1;
        dn.addEventListener("click", () => { swap(stage.blocks, idx, idx + 1); saveStage(stage); render(); });
        const rm = document.createElement("button"); rm.className = "danger"; rm.textContent = "削除";
        rm.addEventListener("click", () => { stage.blocks.splice(idx, 1); saveStage(stage); render(); });
        nav.append(up, dn, rm);
        row.append(i, sel, nav);
        seq.appendChild(row);
    });
    root.appendChild(seq);

    const actions = document.createElement("div");
    actions.className = "block-actions";
    const delBtn = document.createElement("button");
    delBtn.className = "danger"; delBtn.textContent = "この stage を削除";
    delBtn.addEventListener("click", async () => {
        if (!confirm(`stage "${stage.id}" を削除しますか?`)) return;
        try {
            await api("DELETE", `./api/stages/${encodeURIComponent(stage.id)}`);
            state.stages = state.stages.filter((s) => s.id !== stage.id);
            state.selectedStageId = state.stages[0]?.id ?? null;
            render();
        } catch (err) { setStatus(err.message, "err"); }
    });
    actions.appendChild(delBtn);
    root.appendChild(actions);
}

// ---------------------------------------------------------------------------
// Enemies mode
// ---------------------------------------------------------------------------

function renderEnemyList() {
    const ul = $("#enemy-list");
    ul.innerHTML = "";
    for (const e of state.enemies) {
        const li = document.createElement("li");
        if (e.id === state.selectedEnemyId) li.classList.add("sel");
        const sb = findSkillBlock(e.skillBlockId);
        li.innerHTML = `
            <span class="nm">${escapeHtml(e.name || e.id)}</span>
            <span class="sub">${escapeHtml(e.id)} · Lv${e.level} · ${sb ? escapeHtml(sb.name) : "(no skill-block)"}</span>
        `;
        li.addEventListener("click", () => { state.selectedEnemyId = e.id; render(); });
        ul.appendChild(li);
    }
}

function currentEnemy() {
    return state.enemies.find((e) => e.id === state.selectedEnemyId) || null;
}

function renderEnemyEditor() {
    const root = $("#enemy-editor");
    root.innerHTML = "";
    const enemy = currentEnemy();
    if (!enemy) {
        root.innerHTML = "<p class='hint'>Enemy を選択するか、サイドバーから追加してください。</p>";
        return;
    }
    const grid = document.createElement("div");
    grid.className = "sheet-grid";

    const idInp = fieldInput("id", enemy.id, (v) => {
        const trimmed = v.trim();
        if (!trimmed || trimmed === enemy.id) return;
        if (state.enemies.some((x) => x !== enemy && x.id === trimmed)) {
            setStatus(`id 重複: ${trimmed}`, "err"); return;
        }
        renameEnemy(enemy, trimmed);
    });

    const nameInp = fieldInput("name", enemy.name || "", (v) => { enemy.name = v; saveEnemy(enemy); });

    const sbSel = document.createElement("select");
    const optEmpty = document.createElement("option"); optEmpty.value = ""; optEmpty.textContent = "(未選択)";
    sbSel.appendChild(optEmpty);
    for (const sb of state.skillBlocks) {
        const o = document.createElement("option");
        o.value = sb.id; o.textContent = `${sb.name || sb.id}`;
        sbSel.appendChild(o);
    }
    sbSel.value = enemy.skillBlockId || "";
    sbSel.addEventListener("change", () => { enemy.skillBlockId = sbSel.value; saveEnemy(enemy); renderEnemyList(); });

    const levelInp = document.createElement("input");
    levelInp.type = "number"; levelInp.min = "1"; levelInp.max = "5";
    levelInp.value = String(enemy.level ?? 1);
    levelInp.addEventListener("input", () => {
        const n = Math.max(1, Math.min(5, Math.round(Number(levelInp.value) || 1)));
        enemy.level = n; saveEnemy(enemy); renderEnemyList();
    });

    const notesTa = document.createElement("textarea");
    notesTa.value = enemy.notes ?? "";
    notesTa.addEventListener("change", () => {
        enemy.notes = notesTa.value || undefined; saveEnemy(enemy);
    });

    grid.appendChild(wrapLabel("id", idInp.input));
    grid.appendChild(wrapLabel("表示名", nameInp.input));
    grid.appendChild(wrapLabel("所持 SkillBlock", sbSel));
    grid.appendChild(wrapLabel("level (1〜5)", levelInp));
    const notesLbl = wrapLabel("notes", notesTa); notesLbl.classList.add("span-2");
    grid.appendChild(notesLbl);
    root.appendChild(grid);

    const actions = document.createElement("div");
    actions.className = "block-actions";
    const delBtn = document.createElement("button");
    delBtn.className = "danger"; delBtn.textContent = "この enemy を削除";
    delBtn.addEventListener("click", async () => {
        if (!confirm(`enemy "${enemy.id}" を削除しますか?`)) return;
        try {
            await api("DELETE", `./api/enemies/${encodeURIComponent(enemy.id)}`);
            state.enemies = state.enemies.filter((e) => e.id !== enemy.id);
            state.selectedEnemyId = state.enemies[0]?.id ?? null;
            render(); setStatus("deleted", "ok");
        } catch (err) { setStatus(err.message, "err"); }
    });
    actions.appendChild(delBtn);
    root.appendChild(actions);
}

// ---------------------------------------------------------------------------
// SkillBlocks mode
// ---------------------------------------------------------------------------

function renderSbList() {
    const ul = $("#sb-list");
    ul.innerHTML = "";
    for (const s of state.skillBlocks) {
        const li = document.createElement("li");
        if (s.id === state.selectedSbId) li.classList.add("sel");
        const filled = countFilledVoxels(s.shape);
        const size   = s.size ?? 3;
        li.innerHTML = `
            <span class="nm">${escapeHtml(s.name || s.id)}</span>
            <span class="sub">${escapeHtml(s.id)} · ${size}³ · ${filled} voxels · ${s.skills.length} skills · ${formatInterval(s.interval)}</span>
        `;
        li.addEventListener("click", () => {
            state.selectedSbId = s.id;
            state.selectedSbZ  = 0;
            render();
        });
        ul.appendChild(li);
    }
}

function currentSb() {
    return state.skillBlocks.find((s) => s.id === state.selectedSbId) || null;
}

function countFilledVoxels(shape) {
    if (!Array.isArray(shape)) return 0;
    let n = 0;
    for (const layer of shape)
        for (const row of layer)
            for (const v of row) if (v) ++n;
    return n;
}

function formatInterval(iv) {
    const n = Number(iv);
    if (!Number.isFinite(n)) return "CD?";
    if (n === 0) return "CD 0s";
    return `CD ${n}s`;
}

function renderSbEditor() {
    const root = $("#sb-editor");
    root.innerHTML = "";
    const sb = currentSb();
    if (!sb) {
        root.innerHTML = "<p class='hint'>SkillBlock を選択するか、サイドバーから追加してください。</p>";
        return;
    }

    const grid = document.createElement("div");
    grid.className = "sheet-grid";

    const idInp = fieldInput("id", sb.id, (v) => {
        const trimmed = v.trim();
        if (!trimmed || trimmed === sb.id) return;
        if (state.skillBlocks.some((x) => x !== sb && x.id === trimmed)) {
            setStatus(`id 重複: ${trimmed}`, "err"); return;
        }
        renameSb(sb, trimmed);
    });
    const nameInp = fieldInput("name", sb.name || "", (v) => { sb.name = v; saveSb(sb); });

    // size select (server-side resize preserves overlapping voxels)
    const sizes = state.meta?.skillBlockSizes ?? [3, 5, 7, 9];
    const sizeSel = document.createElement("select");
    for (const n of sizes) {
        const o = document.createElement("option");
        o.value = String(n); o.textContent = `${n}×${n}×${n}`;
        sizeSel.appendChild(o);
    }
    sizeSel.value = String(sb.size ?? 3);
    sizeSel.addEventListener("change", async () => {
        const nextSize = Number(sizeSel.value);
        try {
            const r = await api("POST", `./api/skill-blocks/${encodeURIComponent(sb.id)}/resize`, { size: nextSize });
            const idx = state.skillBlocks.findIndex((x) => x.id === sb.id);
            if (idx >= 0) state.skillBlocks[idx] = r.skillBlock;
            if (state.selectedSbZ >= nextSize) state.selectedSbZ = nextSize - 1;
            render();
            setStatus("resized", "ok");
        } catch (err) { setStatus(err.message, "err"); }
    });

    // interval (seconds)
    const ivInp = document.createElement("input");
    ivInp.type = "number"; ivInp.min = "0"; ivInp.step = "0.1";
    ivInp.value = String(sb.interval ?? 1.0);
    ivInp.addEventListener("input", () => {
        const n = Number(ivInp.value);
        sb.interval = Number.isFinite(n) && n >= 0 ? n : 0;
        saveSb(sb); renderSbList();
    });

    const chipsWrap = renderSkillPicker(sb.skills, (next) => { sb.skills = next; saveSb(sb); renderSbList(); });

    const notesTa = document.createElement("textarea");
    notesTa.value = sb.notes ?? "";
    notesTa.addEventListener("change", () => {
        sb.notes = notesTa.value || undefined; saveSb(sb);
    });

    grid.appendChild(wrapLabel("id", idInp.input));
    grid.appendChild(wrapLabel("表示名", nameInp.input));
    grid.appendChild(wrapLabel("立方体サイズ (N³)", sizeSel));
    grid.appendChild(wrapLabel("インターバル (秒)", ivInp));
    const skillsLbl = wrapLabel("skills (カタログから multi-select)", chipsWrap); skillsLbl.classList.add("span-2");
    grid.appendChild(skillsLbl);
    const shapeLbl  = wrapLabel("形状 (Z レイヤ切替 + セルクリック)", renderShapeEditor(sb)); shapeLbl.classList.add("span-2");
    grid.appendChild(shapeLbl);
    const notesLbl  = wrapLabel("notes", notesTa); notesLbl.classList.add("span-2");
    grid.appendChild(notesLbl);
    root.appendChild(grid);

    const actions = document.createElement("div");
    actions.className = "block-actions";
    const delBtn = document.createElement("button");
    delBtn.className = "danger"; delBtn.textContent = "この SkillBlock を削除";
    delBtn.addEventListener("click", async () => {
        if (!confirm(`SkillBlock "${sb.id}" を削除しますか? (参照している Enemy の skillBlockId は空になります)`)) return;
        try {
            await api("DELETE", `./api/skill-blocks/${encodeURIComponent(sb.id)}`);
            state.skillBlocks = state.skillBlocks.filter((x) => x.id !== sb.id);
            for (const e of state.enemies) if (e.skillBlockId === sb.id) e.skillBlockId = "";
            state.selectedSbId = state.skillBlocks[0]?.id ?? null;
            render(); setStatus("deleted", "ok");
        } catch (err) { setStatus(err.message, "err"); }
    });
    actions.appendChild(delBtn);
    root.appendChild(actions);
}

/**
 * 3D voxel shape editor.
 *
 * UI strategy: show one Z-layer at a time as an N×N clickable grid.
 * Below, a Z navigator lets you jump between layers or flip through.
 * Filled voxels in neighbouring layers are shown as faint outlines so
 * the editor has some depth feedback without a true 3D view.
 */
function renderShapeEditor(sb) {
    const wrap = document.createElement("div");
    wrap.className = "shape-editor";

    const size = sb.size ?? 3;
    if (!Array.isArray(sb.shape) || sb.shape.length !== size) {
        // Defensive: re-create a zero shape if something was lost.
        sb.shape = Array.from({ length: size }, () =>
            Array.from({ length: size }, () =>
                Array.from({ length: size }, () => 0)));
    }

    state.selectedSbZ = Math.max(0, Math.min(state.selectedSbZ, size - 1));

    // ---- Z navigator ---------------------------------------------
    const nav = document.createElement("div");
    nav.className = "shape-nav";
    const prev = document.createElement("button");
    prev.type = "button"; prev.textContent = "◀";
    prev.addEventListener("click", () => {
        state.selectedSbZ = Math.max(0, state.selectedSbZ - 1);
        renderSbEditor();
    });
    const zLabel = document.createElement("span");
    zLabel.className = "shape-z";
    zLabel.textContent = `Z = ${state.selectedSbZ + 1} / ${size}`;
    const next = document.createElement("button");
    next.type = "button"; next.textContent = "▶";
    next.addEventListener("click", () => {
        state.selectedSbZ = Math.min(size - 1, state.selectedSbZ + 1);
        renderSbEditor();
    });

    const slider = document.createElement("input");
    slider.type = "range"; slider.min = "0"; slider.max = String(size - 1); slider.step = "1";
    slider.value = String(state.selectedSbZ);
    slider.addEventListener("input", () => {
        state.selectedSbZ = Number(slider.value);
        zLabel.textContent = `Z = ${state.selectedSbZ + 1} / ${size}`;
        refreshGrid();
    });
    nav.append(prev, zLabel, next, slider);

    // Bulk actions
    const clearBtn = document.createElement("button");
    clearBtn.type = "button"; clearBtn.textContent = "この層をクリア";
    clearBtn.addEventListener("click", () => {
        const layer = sb.shape[state.selectedSbZ];
        if (!Array.isArray(layer)) return;
        for (let y = 0; y < size; ++y)
            for (let x = 0; x < size; ++x)
                layer[y][x] = 0;
        saveSb(sb); refreshGrid(); renderSbList();
    });
    const fillBtn = document.createElement("button");
    fillBtn.type = "button"; fillBtn.textContent = "この層を全塗り";
    fillBtn.addEventListener("click", () => {
        const layer = sb.shape[state.selectedSbZ];
        if (!Array.isArray(layer)) return;
        for (let y = 0; y < size; ++y)
            for (let x = 0; x < size; ++x)
                layer[y][x] = 1;
        saveSb(sb); refreshGrid(); renderSbList();
    });
    nav.append(clearBtn, fillBtn);

    wrap.appendChild(nav);

    // ---- 2D grid (for the current Z layer) -----------------------
    const gridBox = document.createElement("div");
    gridBox.className = "shape-grid-wrap";
    wrap.appendChild(gridBox);

    function refreshGrid() {
        gridBox.innerHTML = "";
        const tbl = document.createElement("div");
        tbl.className = "shape-grid";
        tbl.style.gridTemplateColumns = `repeat(${size}, 28px)`;
        const layer = sb.shape[state.selectedSbZ] ?? [];
        for (let y = 0; y < size; ++y) {
            const row = layer[y] ?? [];
            for (let x = 0; x < size; ++x) {
                const cell = document.createElement("button");
                cell.type = "button";
                cell.className = "shape-cell";
                const filled = !!row[x];
                if (filled) cell.classList.add("on");
                cell.dataset.x = String(x);
                cell.dataset.y = String(y);
                cell.title = `x=${x}, y=${y}, z=${state.selectedSbZ}`;
                cell.addEventListener("click", () => {
                    const v = sb.shape[state.selectedSbZ][y][x];
                    sb.shape[state.selectedSbZ][y][x] = v ? 0 : 1;
                    cell.classList.toggle("on");
                    saveSb(sb); renderSbList();
                });
                tbl.appendChild(cell);
            }
        }
        gridBox.appendChild(tbl);

        const meta = document.createElement("div");
        meta.className = "shape-meta";
        const thisLayerFilled = (layer || []).reduce(
            (sum, r) => sum + (Array.isArray(r) ? r.reduce((s, v) => s + (v ? 1 : 0), 0) : 0), 0);
        const total = countFilledVoxels(sb.shape);
        meta.textContent = `layer filled: ${thisLayerFilled} / ${size * size}  ·  total: ${total} / ${size ** 3}`;
        gridBox.appendChild(meta);
    }
    refreshGrid();
    return wrap;
}

// ---------------------------------------------------------------------------
// Persist helpers
// ---------------------------------------------------------------------------

let savingBlock = null;
async function saveBlock(block) {
    const prev = savingBlock || Promise.resolve();
    savingBlock = prev.then(() => _saveBlock(block)).catch((e) => setStatus(e.message, "err"));
    return savingBlock;
}
async function _saveBlock(block) {
    try {
        const r = await api("PUT", `./api/blocks/${encodeURIComponent(block.id)}`, block);
        const idx = state.blocks.findIndex((b) => b.id === block.id);
        if (idx >= 0) state.blocks[idx] = r.block;
        setStatus("saved", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function saveStage(stage) {
    try {
        const r = await api("PUT", `./api/stages/${encodeURIComponent(stage.id)}`, stage);
        const idx = state.stages.findIndex((s) => s.id === stage.id);
        if (idx >= 0) state.stages[idx] = r.stage;
        setStatus("saved", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function saveEnemy(enemy) {
    try {
        const r = await api("PUT", `./api/enemies/${encodeURIComponent(enemy.id)}`, enemy);
        const idx = state.enemies.findIndex((x) => x.id === enemy.id);
        if (idx >= 0) state.enemies[idx] = r.enemy;
        setStatus("saved", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function saveSb(sb) {
    try {
        const r = await api("PUT", `./api/skill-blocks/${encodeURIComponent(sb.id)}`, sb);
        const idx = state.skillBlocks.findIndex((x) => x.id === sb.id);
        if (idx >= 0) state.skillBlocks[idx] = r.skillBlock;
        setStatus("saved", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function renameBlock(block, newId) {
    const copy = { ...block, id: newId };
    try {
        await api("PUT",    `./api/blocks/${encodeURIComponent(newId)}`,  copy);
        await api("DELETE", `./api/blocks/${encodeURIComponent(block.id)}`);
        state.blocks = state.blocks.filter((b) => b.id !== block.id);
        state.blocks.push(copy);
        for (const s of state.stages) s.blocks = s.blocks.map((bid) => (bid === block.id ? newId : bid));
        state.selectedBlockId = newId; render(); setStatus("renamed", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function renameStage(stage, newId) {
    const copy = { ...stage, id: newId };
    try {
        await api("PUT",    `./api/stages/${encodeURIComponent(newId)}`,  copy);
        await api("DELETE", `./api/stages/${encodeURIComponent(stage.id)}`);
        state.stages = state.stages.filter((s) => s.id !== stage.id);
        state.stages.push(copy);
        state.selectedStageId = newId; render(); setStatus("renamed", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function renameEnemy(enemy, newId) {
    const copy = { ...enemy, id: newId };
    try {
        await api("PUT",    `./api/enemies/${encodeURIComponent(newId)}`,   copy);
        await api("DELETE", `./api/enemies/${encodeURIComponent(enemy.id)}`);
        state.enemies = state.enemies.filter((e) => e.id !== enemy.id);
        state.enemies.push(copy);
        state.selectedEnemyId = newId; render(); setStatus("renamed", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

async function renameSb(sb, newId) {
    const copy = { ...sb, id: newId };
    try {
        await api("PUT",    `./api/skill-blocks/${encodeURIComponent(newId)}`,  copy);
        await api("DELETE", `./api/skill-blocks/${encodeURIComponent(sb.id)}`);
        state.skillBlocks = state.skillBlocks.filter((x) => x.id !== sb.id);
        state.skillBlocks.push(copy);
        for (const e of state.enemies) if (e.skillBlockId === sb.id) e.skillBlockId = newId;
        state.selectedSbId = newId; render(); setStatus("renamed", "ok");
    } catch (err) { setStatus(err.message, "err"); }
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

function wrapLabel(caption, child) {
    const l = document.createElement("label");
    l.innerHTML = `<span>${escapeHtml(caption)}</span>`;
    l.appendChild(child);
    return l;
}
function fieldInput(name, value, onChange) {
    const input = document.createElement("input");
    input.type = "text"; input.name = name; input.value = value;
    let timer = null;
    input.addEventListener("input", () => {
        if (timer) clearTimeout(timer);
        timer = setTimeout(() => onChange(input.value), 180);
    });
    return { input, wrapper: input };
}
function cornerLabel() {
    const d = document.createElement("div");
    d.className = "col-label"; d.textContent = "·";
    return d;
}
function swap(arr, i, j) { const t = arr[i]; arr[i] = arr[j]; arr[j] = t; }
function isFiniteNum(v) { return v !== "" && Number.isFinite(Number(v)); }

// ---------------------------------------------------------------------------
// Toolbar / events
// ---------------------------------------------------------------------------

$$(".tab").forEach((t) => t.addEventListener("click", () => { state.mode = t.dataset.mode; render(); }));

$("#block-add").addEventListener("click", async () => {
    try {
        const skel = await api("POST", "./api/new/block", {});
        await api("PUT", `./api/blocks/${encodeURIComponent(skel.id)}`, skel);
        state.blocks.push(skel); state.selectedBlockId = skel.id; render();
    } catch (err) { setStatus(err.message, "err"); }
});
$("#stage-add").addEventListener("click", async () => {
    try {
        const skel = await api("POST", "./api/new/stage", {});
        await api("PUT", `./api/stages/${encodeURIComponent(skel.id)}`, skel);
        state.stages.push(skel); state.selectedStageId = skel.id; render();
    } catch (err) { setStatus(err.message, "err"); }
});
$("#enemy-add").addEventListener("click", async () => {
    try {
        const skel = await api("POST", "./api/new/enemy", {});
        await api("PUT", `./api/enemies/${encodeURIComponent(skel.id)}`, skel);
        state.enemies.push(skel); state.selectedEnemyId = skel.id; render();
    } catch (err) { setStatus(err.message, "err"); }
});
$("#sb-add").addEventListener("click", async () => {
    try {
        const skel = await api("POST", "./api/new/skill-block", {});
        await api("PUT", `./api/skill-blocks/${encodeURIComponent(skel.id)}`, skel);
        state.skillBlocks.push(skel); state.selectedSbId = skel.id; render();
    } catch (err) { setStatus(err.message, "err"); }
});

$("#cell-close").addEventListener("click", () => $("#cell-dialog").close());
$("#new-obj-type").addEventListener("change", (ev) => {
    state.newObjDraft = makeDraft(ev.target.value);
    renderNewObjFields();
});
$("#new-obj-add").addEventListener("click", addNewObjToActiveCell);

boot();
