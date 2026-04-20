// Shell UI — fetches the plugin list and renders the sidebar.
// Clicking a plugin swaps the iframe to its root URL. Health polling
// paints a small "clients" badge next to each entry every 2 seconds.

const nav    = document.getElementById("plugin-nav");
const viewer = document.getElementById("viewer");

let plugins = [];

async function fetchPlugins() {
    const r = await fetch("/api/plugins");
    const j = await r.json();
    plugins = j.plugins ?? [];
    render();
    applyInitialRoute();
}

function render() {
    nav.innerHTML = "";
    if (plugins.length === 0) {
        const p = document.createElement("div");
        p.className = "empty";
        p.textContent = "no plugins registered";
        nav.appendChild(p);
        return;
    }
    for (const plug of plugins) {
        const a = document.createElement("a");
        a.className = "plugin";
        a.dataset.id = plug.id;
        a.innerHTML = `
            <span class="badge" data-role="badge">&nbsp;</span>
            <div class="title">${plug.icon ? plug.icon + " " : ""}${escapeHtml(plug.title)}</div>
            <div class="desc">${escapeHtml(plug.description || "")}</div>`;
        a.onclick = (e) => {
            e.preventDefault();
            activate(plug.id);
        };
        nav.appendChild(a);
    }
}

function activate(id) {
    const plug = plugins.find((p) => p.id === id);
    if (!plug) return;
    viewer.src = plug.url;
    history.replaceState(null, "", `/#/${id}`);
    for (const el of nav.querySelectorAll(".plugin")) {
        el.classList.toggle("active", el.dataset.id === id);
    }
}

function applyInitialRoute() {
    const hash = location.hash.replace(/^#\/?/, "");
    if (hash && plugins.some((p) => p.id === hash)) {
        activate(hash);
    } else if (plugins.length > 0) {
        activate(plugins[0].id);
    }
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => (
        { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]
    ));
}

async function pollHealth() {
    try {
        const r = await fetch("/api/health");
        const j = await r.json();
        for (const p of j.plugins ?? []) {
            const el = nav.querySelector(`.plugin[data-id="${p.id}"] [data-role="badge"]`);
            if (!el) continue;
            const clients = typeof p.clients === "number" ? p.clients : 0;
            el.textContent = `● ${clients}`;
            el.className = `badge ${p.ok ? (clients > 0 ? "ok" : "") : "err"}`;
        }
    } catch { /* server restart, ignore */ }
}

fetchPlugins();
setInterval(pollHealth, 2000);
