/// Starting points for a breeding run (GET /vfx-breed/api/seeds): the
/// default effect and vfx-suggest's catalog. The particle editor's current
/// value and pasted JSON (e.g. a vfx-suggest result) are picked up by the UI.

import { DEFAULT_EFFECT, type ParticleEffectConfig } from "../particle/schema.js";
import { CATALOG } from "../vfx-suggest/catalog.js";

export interface SeedEntry {
    id:     string;
    name:   string;
    source: "default" | "catalog";
    config: ParticleEffectConfig;
}

export function listSeeds(): SeedEntry[] {
    return [
        { id: "default", name: "既定エフェクト", source: "default", config: DEFAULT_EFFECT },
        ...CATALOG.map((p, i): SeedEntry => ({
            id: `catalog-${i}`,
            name: p.name,
            source: "catalog",
            config: p.config,
        })),
    ];
}
