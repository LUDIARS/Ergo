// Ergo Studio (web). Talks only to its own origin: the plugins' REST routes
// and the variable WebSocket (role "ui"). Every value from the server is put
// on the page with textContent (never as HTML).
'use strict';

const $ = (id) => document.getElementById(id);

function el(tag, props, ...children) {
  const e = document.createElement(tag);
  if (props) {
    for (const [k, v] of Object.entries(props)) {
      if (k === 'class') e.className = v;
      else if (k === 'text') e.textContent = v;
      else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
      else e.setAttribute(k, v);
    }
  }
  for (const c of children) if (c != null) e.append(c);
  return e;
}

let toastTimer = 0;
function toast(msg, bad) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast' + (bad ? ' bad' : '');
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, 4000);
}

async function api(path, opts) {
  const init = { method: 'GET', headers: {}, credentials: 'same-origin' };
  if (opts && opts.body !== undefined) {
    init.method = opts.method || 'POST';
    init.headers['content-type'] = 'application/json';
    init.body = JSON.stringify(opts.body);
  }
  const res = await fetch(path, init);
  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = text; }
  if (!res.ok) {
    const err = (data && data.err) || ('HTTP ' + res.status);
    throw new Error(err);
  }
  return data;
}

// ---- tabs ------------------------------------------------------------------

for (const b of document.querySelectorAll('#tabs button')) {
  b.addEventListener('click', () => {
    for (const x of document.querySelectorAll('#tabs button')) x.classList.toggle('active', x === b);
    for (const s of document.querySelectorAll('.tab')) s.classList.toggle('active', s.id === 'tab-' + b.dataset.tab);
    if (b.dataset.tab === 'assets' && !assets.loaded) loadAssetRoots();
    if (b.dataset.tab === 'tuning') loadVersions();
    if (b.dataset.tab === 'lighting') loadPresets();
  });
}

// ---- variables (ergo_bind through the variable plugin) ---------------------

const vars = new Map();   // name -> {name, kind, meta, value, app}
const controls = new Map();   // name -> [update(value)]
let ws = null;
let retry = 0;

function wsUrl() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  return proto + '//' + location.host + '/variable/ws';
}

function connect() {
  ws = new WebSocket(wsUrl());
  ws.addEventListener('open', () => {
    retry = 0;
    ws.send(JSON.stringify({ op: 'hello', role: 'ui' }));
    setLink(true);
  });
  ws.addEventListener('message', (ev) => {
    let m;
    try { m = JSON.parse(ev.data); } catch { return; }
    if (m.op === 'registry' && Array.isArray(m.vars)) {
      vars.clear();
      for (const v of m.vars) vars.set(v.name, v);
      renderVariables();
    } else if (m.op === 'value' && vars.has(m.name)) {
      vars.get(m.name).value = m.value;
      for (const f of controls.get(m.name) || []) f(m.value);
    }
  });
  ws.addEventListener('close', () => {
    setLink(false);
    retry = Math.min(retry + 1, 6);
    setTimeout(connect, 500 * retry);
  });
}

function setLink(ok) {
  const s = $('link-state');
  const apps = new Set([...vars.values()].map((v) => v.app));
  s.textContent = !ok ? '未接続' : apps.size ? 'ゲーム接続中: ' + [...apps].join(', ') : 'Studio 接続中 (ゲーム未接続)';
  s.className = 'pill' + (ok && apps.size ? ' ok' : ok ? '' : ' bad');
}

function sendSet(name, value) {
  if (!ws || ws.readyState !== WebSocket.OPEN) { toast('Studio に接続していません', true); return; }
  ws.send(JSON.stringify({ op: 'set', name, value }));
}

function categoryOf(v) { return (v.meta && v.meta.category) || '(分類なし)'; }
function isLook(v) { return categoryOf(v).startsWith('look/'); }

function fillAppSelect(sel, list) {
  const keep = sel.value;
  sel.textContent = '';
  for (const a of list) sel.append(el('option', { value: a, text: a }));
  if (list.includes(keep)) sel.value = keep;
}

function renderVariables() {
  controls.clear();
  const apps = [...new Set([...vars.values()].map((v) => v.app))].sort();
  const lookApps = [...new Set([...vars.values()].filter(isLook).map((v) => v.app))].sort();
  fillAppSelect($('look-app'), lookApps);
  fillAppSelect($('tune-app'), apps);
  renderGroups($('look-groups'), (v) => v.app === $('look-app').value && isLook(v) && categoryOf(v) !== 'look/Capture' &&
    !['look.apply', 'look.preset', 'look.capture_request', 'look.capture_path'].includes(v.name),
    'ライティングの値を出しているゲームがありません。');
  const filter = $('tune-filter').value.trim().toLowerCase();
  renderGroups($('tune-groups'), (v) => v.app === $('tune-app').value && (!filter || v.name.toLowerCase().includes(filter)),
    '値を出しているゲームがありません。');
  setLink(ws && ws.readyState === WebSocket.OPEN);
}

$('look-app').addEventListener('change', renderVariables);
$('tune-app').addEventListener('change', () => { renderVariables(); loadVersions(); });
$('tune-filter').addEventListener('input', renderVariables);

function renderGroups(root, pick, emptyText) {
  root.textContent = '';
  const groups = new Map();
  for (const v of [...vars.values()].filter(pick).sort((a, b) => a.name.localeCompare(b.name))) {
    const c = categoryOf(v);
    if (!groups.has(c)) groups.set(c, []);
    groups.get(c).push(v);
  }
  if (!groups.size) { root.append(el('p', { class: 'empty', text: emptyText })); return; }
  for (const [cat, list] of [...groups.entries()].sort()) {
    const box = el('fieldset', { class: 'group' }, el('legend', { text: cat }));
    for (const v of list) box.append(row(v));
    root.append(box);
  }
}

function watch(name, f) {
  if (!controls.has(name)) controls.set(name, []);
  controls.get(name).push(f);
}

function numberControl(v, getComponent, setComponent, label) {
  const m = v.meta || {};
  const ro = !!m.read_only;
  const isInt = v.kind === 'int32' || v.kind === 'int64';
  const min = typeof m.min === 'number' ? m.min : null;
  const max = typeof m.max === 'number' ? m.max : null;
  const step = typeof m.step === 'number' ? m.step : isInt ? 1 : 'any';
  const wrap = el('span', { class: 'num' });
  if (label) wrap.append(el('span', { class: 'comp', text: label }));
  let slider = null;
  if (min !== null && max !== null) {
    slider = el('input', { type: 'range', min: String(min), max: String(max), step: String(step === 'any' ? (max - min) / 200 : step) });
    slider.disabled = ro;
    wrap.append(slider);
  }
  const box = el('input', { type: 'number', step: String(step) });
  box.disabled = ro;
  wrap.append(box);
  const show = (val) => {
    const n = getComponent(val);
    if (typeof n !== 'number') return;
    if (slider && document.activeElement !== slider) slider.value = String(n);
    if (document.activeElement !== box) box.value = String(isInt ? Math.round(n) : Math.round(n * 10000) / 10000);
  };
  show(v.value);
  watch(v.name, show);
  const push = (raw) => {
    let n = Number(raw);
    if (!Number.isFinite(n)) return;
    if (isInt) n = Math.round(n);
    sendSet(v.name, setComponent(vars.get(v.name).value, n));
  };
  if (slider) slider.addEventListener('input', () => { box.value = slider.value; push(slider.value); });
  box.addEventListener('change', () => push(box.value));
  return wrap;
}

function row(v) {
  const r = el('div', { class: 'row' }, el('label', { class: 'name', text: v.name.replace(/^look\./, ''), title: v.name }));
  const ro = !!(v.meta && v.meta.read_only);
  if (v.kind === 'bool') {
    const c = el('input', { type: 'checkbox' });
    c.checked = !!v.value;
    c.disabled = ro;
    watch(v.name, (val) => { c.checked = !!val; });
    c.addEventListener('change', () => sendSet(v.name, c.checked));
    r.append(c);
  } else if (v.kind === 'vec3' || v.kind === 'color') {
    const names = v.kind === 'color' ? ['R', 'G', 'B', '補正'] : ['R', 'G', 'B'];
    names.forEach((label, i) => {
      r.append(numberControl(v, (val) => (Array.isArray(val) ? val[i] : undefined),
        (cur, n) => { const a = Array.isArray(cur) ? cur.slice() : names.map(() => 0); a[i] = n; return a; }, label));
    });
  } else if (v.kind === 'string') {
    const t = el('input', { type: 'text' });
    t.value = v.value == null ? '' : String(v.value);
    t.disabled = ro;
    watch(v.name, (val) => { if (document.activeElement !== t) t.value = val == null ? '' : String(val); });
    t.addEventListener('change', () => sendSet(v.name, t.value));
    r.append(t);
  } else {
    r.append(numberControl(v, (val) => val, (_cur, n) => n));
  }
  if (v.meta && v.meta.unit) r.append(el('span', { class: 'unit', text: v.meta.unit }));
  return r;
}

// ---- lighting presets (lookdev plugin) --------------------------------------

async function loadPresets() {
  try {
    const list = await api('/lookdev/api/presets');
    const sel = $('preset-list');
    sel.textContent = '';
    for (const p of list) sel.append(el('option', { value: p.name, text: p.name + (p.updated ? ' (' + p.updated + ')' : '') }));
    $('preset-msg').textContent = list.length ? '' : '保存したプリセットはまだありません。';
  } catch (e) { $('preset-msg').textContent = 'プリセットを読めません: ' + e.message; }
}

$('preset-load').addEventListener('click', async () => {
  const name = $('preset-list').value;
  if (!name) return;
  try {
    const p = await api('/lookdev/api/presets/' + encodeURIComponent(name));
    if (!vars.has('look.apply')) { toast('ゲームが look.apply を出していません', true); return; }
    sendSet('look.apply', p.text);
    toast('「' + name + '」を反映しました');
  } catch (e) { toast(e.message, true); }
});

$('preset-save').addEventListener('click', async () => {
  const name = $('preset-name').value.trim();
  const cur = vars.get('look.preset');
  if (!name) { toast('プリセット名を入れてください', true); return; }
  if (!cur) { toast('ゲームが look.preset を出していません', true); return; }
  try {
    await api('/lookdev/api/presets', { body: { name, text: String(cur.value || '') } });
    toast('「' + name + '」を保存しました');
    loadPresets();
  } catch (e) { toast(e.message, true); }
});

// ---- versions of the values (tuning plugin) --------------------------------

function currentValues(app) {
  const out = {};
  for (const v of vars.values()) {
    if (v.app === app && !(v.meta && v.meta.read_only)) out[v.name] = v.value;
  }
  return out;
}

async function loadVersions() {
  const app = $('tune-app').value;
  const sel = $('ver-list');
  sel.textContent = '';
  if (!app) return;
  try {
    const data = await api('/tuning/api/versions?app=' + encodeURIComponent(app));
    for (const v of data.versions || []) sel.append(el('option', { value: v.id, text: v.name + ' — ' + v.created + (v.note ? ' / ' + v.note : '') }));
    $('ver-msg').textContent = data.folder ? '保存先: ' + data.folder : '';
  } catch (e) { $('ver-msg').textContent = '版を読めません: ' + e.message; }
}

$('ver-save').addEventListener('click', async () => {
  const app = $('tune-app').value;
  const name = $('ver-name').value.trim();
  if (!app || !name) { toast('ゲームと版の名前を選んでください', true); return; }
  try {
    await api('/tuning/api/versions', { body: { app, name, note: $('ver-note').value.trim(), values: currentValues(app) } });
    toast('版「' + name + '」を保存しました');
    loadVersions();
  } catch (e) { toast(e.message, true); }
});

async function selectedVersion() {
  const app = $('tune-app').value;
  const id = $('ver-list').value;
  if (!app || !id) throw new Error('版を選んでください');
  return api('/tuning/api/versions/' + encodeURIComponent(app) + '/' + encodeURIComponent(id));
}

$('ver-diff').addEventListener('click', async () => {
  try {
    const ver = await selectedVersion();
    const lines = [];
    for (const [name, value] of Object.entries(ver.values || {})) {
      const live = vars.get(name);
      if (!live) lines.push(name + ': ' + JSON.stringify(value) + ' (今のゲームに無い)');
      else if (JSON.stringify(live.value) !== JSON.stringify(value)) lines.push(name + ': ' + JSON.stringify(live.value) + ' → ' + JSON.stringify(value));
    }
    const out = $('ver-diff-out');
    out.textContent = lines.length ? lines.join('\n') : '今の値と同じです。';
    out.hidden = false;
  } catch (e) { toast(e.message, true); }
});

$('ver-restore').addEventListener('click', async () => {
  try {
    const ver = await selectedVersion();
    let skipped = 0;
    for (const [name, value] of Object.entries(ver.values || {})) {
      if (vars.has(name)) sendSet(name, value); else skipped++;
    }
    toast('版「' + ver.name + '」に戻しました' + (skipped ? ' (今のゲームに無い ' + skipped + ' 件は飛ばしました)' : ''));
  } catch (e) { toast(e.message, true); }
});

// ---- services (Excubitor through the services plugin) -----------------------

async function loadServices() {
  const ul = $('svc-list');
  ul.textContent = '';
  try {
    const list = await api('/services/api/services');
    if (!list.length) ul.append(el('li', { class: 'empty', text: 'サービスがありません。' }));
    for (const s of list) {
      const state = s.running ? '動作中' : (s.state || '停止');
      ul.append(el('li', null,
        el('div', { class: 'svc-name', text: s.name || s.code }),
        el('div', { class: 'svc-meta', text: s.code + ' / ' + state + (s.port ? ' / :' + s.port : '') }),
        el('div', { class: 'svc-actions' },
          el('button', { text: '起動', onclick: () => control(s.code, 'start') }),
          el('button', { text: '再起動', onclick: () => control(s.code, 'restart') }),
          el('button', { text: '停止', onclick: () => control(s.code, 'stop') }),
          el('button', { text: 'ログ', onclick: () => showLogs(s.code) }))));
    }
  } catch (e) { ul.append(el('li', { class: 'empty', text: 'サービスを読めません: ' + e.message })); }
}

async function control(code, action, force) {
  try {
    await api('/services/api/services/' + encodeURIComponent(code) + '/control', { body: { action, force: !!force } });
    toast(code + ': ' + action + ' を受け付けました');
    setTimeout(loadServices, 1500);
  } catch (e) {
    if (/test|claim|テスト/i.test(e.message) && !force && confirm(code + ' は別の作業がテスト中です。それでも ' + action + ' しますか？')) {
      control(code, action, true);
      return;
    }
    toast(code + ': ' + e.message, true);
  }
}

async function showLogs(code) {
  const out = $('svc-log');
  try {
    const data = await api('/services/api/services/' + encodeURIComponent(code) + '/logs');
    const lines = Array.isArray(data) ? data : (data.lines || data.logs || []);
    out.textContent = (Array.isArray(lines) ? lines.map((l) => (typeof l === 'string' ? l : JSON.stringify(l))).join('\n') : JSON.stringify(data, null, 2)) || '(ログなし)';
  } catch (e) { out.textContent = 'ログを読めません: ' + e.message; }
  out.hidden = false;
}

$('svc-reload').addEventListener('click', loadServices);
async function loadGen5() {
  const button = $('gen5-reload');
  if (button.disabled) return;
  button.disabled = true;
  const list = $('gen5-list');
  list.textContent = '読み込み中…';
  try {
    const services = await api('/services/api/gen5');
    const labels = { running: '稼働中', stopped: '停止', unregistered: '未登録', unknown: '不明' };
    list.replaceChildren(...services.map(s => el('li', {
      text: s.code + ' — ' + (Object.hasOwn(labels, s.state) ? labels[s.state] : s.state || '不明')
    })));
  } catch (err) {
    list.textContent = 'Gen5 の状態を取得できませんでした。' + err.message;
  } finally {
    button.disabled = false;
  }
}
$('gen5-reload').addEventListener('click', loadGen5);
$('gen5-services').addEventListener('toggle', e => { if (e.target.open) loadGen5(); });

// ---- assets (assets plugin) --------------------------------------------------

const assets = { loaded: false, root: 0, dir: '', entries: [] };

function fmtSize(n) {
  if (!n) return '';
  const u = ['B', 'KB', 'MB', 'GB'];
  let i = 0;
  while (n >= 1024 && i < u.length - 1) { n /= 1024; i++; }
  return (i ? n.toFixed(1) : String(n)) + ' ' + u[i];
}

function fmtTime(s) {
  if (!s) return '';
  const d = new Date(s * 1000);
  return d.toLocaleString('ja-JP', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' });
}

const KIND = { dir: 'フォルダ', model: 'モデル', texture: '画像', audio: '音', shader: 'シェーダー', scene: 'シーン', data: 'データ', other: 'その他' };

async function loadAssetRoots() {
  const sel = $('asset-root');
  sel.textContent = '';
  try {
    const roots = await api('/assets/api/roots');
    assets.loaded = true;
    if (!roots.length) {
      sel.append(el('option', { text: '(プロフィールに assets がありません)' }));
      return;
    }
    for (const r of roots) sel.append(el('option', { value: String(r.index), text: r.profile + ' / ' + r.name + (r.exists ? '' : ' (見つかりません)') }));
    assets.root = roots[0].index;
    openDir('');
  } catch (e) { toast('アセットを読めません: ' + e.message, true); }
}

$('asset-root').addEventListener('change', () => { assets.root = Number($('asset-root').value); openDir(''); });
$('asset-filter').addEventListener('input', renderAssets);

async function openDir(dir) {
  try {
    const data = await api('/assets/api/list?root=' + assets.root + '&dir=' + encodeURIComponent(dir));
    assets.dir = dir;
    assets.entries = data.entries || [];
    if (data.truncated) toast('件数が多いので先頭の ' + assets.entries.length + ' 件だけ出しています');
    renderCrumbs();
    renderAssets();
  } catch (e) { toast(e.message, true); }
}

function renderCrumbs() {
  const c = $('asset-crumbs');
  c.textContent = '';
  c.append(el('a', { href: '#', text: '(最上位)', onclick: (ev) => { ev.preventDefault(); openDir(''); } }));
  const parts = assets.dir ? assets.dir.split('/') : [];
  parts.forEach((p, i) => {
    const path = parts.slice(0, i + 1).join('/');
    c.append(' / ', el('a', { href: '#', text: p, onclick: (ev) => { ev.preventDefault(); openDir(path); } }));
  });
}

function renderAssets() {
  const body = $('asset-table').querySelector('tbody');
  body.textContent = '';
  const f = $('asset-filter').value.trim().toLowerCase();
  const list = assets.entries.filter((e) => !f || e.name.toLowerCase().includes(f));
  if (!list.length) body.append(el('tr', null, el('td', { colspan: '4', class: 'empty', text: '何もありません。' })));
  for (const e of list) {
    const name = el('a', { href: '#', text: (e.type === 'dir' ? '📁 ' : '') + e.name });
    name.addEventListener('click', (ev) => {
      ev.preventDefault();
      if (e.type === 'dir') openDir(e.path); else preview(e);
    });
    body.append(el('tr', null, el('td', null, name), el('td', { text: KIND[e.kind] || e.kind }),
      el('td', { class: 'right', text: fmtSize(e.size) }), el('td', { text: fmtTime(e.modified) })));
  }
}

async function preview(e) {
  const box = $('asset-preview');
  box.textContent = '';
  box.append(el('h3', { text: e.name }), el('p', { class: 'hint', text: e.path + ' / ' + (KIND[e.kind] || e.kind) + ' / ' + fmtSize(e.size) }));
  if (!e.preview) { box.append(el('p', { class: 'empty', text: 'この種類のファイルは中身を表示しません。' })); return; }
  const url = '/assets/api/file?root=' + assets.root + '&path=' + encodeURIComponent(e.path);
  try {
    const res = await fetch(url, { credentials: 'same-origin' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const type = res.headers.get('content-type') || '';
    if (type.startsWith('image/')) {
      const blob = await res.blob();
      box.append(el('img', { src: URL.createObjectURL(blob), alt: e.name }));
    } else {
      const text = await res.text();
      box.append(el('pre', { class: 'log', text: text.length > 200000 ? text.slice(0, 200000) + '\n…' : text }));
    }
  } catch (err) { box.append(el('p', { class: 'empty', text: '読めません: ' + err.message })); }
}

// ---- remote play (the play plugin: spec/feature/tool/play.md) ---------------

const play = { targets: [], ws: null, pc: null, watching: '', logNext: 0, logTarget: '', retry: 0, lastMove: 0 };
const STATE_TEXT = { idle: '停止中', building: 'ビルド中', running: '実行中', failed: '失敗' };

// KeyboardEvent.code -> the plugin's key names (key_names.h).
function keyName(code) {
  if (/^Key[A-Z]$/.test(code)) return code.slice(3);
  if (/^Digit[0-9]$/.test(code)) return code.slice(5);
  if (/^F([1-9]|1[0-2])$/.test(code)) return code;
  const named = {
    Enter: 'Enter', NumpadEnter: 'Enter', Escape: 'Escape', Space: 'Space', Tab: 'Tab', Backspace: 'Backspace',
    ShiftLeft: 'Shift', ShiftRight: 'Shift', ControlLeft: 'Ctrl', ControlRight: 'Ctrl', AltLeft: 'Alt', AltRight: 'Alt',
    ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right', Insert: 'Insert', Delete: 'Delete',
    Home: 'Home', End: 'End', PageUp: 'PageUp', PageDown: 'PageDown', CapsLock: 'CapsLock', Minus: 'Minus',
    Equal: 'Equal', BracketLeft: 'BracketLeft', BracketRight: 'BracketRight', Semicolon: 'Semicolon', Quote: 'Quote',
    Comma: 'Comma', Period: 'Period', Slash: 'Slash', Backslash: 'Backslash', Backquote: 'Backquote',
  };
  return named[code] || '';
}

function playSend(msg) {
  if (play.ws && play.ws.readyState === WebSocket.OPEN) play.ws.send(JSON.stringify(msg));
}

function currentTarget() { return play.targets.find((t) => t.name === $('play-target').value); }

let startingScenePreview = false;
let activePreviewTarget = '';
function sceneTargets() {
  const select = $('scene-select');
  const app = select.selectedOptions[0]?.dataset.app;
  // The run endpoint resolves by name. Suppress ambiguous names across profiles.
  return play.targets.filter(t => t.scenePreview && t.scenePreview.app === app &&
    t.scenePreview.scene === select.selectedOptions[0]?.dataset.runtimeId && play.targets.filter(other => other.name === t.name).length === 1);
}

function updateSceneActions() {
  const targets = sceneTargets();
  if (activePreviewTarget && !targets.some(t => t.name === activePreviewTarget)) stopWatching();
  const drivers = targets.filter(t => t.scenePreview.mode === 'driver');
  const lighting = targets.filter(t => t.scenePreview.mode === 'lighting');
  const select = $('scene-state');
  const keep = select.value;
  select.replaceChildren(...lighting.map(t => new Option(t.scenePreview.state || t.label, t.name)));
  if (lighting.some(t => t.name === keep)) select.value = keep;
  $('scene-state-label').hidden = lighting.length < 2;
  const unavailable = !play.ws || play.ws.readyState !== WebSocket.OPEN || startingScenePreview;
  $('scene-run').disabled = unavailable || drivers.length !== 1;
  $('scene-lighting').disabled = unavailable || lighting.length === 0;
  $('scene-run').title = drivers.length === 1 ? drivers[0].label : 'シーンドライバを一つ登録してください';
  $('scene-lighting').title = lighting.length ? select.selectedOptions[0].text : '実シェーダを使う表示対象が未登録です';
}

async function openScenePreview(mode) {
  if (startingScenePreview) return;
  const targets = sceneTargets();
  const target = targets.find(t => t.scenePreview.mode === mode &&
    (mode === 'driver' || t.name === $('scene-state').value));
  if (!target) return;
  if (targets.some(t => t.name !== target.name && ['building', 'running'].includes(t.state))) {
    toast('先に実行中のシーン対象を停止してください', true);
    return;
  }
  if (target.state === 'building') { toast('ビルド完了後に選んでください', true); return; }
  startingScenePreview = true;
  updateSceneActions();
  try {
    if (currentTarget()?.name !== target.name) stopWatching();
    $('play-target').value = target.name;
    renderPlayState();
    if (target.state !== 'running') {
      await api('/play/api/targets/' + encodeURIComponent(target.name) + '/run', {body:{}});
    }
    if (sceneTargets().some(t => t.name === target.name) && currentTarget()?.name === target.name) {
      activePreviewTarget = target.name;
      if (!play.watching) await watchPlayStream();
    }
  } catch (err) {
    toast(target.label + ': ' + err.message, true);
  } finally {
    startingScenePreview = false;
    await loadTargets();
  }
}

function renderPlayState() {
  updateSceneActions();
  const t = currentTarget();
  $('play-missing').hidden = play.targets.length > 0;
  for (const id of ['play-build', 'play-run', 'play-stop', 'play-watch']) $(id).disabled = !t;
  const pill = $('play-state');
  if (!t) { pill.textContent = '-'; pill.className = 'pill'; return; }
  const busy = t.state === 'building' || t.state === 'running';
  $('play-build').disabled = !t.can_build || busy;
  $('play-run').disabled = busy;
  $('play-stop').disabled = !busy;
  pill.textContent = (STATE_TEXT[t.state] || t.state) + (t.exit_code != null && !busy ? ' (終了コード ' + t.exit_code + ')' : '');
  pill.className = 'pill' + (t.state === 'running' ? ' ok' : t.state === 'failed' ? ' bad' : '');
  const box = $('play-buttons');
  box.replaceChildren();
  for (const b of t.buttons) {
    const btn = el('button', { text: b.label });
    btn.addEventListener('pointerdown', (e) => { e.preventDefault(); playSend({ type: 'key', key: b.key, down: true }); });
    for (const ev of ['pointerup', 'pointerleave']) btn.addEventListener(ev, () => playSend({ type: 'key', key: b.key, down: false }));
    box.append(btn);
  }
}

async function loadTargets() {
  try {
    const data = await api('/play/api/targets');
    const keep = $('play-target').value;
    play.targets = data.targets || [];
    const sel = $('play-target');
    sel.replaceChildren(...play.targets.map((t) => el('option', { value: t.name, text: t.label })));
    if (play.targets.some((t) => t.name === keep)) sel.value = keep;
    for (const e of data.errors || []) toast('プロフィールの play を読めません: ' + e, true);
  } catch (e) { toast('遊べるゲームの一覧を読めません: ' + e.message, true); }
  renderPlayState();
}

async function targetAction(action) {
  const t = currentTarget();
  if (!t) return;
  try {
    await api('/play/api/targets/' + encodeURIComponent(t.name) + '/' + action, { body: {} });
    if (action === 'stop' && activePreviewTarget === t.name) stopWatching();
  } catch (e) { toast(t.label + ': ' + e.message, true); }
  loadTargets();
}

async function pollLogs() {
  const t = currentTarget();
  if (t) {
    if (play.logTarget !== t.name) { play.logTarget = t.name; play.logNext = 0; $('play-log').textContent = ''; }
    try {
      const data = await api('/play/api/targets/' + encodeURIComponent(t.name) + '/logs?since=' + play.logNext);
      if (data.lines.length) {
        const pre = $('play-log');
        const atEnd = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 4;
        const lines = (pre.textContent ? pre.textContent.split('\n') : []).concat(data.lines.map((l) => l.text));
        pre.textContent = lines.slice(-2000).join('\n');
        if (atEnd) pre.scrollTop = pre.scrollHeight;
      }
      play.logNext = data.next;
    } catch { /* the next poll retries */ }
  }
  setTimeout(pollLogs, 1000);
}

function connectPlay() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  play.ws = new WebSocket(proto + '//' + location.host + '/play/ws');
  play.ws.addEventListener('open', () => { play.retry = 0; updateSceneActions(); });
  play.ws.addEventListener('message', async (ev) => {
    let m;
    try { m = JSON.parse(ev.data); } catch { return; }
    if (m.type === 'answer' && play.pc) {
      try { await play.pc.setRemoteDescription({ type: 'answer', sdp: m.sdp }); } catch (e) { toast('映像をつなげません: ' + e.message, true); }
    } else if (m.type === 'state') {
      const t = play.targets.find((x) => x.name === m.target);
      if (t) { t.state = m.state; t.exit_code = m.exit_code; renderPlayState(); }
    } else if (m.type === 'error') {
      if (m.error === 'stream_ended') stopWatching();
      toast('遠隔プレイ: ' + (m.message || m.error), true);
    }
  });
  play.ws.addEventListener('close', () => {
    stopWatching();
    updateSceneActions();
    play.retry = Math.min(play.retry + 1, 6);
    setTimeout(connectPlay, 500 * play.retry);
  });
}

let streamRevision = 0;
function stopWatching() {
  ++streamRevision;
  activePreviewTarget = '';
  if (play.pc) { play.pc.close(); play.pc = null; }
  play.watching = '';
  $('play-video').srcObject = null;
  $('play-stage').classList.remove('live-game');
  $('play-note').hidden = false;
  $('play-watch').textContent = '映像を見る';
}

async function watchPlayStream() {
  const t = currentTarget();
  if (!t) return;
  if (play.watching) {   // the button toggles
    stopWatching();
    if (play.ws) play.ws.close();   // the plugin ends the viewer with the socket; it reconnects
    return;
  }
  const revision = ++streamRevision;
  if (play.pc) play.pc.close();
  try {
    const ice = await api('/play/api/ice');
    if (streamRevision !== revision) return;
    const pc = new RTCPeerConnection({ iceServers: ice.iceServers || [] });
    play.pc = pc;
    const tr = pc.addTransceiver('video', { direction: 'recvonly' });
    const caps = RTCRtpReceiver.getCapabilities && RTCRtpReceiver.getCapabilities('video');
    if (caps && tr.setCodecPreferences) {
      const h264 = caps.codecs.filter((c) => /h264/i.test(c.mimeType));
      if (h264.length) tr.setCodecPreferences(h264);
    }
    pc.addEventListener('track', (e) => {
      if (play.pc !== pc) return;
      $('play-stage').classList.add('live-game');
      $('play-video').srcObject = e.streams[0] || new MediaStream([e.track]);
      $('play-note').hidden = true;
    });
    await pc.setLocalDescription(await pc.createOffer());
    // The Studio takes one offer with its candidates (no trickle): wait for gathering.
    await new Promise((resolve) => {
      if (pc.iceGatheringState === 'complete') return resolve();
      const done = () => { if (pc.iceGatheringState === 'complete') resolve(); };
      pc.addEventListener('icegatheringstatechange', done);
      setTimeout(resolve, 3000);
    });
    if (play.pc !== pc || currentTarget()?.name !== t.name) return;
    if (play.ws?.readyState !== WebSocket.OPEN) throw new Error('映像接続が切れました');
    play.watching = t.name;
    $('play-watch').textContent = '映像を止める';
    playSend({ type: 'offer', target: t.name, sdp: pc.localDescription.sdp });
  } catch (e) {
    if (streamRevision !== revision) return;
    stopWatching();
    toast('映像をつなげません: ' + e.message, true);
  }
}

// Pointer position as 0..1 of the picture (the video keeps its aspect ratio).
function videoPoint(e, clamp) {
  const v = $('play-video');
  const r = v.getBoundingClientRect();
  const vw = v.videoWidth || r.width, vh = v.videoHeight || r.height;
  const scale = Math.min(r.width / vw, r.height / vh);
  const w = vw * scale, h = vh * scale;
  const x = (e.clientX - r.left - (r.width - w) / 2) / w;
  const y = (e.clientY - r.top - (r.height - h) / 2) / h;
  if (clamp) return { x: Math.min(1, Math.max(0, x)), y: Math.min(1, Math.max(0, y)) };
  if (x < 0 || x > 1 || y < 0 || y > 1) return null;
  return { x, y };
}

function bindPlayInput() {
  const stage = $('play-stage');
  const video = $('play-video');
  const onKey = (down) => (e) => {
    if (!play.watching) return;
    const key = keyName(e.code);
    if (!key) return;
    e.preventDefault();
    if (down && e.repeat) return;   // the game sees the key held, not repeated
    playSend({ type: 'key', key, down });
  };
  stage.addEventListener('keydown', onKey(true));
  stage.addEventListener('keyup', onKey(false));
  video.addEventListener('contextmenu', (e) => e.preventDefault());
  video.addEventListener('pointerdown', (e) => {
    stage.focus();
    const p = videoPoint(e);
    if (!play.watching || !p) return;
    video.setPointerCapture(e.pointerId);
    playSend({ type: 'pointer', x: p.x, y: p.y, button: e.button, phase: 'down' });
  });
  video.addEventListener('pointerup', (e) => {
    const p = videoPoint(e, true);   // a release outside the picture still lets go
    if (!play.watching || !p) return;
    playSend({ type: 'pointer', x: p.x, y: p.y, button: e.button, phase: 'up' });
  });
  video.addEventListener('pointermove', (e) => {
    const now = performance.now();
    if (!play.watching || now - play.lastMove < 16) return;
    const p = videoPoint(e);
    if (!p) return;
    play.lastMove = now;
    playSend({ type: 'pointer', x: p.x, y: p.y, button: 0, phase: 'move' });
  });
  video.addEventListener('wheel', (e) => {
    const p = videoPoint(e);
    if (!play.watching || !p || !e.deltaY) return;
    e.preventDefault();
    const notches = -Math.sign(e.deltaY) * Math.max(1, Math.min(10, Math.round(Math.abs(e.deltaY) / 100)));
    playSend({ type: 'wheel', x: p.x, y: p.y, delta: notches });
  }, { passive: false });
}

$('scene-select').addEventListener('change', () => { stopWatching(); updateSceneActions(); });
$('scene-select').addEventListener('scene-list', updateSceneActions);
$('scene-state').addEventListener('change', updateSceneActions);
$('scene-run').addEventListener('click', () => openScenePreview('driver'));
$('scene-lighting').addEventListener('click', () => openScenePreview('lighting'));
$('play-target').addEventListener('change', () => { stopWatching(); renderPlayState(); });
$('play-build').addEventListener('click', () => targetAction('build'));
$('play-run').addEventListener('click', () => targetAction('run'));
$('play-stop').addEventListener('click', () => targetAction('stop'));
$('play-watch').addEventListener('click', watchPlayStream);

// ---- start -----------------------------------------------------------------

async function start() {
  bindPlayInput();
  await loadTargets();
  connectPlay();
  pollLogs();
  loadServices();
  loadPresets();
  connect();
}

start();
