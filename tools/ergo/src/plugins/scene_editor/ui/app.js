// scene_editor browser UI — scene tree + actor inspector + WS patch relay.

let ws = null;
let reconnectTimer = null;

// state
const scenes = new Map();   // id -> SceneInfo
let selectedSceneId = null;
let selectedActorId = null;

// ── DOM refs ────────────────────────────────────────────────────────────────
const statusEl     = document.getElementById("status");
const treeScroll   = document.getElementById("tree-scroll");
const inspScroll   = document.getElementById("inspector-scroll");

// ── WebSocket ───────────────────────────────────────────────────────────────
function connect() {
    if (ws) { try { ws.close(); } catch (_) {} }
    ws = new WebSocket(`ws://${location.host}/scene_editor/ws`);

    ws.onopen = () => {
        setStatus("connected");
        ws.send(JSON.stringify({ op: "hello", role: "ui" }));
    };

    ws.onmessage = (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (!msg || !msg.op) return;

        switch (msg.op) {
            case "scenes":
                scenes.clear();
                for (const s of msg.scenes) scenes.set(s.id, s);
                renderTree();
                renderInspector();
                break;
            case "scene_update":
                scenes.set(msg.scene.id, msg.scene);
                renderTree();
                if (selectedSceneId === msg.scene.id) renderInspector();
                break;
            case "scene_removed":
                scenes.delete(msg.id);
                if (selectedSceneId === msg.id) { selectedSceneId = null; selectedActorId = null; }
                renderTree();
                renderInspector();
                break;
        }
    };

    ws.onclose = () => {
        setStatus("disconnected");
        reconnectTimer = setTimeout(connect, 3000);
    };
    ws.onerror = () => { ws.close(); };
}

function setStatus(s) {
    statusEl.textContent = s === "connected" ? "● connected" : "○ disconnected";
    statusEl.className = s;
}

// ── Scene tree ──────────────────────────────────────────────────────────────
function renderTree() {
    if (scenes.size === 0) {
        treeScroll.innerHTML = '<div class="empty-tree">No scenes mounted.<br>Connect an engine client<br>and push a scene via WS.</div>';
        return;
    }

    const frag = document.createDocumentFragment();
    for (const [sid, scene] of scenes) {
        const group = document.createElement("div");
        group.className = "scene-group";
        group.dataset.sceneId = sid;

        const label = document.createElement("div");
        label.className = "scene-label";
        label.innerHTML = `<span class="arrow">▾</span><span>${esc(sid)}</span><span style="margin-left:auto;font-size:10px;color:#7f849c">${esc(scene.domain)}${esc(scene.mount)}</span>`;
        label.addEventListener("click", () => {
            group.classList.toggle("collapsed");
        });
        group.appendChild(label);

        const list = document.createElement("div");
        list.className = "actor-list";

        for (const actor of scene.actors) {
            const item = document.createElement("div");
            item.className = "actor-item" + (sid === selectedSceneId && actor.id === selectedActorId ? " selected" : "");
            item.dataset.actorId = actor.id;
            item.innerHTML = `${esc(actor.name || actor.id)}<span class="actor-type">${esc(actor.type)}</span>`;
            item.title = actor.id;
            item.addEventListener("click", () => {
                selectedSceneId = sid;
                selectedActorId = actor.id;
                renderTree();
                renderInspector();
            });
            list.appendChild(item);
        }

        if (scene.actors.length === 0) {
            const empty = document.createElement("div");
            empty.style.cssText = "padding:6px 10px 6px 28px;color:#45475a;font-size:11px";
            empty.textContent = "(no actors)";
            list.appendChild(empty);
        }

        group.appendChild(list);
        frag.appendChild(group);
    }

    treeScroll.textContent = "";
    treeScroll.appendChild(frag);
}

// ── Inspector ───────────────────────────────────────────────────────────────
function renderInspector() {
    if (!selectedSceneId || !selectedActorId) {
        inspScroll.innerHTML = '<div id="inspector-empty">Select an actor in the scene tree.</div>';
        return;
    }
    const scene = scenes.get(selectedSceneId);
    if (!scene) { inspScroll.innerHTML = ""; return; }
    const actor = scene.actors.find(a => a.id === selectedActorId);
    if (!actor) { inspScroll.innerHTML = ""; return; }

    const t = actor.transform;

    inspScroll.innerHTML = `
<div class="field-group">
  <div class="field-group-label">Identity</div>
  ${fieldReadonly("id", actor.id)}
  ${fieldEditable("name", actor.name, "name")}
  ${fieldEditable("type", actor.type, "type")}
  ${fieldEditable("parent", actor.parent, "parent")}
</div>

<div class="field-group">
  <div class="field-group-label">Transform</div>
  ${vec3Field("position", "transform.pos", t.pos)}
  ${vec3Field("rotation", "transform.rot.xyz", [t.rot[0], t.rot[1], t.rot[2]])}
  ${vec3Field("scale", "transform.scale", t.scale)}
</div>

<div class="field-group">
  <div class="field-group-label">Visual</div>
  ${fieldEditable("kind", actor.visual.kind, "visual.kind")}
  ${fieldEditable("ref", actor.visual.ref, "visual.ref")}
  ${fieldEditable("material", actor.visual.material, "visual.material")}
</div>

${actor.instanceOf ? `<div class="field-group">
  <div class="field-group-label">Prefab</div>
  ${fieldReadonly("instanceOf", actor.instanceOf)}
  ${actor.overrides?.length ? `<div style="margin-top:4px;font-size:11px;color:#7f849c">${actor.overrides.length} override(s)</div>` : ""}
</div>` : ""}

${actor.vars && actor.vars.length ? `<div class="field-group">
  <div class="field-group-label">Vars</div>
  ${actor.vars.map(v => varRow(v)).join("")}
</div>` : ""}

${actor.components && actor.components.length ? `<div class="field-group">
  <div class="field-group-label">Components</div>
  ${actor.components.map(c => compRow(c)).join("")}
</div>` : ""}
`;

    // bind patch buttons
    inspScroll.querySelectorAll("[data-field]").forEach(el => {
        const btn = el.querySelector(".patch-btn");
        if (!btn) return;
        btn.addEventListener("click", () => {
            const input = el.querySelector(".field-input");
            if (!input) return;
            sendPatch(el.dataset.field, input.value);
        });
    });

    // vec3 patch buttons
    inspScroll.querySelectorAll("[data-vec-field]").forEach(el => {
        const btn = el.querySelector(".patch-btn");
        if (!btn) return;
        btn.addEventListener("click", () => {
            const inputs = el.querySelectorAll(".field-input.vec");
            if (inputs.length < 3) return;
            const val = [parseFloat(inputs[0].value), parseFloat(inputs[1].value), parseFloat(inputs[2].value)];
            sendPatch(el.dataset.vecField, val);
        });
    });
}

function sendPatch(field, value) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
        op: "patch",
        scene_id: selectedSceneId,
        actor_id: selectedActorId,
        field,
        value,
    }));
}

// ── HTML helpers ─────────────────────────────────────────────────────────────
function esc(s) {
    return String(s ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");
}

function fieldReadonly(label, value) {
    return `<div class="field-row">
  <span class="field-label">${esc(label)}</span>
  <input class="field-input" readonly value="${esc(value)}">
</div>`;
}

function fieldEditable(label, value, field) {
    return `<div class="field-row" data-field="${esc(field)}">
  <span class="field-label">${esc(label)}</span>
  <input class="field-input" value="${esc(value)}">
  <button class="patch-btn">Apply</button>
</div>`;
}

function vec3Field(label, vecField, vec) {
    const [x, y, z] = vec;
    return `<div class="field-row" data-vec-field="${esc(vecField)}">
  <span class="field-label">${esc(label)}</span>
  <div class="vec-group">
    <span class="vec-axis">X</span><input class="field-input vec" value="${x.toFixed(3)}">
    <span class="vec-axis">Y</span><input class="field-input vec" value="${y.toFixed(3)}">
    <span class="vec-axis">Z</span><input class="field-input vec" value="${z.toFixed(3)}">
  </div>
  <button class="patch-btn">Apply</button>
</div>`;
}

function varRow(v) {
    return `<div class="var-row">
  <span class="var-name">${esc(v.name)}</span><span class="var-type">${esc(v.type)}</span>
  <div class="var-value">${esc(v.value)}</div>
</div>`;
}

function compRow(c) {
    const propHtml = c.props.length
        ? `<div class="comp-props">${c.props.map(([k,v]) => `<span class="tag-badge">${esc(k)}=${esc(v)}</span>`).join("")}</div>`
        : "";
    return `<div class="comp-row"><span class="comp-type">${esc(c.type)}</span>${propHtml}</div>`;
}

// ── Init ─────────────────────────────────────────────────────────────────────
setStatus("disconnected");
connect();
