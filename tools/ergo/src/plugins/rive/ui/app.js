// Rive Player — @rive-app/canvas-advanced を CDN ロードして .riv を再生 + 構造可視化。
//
// API メモ:
//   - import('https://unpkg.com/@rive-app/canvas-advanced@2.x/rive.mjs')
//     から Rive (factory) を取得
//   - new Rive({ canvas, buffer, ... }) で再生
//   - rive.artboardNames / rive.stateMachineNames / rive.animationNames
//   - StateMachineInput は rive.stateMachineInputs(name) で取得
//
// HTML 側で用意した要素を参照する DOM ID は html を参照。

import RiveCanvas from "https://unpkg.com/@rive-app/canvas-advanced@2.30.0/rive.mjs";

// --- DOM ----------------------------------------------------------------
const $ = (id) => document.getElementById(id);
const statusEl       = $("status");
const dropzone       = $("dropzone");
const fileInput      = $("file_input");
const fileBtn        = $("file_btn");
const canvasEl       = $("canvas");
const artboardSel    = $("artboard_select");
const sceneSel       = $("scene_select");
const fitSel         = $("fit_select");
const playBtn        = $("play_btn");
const pauseBtn       = $("pause_btn");
const speedInput     = $("speed_input");
const speedLabel     = $("speed_label");

const metaSummaryEl  = $("meta_summary");
const artboardListEl = $("artboard_list");
const smInputsEl     = $("sm_inputs");
const animationListEl= $("animation_list");
const timelineCursor = $("timeline_cursor");
const timelineReadout= $("timeline_readout");

// --- 状態 ---------------------------------------------------------------
let riveModule    = null;   // Rive WASM module (from RiveCanvas())
let riveFile      = null;   // currently loaded rive.File
let artboard      = null;   // currently selected Artboard
let scene         = null;   // SM instance or LinearAnimation instance
let sceneIsSM     = false;
let renderer      = null;   // rive.Renderer (canvas)
let lastTimeMs    = 0;
let playing       = false;
let playbackSpeed = 1.0;
let currentFit    = "contain";
let currentBuffer = null;   // ArrayBuffer (再 importFile 用)
let currentName   = "";
let activeAnimDuration = 0; // 現在再生中アニメの duration (sec, SM 時は 0)
let elapsedSec    = 0;

let ws = null;

// --- WebSocket ----------------------------------------------------------
(function openWs() {
    try {
        const proto = location.protocol === "https:" ? "wss" : "ws";
        ws = new WebSocket(`${proto}://${location.host}/rive/ws`);
        ws.addEventListener("open", () => {
            // 開いた瞬間 ping だけ
            ws.send(JSON.stringify({ op: "ping" }));
        });
        ws.addEventListener("error", () => { /* server なしでも UI は動かす */ });
    } catch {
        ws = null;
    }
})();

function publishMeta() {
    if (!ws || ws.readyState !== WebSocket.OPEN || !riveFile) return;
    const ab_names = riveFile.artboardNames || [];
    const artboards = ab_names.map((n) => {
        const ab = riveFile.artboardByName(n);
        if (!ab) return null;
        const smNames   = ab.stateMachineNames   ? ab.stateMachineNames()   : [];
        const animNames = ab.animationNames      ? ab.animationNames()      : [];
        const animDurs  = animNames.map((nm) => {
            try {
                const inst = ab.animationByName(nm);
                return inst ? (inst.duration / inst.fps) : 0;
            } catch { return 0; }
        });
        const out = {
            name:               n,
            width:              ab.width,
            height:             ab.height,
            stateMachineNames:  smNames,
            animationNames:     animNames,
            animationDurations: animDurs,
        };
        try { ab.delete(); } catch {}
        return out;
    }).filter(Boolean);
    ws.send(JSON.stringify({
        op: "publish",
        meta: {
            name: currentName,
            size: currentBuffer ? currentBuffer.byteLength : 0,
            artboards,
        },
    }));
}

// --- ファイル受け取り ---------------------------------------------------
function onFile(file) {
    if (!file) return;
    currentName = file.name;
    statusEl.textContent = `読み込み中: ${file.name} (${file.size} bytes)`;
    const reader = new FileReader();
    reader.onload = async (e) => {
        currentBuffer = e.target.result;
        await loadRiv(new Uint8Array(currentBuffer));
    };
    reader.readAsArrayBuffer(file);
}
fileBtn.addEventListener("click", () => fileInput.click());
fileInput.addEventListener("change", (e) => onFile(e.target.files[0]));

["dragenter", "dragover"].forEach((evt) =>
    dropzone.addEventListener(evt, (e) => {
        e.preventDefault(); dropzone.classList.add("over");
    })
);
["dragleave", "drop"].forEach((evt) =>
    dropzone.addEventListener(evt, (e) => {
        e.preventDefault(); dropzone.classList.remove("over");
    })
);
dropzone.addEventListener("drop", (e) => {
    const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
    if (f) onFile(f);
});

// --- Rive ロード本体 ---------------------------------------------------
async function ensureRive() {
    if (!riveModule) {
        riveModule = await RiveCanvas({
            locateFile: (path) => `https://unpkg.com/@rive-app/canvas-advanced@2.30.0/${path}`,
        });
    }
    if (!renderer) {
        renderer = riveModule.makeRenderer(canvasEl);
    }
}

async function loadRiv(bytes) {
    await ensureRive();
    // 既存ファイル破棄
    teardownScene();
    if (riveFile) { try { riveFile.delete(); } catch {} riveFile = null; }
    // import
    riveFile = await riveModule.load(bytes);
    if (!riveFile) {
        statusEl.textContent = "Rive ファイルのインポートに失敗";
        return;
    }
    // artboard リスト構築
    refreshArtboardList();
    // 既定 artboard を初期選択
    const names = riveFile.artboardNames;
    if (names && names.length > 0) {
        artboardSel.value = names[0];
        await selectArtboard(names[0]);
    }
    statusEl.textContent = `${currentName} — ${names ? names.length : 0} artboards`;
    publishMeta();
}

function refreshArtboardList() {
    artboardSel.innerHTML = "";
    artboardListEl.innerHTML = "";
    const names = riveFile.artboardNames || [];
    names.forEach((n) => {
        const o = document.createElement("option");
        o.value = n; o.textContent = n;
        artboardSel.appendChild(o);

        const li = document.createElement("li");
        const ab = riveFile.artboardByName(n);
        const w = ab ? ab.width  : 0;
        const h = ab ? ab.height : 0;
        const sm_count   = ab && ab.stateMachineNames ? ab.stateMachineNames().length : 0;
        const anim_count = ab && ab.animationNames    ? ab.animationNames().length    : 0;
        if (ab) try { ab.delete(); } catch {}
        li.innerHTML = `<span>${n}</span><span class="sub">${w}×${h} · SM${sm_count} · anim${anim_count}</span>`;
        li.addEventListener("click", () => { artboardSel.value = n; selectArtboard(n); });
        artboardListEl.appendChild(li);
    });
    // メタサマリ
    metaSummaryEl.textContent = `${currentName} · ${currentBuffer ? currentBuffer.byteLength : 0} bytes · ${names.length} artboards`;
}

function teardownScene() {
    if (scene) {
        try { scene.delete(); } catch {}
        scene = null;
    }
    if (artboard) {
        try { artboard.delete(); } catch {}
        artboard = null;
    }
    sceneIsSM = false;
    activeAnimDuration = 0;
    elapsedSec = 0;
}

async function selectArtboard(name) {
    teardownScene();
    artboard = riveFile.artboardByName(name);
    if (!artboard) return;
    // scene 候補リスト = "SM:<name>" と "Anim:<name>"
    sceneSel.innerHTML = "";
    const smNames   = artboard.stateMachineNames ? artboard.stateMachineNames() : [];
    const animNames = artboard.animationNames    ? artboard.animationNames()    : [];
    smNames.forEach((n) => {
        const o = document.createElement("option");
        o.value = `sm:${n}`; o.textContent = `SM · ${n}`;
        sceneSel.appendChild(o);
    });
    animNames.forEach((n) => {
        const o = document.createElement("option");
        o.value = `anim:${n}`; o.textContent = `Anim · ${n}`;
        sceneSel.appendChild(o);
    });
    // animation リスト
    animationListEl.innerHTML = "";
    animNames.forEach((n) => {
        const li = document.createElement("li");
        let dur = 0;
        try {
            const inst = artboard.animationByName(n);
            if (inst) { dur = inst.duration / inst.fps; inst.delete(); }
        } catch {}
        li.innerHTML = `<span>${n}</span><span class="sub">${dur.toFixed(3)} s</span>`;
        const btn = document.createElement("button");
        btn.textContent = "▶";
        btn.addEventListener("click", () => { sceneSel.value = `anim:${n}`; selectScene(`anim:${n}`); });
        li.appendChild(btn);
        animationListEl.appendChild(li);
    });
    // 既定 scene: SM 0 があればそれ、 なければ anim 0
    if (smNames.length > 0) {
        sceneSel.value = `sm:${smNames[0]}`;
        await selectScene(`sm:${smNames[0]}`);
    } else if (animNames.length > 0) {
        sceneSel.value = `anim:${animNames[0]}`;
        await selectScene(`anim:${animNames[0]}`);
    }
    highlightActiveArtboard(name);
}

function highlightActiveArtboard(name) {
    [...artboardListEl.children].forEach((li) => {
        if (li.firstChild && li.firstChild.textContent === name) li.classList.add("active");
        else li.classList.remove("active");
    });
}

async function selectScene(value) {
    if (!artboard) return;
    if (scene) { try { scene.delete(); } catch {} scene = null; }
    sceneIsSM = false;
    elapsedSec = 0;
    activeAnimDuration = 0;
    smInputsEl.innerHTML = "";
    const [kind, name] = value.split(/:(.+)/);
    if (kind === "sm") {
        scene = new riveModule.StateMachineInstance(artboard.stateMachineByName(name), artboard);
        sceneIsSM = true;
        // input 列挙
        const n = scene.inputCount();
        for (let i = 0; i < n; ++i) {
            const inp = scene.input(i);
            renderSMInputRow(inp);
        }
        // SM は明確な duration が無いので 0 (timeline は計測値で出す)
    } else if (kind === "anim") {
        const inst = artboard.animationByName(name);
        scene = new riveModule.LinearAnimationInstance(inst, artboard);
        activeAnimDuration = inst.duration / inst.fps;
        smInputsEl.textContent = "(animation 駆動 — state machine inputs 無し)";
    }
    if (!playing) play(); // 自動再生
}

function renderSMInputRow(inp) {
    const row = document.createElement("div");
    row.className = "input-row";
    // Rive runtime: inp.type は number; 56=trigger 58=bool 60=number (要件依存)
    // 公開 API では inp.asBool() / inp.asNumber() / inp.asTrigger() 風アクセサがある。
    // 安全のため try/catch して持っているものだけ使う。
    const name = document.createElement("span");
    name.className = "name"; name.textContent = inp.name;
    row.appendChild(name);
    const kind = document.createElement("span");
    kind.className = "kind"; kind.textContent = inp.type ?? "?";
    row.appendChild(kind);
    // bool 想定
    try {
        const asBool = inp.asBool && inp.asBool();
        if (asBool && typeof asBool.value !== "undefined") {
            const cb = document.createElement("input");
            cb.type = "checkbox"; cb.checked = !!asBool.value;
            cb.addEventListener("change", () => { asBool.value = cb.checked; });
            row.appendChild(cb);
            smInputsEl.appendChild(row);
            return;
        }
    } catch {}
    // number 想定
    try {
        const asNum = inp.asNumber && inp.asNumber();
        if (asNum && typeof asNum.value !== "undefined") {
            const num = document.createElement("input");
            num.type = "number"; num.value = String(asNum.value); num.step = "0.1";
            num.addEventListener("change", () => { asNum.value = Number(num.value); });
            row.appendChild(num);
            smInputsEl.appendChild(row);
            return;
        }
    } catch {}
    // trigger 想定
    try {
        const asTrig = inp.asTrigger && inp.asTrigger();
        if (asTrig && typeof asTrig.fire === "function") {
            const btn = document.createElement("button");
            btn.className = "trigger"; btn.textContent = "fire";
            btn.addEventListener("click", () => asTrig.fire());
            row.appendChild(btn);
            smInputsEl.appendChild(row);
            return;
        }
    } catch {}
    // 未対応の input kind
    const note = document.createElement("span");
    note.className = "kind"; note.textContent = "(unsupported)";
    row.appendChild(note);
    smInputsEl.appendChild(row);
}

// --- UI イベント -------------------------------------------------------
artboardSel.addEventListener("change", () => selectArtboard(artboardSel.value));
sceneSel.addEventListener("change",    () => selectScene(sceneSel.value));
fitSel.addEventListener("change",      () => { currentFit = fitSel.value; });
playBtn.addEventListener("click",      () => play());
pauseBtn.addEventListener("click",     () => pause());
speedInput.addEventListener("input",   () => {
    playbackSpeed = Number(speedInput.value);
    speedLabel.textContent = playbackSpeed.toFixed(2) + "×";
});

function play()  { if (!playing) { playing = true; lastTimeMs = performance.now(); requestAnimationFrame(tick); } }
function pause() { playing = false; }

// --- 描画ループ -------------------------------------------------------
function tick(nowMs) {
    if (!playing || !riveModule || !artboard || !scene || !renderer) return;
    const dt = Math.min(0.1, (nowMs - lastTimeMs) / 1000) * playbackSpeed;
    lastTimeMs = nowMs;

    if (sceneIsSM) scene.advance(artboard, dt);
    else           scene.advance(dt);
    artboard.advance(dt);
    elapsedSec += dt;

    renderer.clear();
    renderer.save();
    const fit = riveModule.Fit[currentFit] ?? riveModule.Fit.contain;
    const alignment = riveModule.Alignment.center;
    renderer.align(fit, alignment,
        { minX: 0, minY: 0, maxX: canvasEl.width, maxY: canvasEl.height },
        artboard.bounds);
    artboard.draw(renderer);
    renderer.restore();
    renderer.flush();

    // timeline
    let tNorm = 0;
    let cur = elapsedSec;
    if (sceneIsSM) {
        // SM は無限長扱い: 適当に 10s ループでカーソル表示
        const wrap = 10;
        cur = elapsedSec % wrap;
        tNorm = cur / wrap;
        timelineReadout.textContent = `SM elapsed: ${elapsedSec.toFixed(2)} s`;
    } else if (activeAnimDuration > 0) {
        cur = elapsedSec % activeAnimDuration;
        tNorm = cur / activeAnimDuration;
        timelineReadout.textContent = `${cur.toFixed(2)} / ${activeAnimDuration.toFixed(2)} s`;
    } else {
        timelineReadout.textContent = `${elapsedSec.toFixed(2)} s`;
    }
    timelineCursor.style.left = (tNorm * 100).toFixed(2) + "%";

    requestAnimationFrame(tick);
}
