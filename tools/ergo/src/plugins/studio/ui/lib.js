// @implements SPEC-ergo-studio
// @implements SPEC-ergo-studio-server-side
// Studio 画面の共通部品: API 呼び出し・DOM 生成・通知・選択中プロジェクト。
// サービス由来の文字列は必ず textContent で入れる (HTML として解釈させない)。

const BASE = "/studio";

export async function api(path, { method = "GET", body } = {}) {
  const init = { method, headers: { accept: "application/json" } };
  if (body !== undefined) {
    init.headers["content-type"] = "application/json";
    init.body = JSON.stringify(body);
  }
  const res = await fetch(BASE + path, init);
  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = { message: text }; }
  if (!res.ok) {
    const svc = data?.service ? `${data.service}: ` : "";
    throw new Error(`${svc}${data?.message ?? res.statusText} (${res.status})`);
  }
  return data;
}

/// h("div", { class: "x", onclick: fn }, "text", child...)
export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs ?? {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
    else if (k === "class") el.className = v;
    else if (k === "value") el.value = v;
    else if (v === true) el.setAttribute(k, "");
    else el.setAttribute(k, String(v));
  }
  for (const c of children.flat()) {
    if (c === undefined || c === null || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

export function badge(state, text = state) {
  return h("span", { class: `badge ${state}` }, text);
}

export function panel(title, ...children) {
  return h("section", { class: "panel" }, title ? h("h2", {}, title) : null, ...children);
}

let toastTimer = null;
export function toast(message, isError = false) {
  const el = document.getElementById("toast");
  el.textContent = message;
  el.className = isError ? "toast error" : "toast";
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.hidden = true; }, isError ? 9000 : 4000);
}

/// 失敗を画面内に出す (例外を握り潰さない)。
export function errorBox(err) {
  return h("p", { class: "error" }, err instanceof Error ? err.message : String(err));
}

export function fmtTime(v) {
  if (v === null || v === undefined || v === "") return "—";
  const d = typeof v === "number" ? new Date(v < 1e12 ? v * 1000 : v) : new Date(v);
  return Number.isNaN(d.getTime()) ? String(v) : d.toLocaleString("ja-JP");
}

// ---- 選択中のプロジェクト (タブ間で共有) ----
const listeners = new Set();
let current = null;
try { current = localStorage.getItem("ergo-studio.project"); } catch { /* 保存できない環境 */ }

export function selectedProject() { return current; }
export function setSelectedProject(code) {
  current = code || null;
  try { code ? localStorage.setItem("ergo-studio.project", code) : localStorage.removeItem("ergo-studio.project"); } catch { /* noop */ }
  for (const fn of listeners) fn(current);
}
export function onProjectChange(fn) { listeners.add(fn); return () => listeners.delete(fn); }
