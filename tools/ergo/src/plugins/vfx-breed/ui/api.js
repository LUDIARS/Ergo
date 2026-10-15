// HTTP calls of the vfx-breed UI. The server is stateless; everything a
// generation needs is sent with the request.

async function json(res) {
  const body = await res.json().catch(() => null);
  if (!res.ok || !body || body.ok === false) {
    throw new Error(body?.err ?? `HTTP ${res.status}`);
  }
  return body;
}

export async function fetchSeeds() {
  return json(await fetch("/vfx-breed/api/seeds"));
}

export async function fetchHealth() {
  return json(await fetch("/vfx-breed/api/health"));
}

/** The particle editor's current config (GET /particle/api/effect). */
export async function fetchParticleCurrent() {
  return json(await fetch("/particle/api/effect"));
}

/**
 * @param {{ parents: object[], selected?: number[], direction?: string,
 *           strength: number, seed?: number, count: number, generation: number }} req
 */
export async function requestGeneration(req) {
  return json(await fetch("/vfx-breed/api/generation", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(req),
  }));
}

/** Converted by vfx-suggest's ks_export (single source of the conversion). */
export async function requestKsEmitter(config) {
  const key = (config.name ?? "").startsWith("Effect/") ? config.name : undefined;
  const url = "/vfx-suggest/api/ks-emitter" + (key ? `?key=${encodeURIComponent(key)}` : "");
  const body = await json(await fetch(url, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(config),
  }));
  return body.emitter;
}
