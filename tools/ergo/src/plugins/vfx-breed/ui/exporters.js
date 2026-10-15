// Ways out of vfx-breed: live preview in the engine via the particle
// plugin's WS, JSON download, and the PrivateGame emitter export.

import { requestKsEmitter } from "./api.js";

let pws = null;
let onLink = () => {};

export function connectParticle(onLinkChange) {
  onLink = onLinkChange ?? onLink;
  try {
    pws = new WebSocket("ws://" + location.host + "/particle/ws");
    pws.onopen  = () => onLink(true);
    pws.onclose = () => { onLink(false); setTimeout(() => connectParticle(), 2000); };
    pws.onerror = () => {};
  } catch {
    onLink(false);
    setTimeout(() => connectParticle(), 2000);
  }
}

/** Replace the particle plugin's config — every engine client previews it. */
export function sendToParticle(config) {
  if (!pws || pws.readyState !== 1) throw new Error("particle プラグインに接続していません");
  pws.send(JSON.stringify({ op: "replace", config }));
}

function download(name, text) {
  const blob = new Blob([text], { type: "application/json" });
  const a = Object.assign(document.createElement("a"), {
    href: URL.createObjectURL(blob),
    download: name,
  });
  a.click();
  URL.revokeObjectURL(a.href);
}

const fileName = (s) => String(s || "effect").replace(/[\/\s]+/g, "_");

export function downloadConfig(config) {
  download(`${fileName(config.name)}.json`, JSON.stringify(config, null, 2));
}

export async function downloadKsEmitter(config) {
  const emitter = await requestKsEmitter(config);
  download(`${fileName(emitter.name)}.emitter.json`, JSON.stringify(emitter, null, 2));
  return emitter.name;
}
