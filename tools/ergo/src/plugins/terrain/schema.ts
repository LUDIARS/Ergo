/// Terrain plugin schema — field-based stage ground layout.
///
/// Domain model:
///   - A **Field** is one square ground tile placed in sequence on a stage.
///     Authors only choose a **category** ("grass" / "soil" / "ice" /
///     "cobble"); the concrete visual pattern is picked at runtime from
///     `FIELD_PATTERNS[category]` via a deterministic random so replays
///     stay stable given the same seed.
///   - A **Stage** is an ordered list of Fields. Stage i+1 connects
///     seamlessly after stage i; there is no branching or looping for
///     the MVP (inspired by DWW's SEQUENTIAL StageBehaviour — the
///     simplest of the ScriptableObject hierarchy).
///   - A single **Store** holds every stage.
///
/// Deliberately game-agnostic: this plugin only emits a JSON file. The
/// consumer (AdventureCube today; others later) is responsible for
/// resolving `category -> pattern` and actually rendering the ground.

export const SCHEMA_VERSION = 1;

/** Broad ground categories. Authors pick one per field; the renderer
 *  selects an actual SVG pattern from `FIELD_PATTERNS[category]`. */
export const FIELD_CATEGORIES = ["grass", "soil", "ice", "cobble"] as const;
export type FieldCategory = typeof FIELD_CATEGORIES[number];

/** Human-readable labels and accent colours for the UI. */
export const CATEGORY_META: Record<FieldCategory, { label: string; accent: string; tint: string }> = {
    grass:  { label: "草原", accent: "#6aa84f", tint: "rgba(106,168,79,0.18)"  },
    soil:   { label: "土",   accent: "#8b6239", tint: "rgba(139,98,57,0.18)"   },
    ice:    { label: "氷",   accent: "#a3cfe6", tint: "rgba(163,207,230,0.25)" },
    cobble: { label: "石畳", accent: "#6e6e76", tint: "rgba(110,110,118,0.2)"  },
};

/** Shipping SVG patterns per category. File names live under
 *  `ui/patterns/`; the UI resolves them relative to `staticRoot`. */
export const FIELD_PATTERNS: Record<FieldCategory, readonly string[]> = {
    grass:  ["grass_01.svg", "grass_02.svg", "grass_03.svg"],
    soil:   ["soil_01.svg", "soil_02.svg"],
    ice:    ["ice_01.svg"],
    cobble: ["cobble_01.svg", "cobble_02.svg"],
};

export interface Field {
    /** Id scoped to its parent Stage. Re-using the same id within one
     *  stage is allowed — the UI disambiguates by position. */
    id:       string;
    category: FieldCategory;
    /** Optional author-provided note (e.g. "boss arrival point"). */
    note?:    string;
}

export interface Stage {
    id:     string;
    name:   string;
    /** Ordered left-to-right connection. Index 0 is the start. */
    fields: Field[];
    /** Optional author-level notes. */
    notes?: string;
}

export interface Store {
    version: number;
    stages:  Stage[];
}

// ─── Helpers ────────────────────────────────────────────────

export function makeField(id: string, category: FieldCategory = "grass"): Field {
    return { id, category };
}

export function makeStage(id: string): Stage {
    return { id, name: id, fields: [] };
}

export function emptyStore(): Store {
    return { version: SCHEMA_VERSION, stages: [] };
}

/** Deterministic pattern picker. Given a (stageId, fieldId, category)
 *  triple and an optional seed, return one of `FIELD_PATTERNS[category]`.
 *  Used by consumers (AC runtime, UI preview) so "random pattern" stays
 *  reproducible. */
export function pickPattern(
    category: FieldCategory,
    stageId: string,
    fieldId: string,
    seed = 0,
): string {
    const patterns = FIELD_PATTERNS[category];
    if (patterns.length === 0) return "";
    // FNV-1a 32-bit — good enough for non-cryptographic dispersion.
    let h = 2166136261 ^ seed;
    const mix = (s: string) => {
        for (let i = 0; i < s.length; ++i) {
            h ^= s.charCodeAt(i);
            h = Math.imul(h, 16777619);
        }
    };
    mix(stageId);
    mix("|");
    mix(fieldId);
    mix("|");
    mix(category);
    const idx = (h >>> 0) % patterns.length;
    return patterns[idx]!;
}

// ─── Normalisation ──────────────────────────────────────────

export function normaliseStore(raw: unknown): Store {
    if (!raw || typeof raw !== "object") throw new Error("store is not an object");
    const r = raw as Record<string, unknown>;
    const out: Store = emptyStore();
    out.version = typeof r.version === "number" ? r.version : SCHEMA_VERSION;

    if (Array.isArray(r.stages)) {
        for (const s of r.stages) out.stages.push(normaliseStage(s));
    }
    return out;
}

export function normaliseStage(raw: unknown): Stage {
    if (!raw || typeof raw !== "object") throw new Error("stage is not an object");
    const r = raw as Record<string, unknown>;
    const id = String(r.id ?? "").trim();
    if (!id) throw new Error("stage.id is required");

    const fieldsIn = Array.isArray(r.fields) ? r.fields : [];
    const fields: Field[] = [];
    for (const f of fieldsIn) fields.push(normaliseField(f));

    return {
        id,
        name:   String(r.name ?? id),
        fields,
        notes:  typeof r.notes === "string" ? r.notes : undefined,
    };
}

export function normaliseField(raw: unknown): Field {
    if (!raw || typeof raw !== "object") throw new Error("field is not an object");
    const r = raw as Record<string, unknown>;
    const rawCat = String(r.category ?? "").trim();
    const category: FieldCategory = FIELD_CATEGORIES.includes(rawCat as FieldCategory)
        ? (rawCat as FieldCategory)
        : "grass";
    const id = String(r.id ?? "").trim() || "f_unnamed";
    return {
        id,
        category,
        note: typeof r.note === "string" ? r.note : undefined,
    };
}
