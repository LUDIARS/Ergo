/*
 * Electron main-process entry for the unified ergo developer tool.
 *
 * Boots the shared Hono/WS server (same one that `npm run serve` uses)
 * on localhost:5170 and opens a BrowserWindow pointed at it. Engine
 * clients (AdventureCube via ergo_particle / ergo_bind) connect to the
 * very same port, so the app is a drop-in replacement for the browser
 * workflow — you just get a native window instead of opening Chrome.
 *
 * CommonJS entry on purpose: avoids the ESM-in-Electron-main crossover
 * and dynamic-imports the ESM server bundle produced by `tsc`.
 */

const { app, BrowserWindow, shell } = require("electron");
const path = require("node:path");

const PORT = Number(process.env.PORT) || 5170;

let mainWindow = null;

function createWindow() {
    mainWindow = new BrowserWindow({
        width:  1280,
        height: 820,
        title:  "ergo",
        autoHideMenuBar: true,
        backgroundColor: "#0e1116",
        webPreferences: {
            contextIsolation: true,
            nodeIntegration:  false,
        },
    });
    mainWindow.loadURL(`http://localhost:${PORT}/`);

    // Route external `target="_blank"` links through the OS browser so
    // /api/health and similar links don't spawn a bare Chromium window.
    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
        void shell.openExternal(url);
        return { action: "deny" };
    });
}

app.whenReady().then(async () => {
    // Dynamically import the ESM server build. `npm start` runs
    // `tsc` first so dist/ exists when we get here.
    const { boot }             = await import(pathToFileUrl("dist/core/server.js"));
    const { PLUGIN_FACTORIES } = await import(pathToFileUrl("dist/core/registry.js"));

    boot({ port: PORT, factories: PLUGIN_FACTORIES });

    // Give the listener a tick to bind before the renderer tries to
    // fetch `/api/plugins`.
    setTimeout(createWindow, 150);

    app.on("activate", () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on("window-all-closed", () => {
    // Quit on all platforms when the window closes — the server's sole
    // purpose is to back the UI.
    app.quit();
});

// Node's dynamic `import()` wants file URLs on Windows, not bare paths.
function pathToFileUrl(relative) {
    const abs = path.resolve(__dirname, "..", relative);
    return require("node:url").pathToFileURL(abs).href;
}
