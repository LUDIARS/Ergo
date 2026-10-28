// @implements SPEC-ergo-studio
// Studio のタブ切り替えとプロジェクト選択。各タブは views/<name>.js の render(container)。

import { api, h, onProjectChange, selectedProject, setSelectedProject, toast } from "./lib.js";

const VIEWS = {
  projects:    () => import("./views/projects.js"),
  tasks:       () => import("./views/tasks.js"),
  sources:     () => import("./views/sources.js"),
  resources:   () => import("./views/resources.js"),
  plugins:     () => import("./views/plugins.js"),
  connections: () => import("./views/connections.js"),
};

let activeTab = "projects";
try { activeTab = localStorage.getItem("ergo-studio.tab") || activeTab; } catch { /* noop */ }

async function show(tab) {
  if (!VIEWS[tab]) tab = "projects";
  activeTab = tab;
  try { localStorage.setItem("ergo-studio.tab", tab); } catch { /* noop */ }
  for (const b of document.querySelectorAll("#tabs button")) b.classList.toggle("active", b.dataset.tab === tab);
  const view = document.getElementById("view");
  view.replaceChildren(h("p", { class: "muted" }, "読み込み中…"));
  const mod = await VIEWS[tab]();
  if (activeTab !== tab) return;   // 読み込み中に別タブへ移った
  view.replaceChildren();
  await mod.render(view);
}

async function loadProjects() {
  const select = document.getElementById("project-select");
  select.replaceChildren(h("option", { value: "" }, "(未選択)"));
  try {
    const { projects } = await api("/api/projects");
    for (const p of projects.sort((a, b) => a.code.localeCompare(b.code))) {
      select.append(h("option", { value: p.code }, `${p.code} — ${p.project}`));
    }
    const cur = selectedProject();
    if (cur && projects.some((p) => p.code === cur)) select.value = cur;
    else if (cur) setSelectedProject(null);
    document.getElementById("status-line").textContent = `Cc 登録 ${projects.length} 件`;
  } catch (err) {
    document.getElementById("status-line").textContent = "Concordia に接続できません";
    toast(err.message, true);
  }
}

document.getElementById("tabs").addEventListener("click", (e) => {
  const tab = e.target?.dataset?.tab;
  if (tab) void show(tab);
});
document.getElementById("project-select").addEventListener("change", (e) => setSelectedProject(e.target.value));
onProjectChange(() => void show(activeTab));
window.addEventListener("studio:projects-changed", () => void loadProjects());

await loadProjects();
await show(activeTab);
