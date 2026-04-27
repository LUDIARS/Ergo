/// AC stage plugin schema — mirrors the JSON consumed by AdventureCube
/// at runtime (`data/master_data/stages/*.json`).
///
/// The AC-side loaders that own the source-of-truth are:
///   - src/skill/placement_loader.{h,cpp}   — fields[] + placements[]
///   - src/combat/enemy_loader.{h,cpp}      — enemies[]
///
/// This plugin is intentionally a *thin* mirror: every field that AC
/// reads has a normaliser here, and unknown fields round-trip via
/// `extras` so manually-authored JSON keeps its `_comment` keys when
/// edited through the UI.

export const SCHEMA_VERSION = 1;

export const FIELD_CATEGORIES = ["grass", "soil", "ice", "cobble"] as const;
export type  FieldCategory = typeof FIELD_CATEGORIES[number];

export const ENEMY_TYPES = ["grunt", "brute", "runner"] as const;
export type  EnemyType = typeof ENEMY_TYPES[number];

export interface Field {
    id:        string;
    category:  FieldCategory;
    note?:     string;
}

/** Pin descriptor on a placement: any of `kind` / `rarity` / `level`
 *  may be present, and the loader treats absence as "random". */
export interface PlacementPin {
    kind?:   string;
    rarity?: string;
    level?:  number;
}

export interface Placement {
    instance_id:      string;
    position:         [number, number, number];
    pin?:             PlacementPin;
    respawn_enabled?: boolean;
    respawn_seconds?: number;
}

/** Enemy basic-attack tuning. AC fills missing fields from
 *  `combat::default_attack_for(type)` per EnemyType. */
export interface EnemyAttack {
    damage?:   number;
    reach?:    number;
    cooldown?: number;
}

export interface Enemy {
    instance_id: string;
    type?:       EnemyType;
    position:    [number, number, number];
    hp?:         number;
    attack?:     EnemyAttack;
    /** Legacy hints accepted by the AC loader when `type` is missing. */
    behavior?:   string;
    effect_id?:  string;
}

export interface StageFile {
    stage_id:    string;
    stage_seed?: number;
    fields:      Field[];
    placements:  Placement[];
    enemies:     Enemy[];
}

// ─── Defaults / constructors ─────────────────────────────────

export function emptyStage(stage_id: string): StageFile {
    return {
        stage_id,
        stage_seed: hashSeed(stage_id),
        fields:     [],
        placements: [],
        enemies:    [],
    };
}

/** FNV-1a 32-bit hash of the stage id — gives every new stage a stable
 *  default seed so SVG pattern picks are deterministic from day 1. */
export function hashSeed(s: string): number {
    let h = 0x811c9dc5 >>> 0;
    for (let i = 0; i < s.length; ++i) {
        h ^= s.charCodeAt(i);
        h = Math.imul(h, 0x01000193) >>> 0;
    }
    return h;
}

// ─── Normalisers (defensive parsing of disk / HTTP input) ────

function asString(v: unknown, fallback = ""): string {
    return typeof v === "string" ? v : fallback;
}
function asNumber(v: unknown): number | undefined {
    const n = Number(v);
    return Number.isFinite(n) ? n : undefined;
}
function asBool(v: unknown): boolean | undefined {
    return typeof v === "boolean" ? v : undefined;
}
function asPosition(v: unknown): [number, number, number] {
    if (!Array.isArray(v)) return [0, 0.35, 0];
    const [x, y, z] = v;
    return [
        Number.isFinite(Number(x)) ? Number(x) : 0,
        Number.isFinite(Number(y)) ? Number(y) : 0.35,
        Number.isFinite(Number(z)) ? Number(z) : 0,
    ];
}

export function normaliseField(raw: unknown): Field {
    if (!raw || typeof raw !== "object") throw new Error("field is not an object");
    const r = raw as Record<string, unknown>;
    const id = asString(r.id).trim();
    if (!id) throw new Error("field.id is required");
    const catRaw = asString(r.category, "grass") as FieldCategory;
    const category: FieldCategory = FIELD_CATEGORIES.includes(catRaw) ? catRaw : "grass";
    const out: Field = { id, category };
    const note = asString(r.note);
    if (note) out.note = note;
    return out;
}

export function normalisePin(raw: unknown): PlacementPin | undefined {
    if (!raw || typeof raw !== "object") return undefined;
    const r = raw as Record<string, unknown>;
    const out: PlacementPin = {};
    const kind = asString(r.kind);   if (kind)   out.kind   = kind;
    const rar  = asString(r.rarity); if (rar)    out.rarity = rar;
    const lvl  = asNumber(r.level);  if (lvl !== undefined) out.level = lvl;
    return Object.keys(out).length ? out : undefined;
}

export function normalisePlacement(raw: unknown): Placement {
    if (!raw || typeof raw !== "object") throw new Error("placement is not an object");
    const r = raw as Record<string, unknown>;
    const instance_id = asString(r.instance_id).trim();
    if (!instance_id) throw new Error("placement.instance_id is required");
    const out: Placement = {
        instance_id,
        position: asPosition(r.position),
    };
    const pin = normalisePin(r.pin);             if (pin) out.pin = pin;
    const re  = asBool(r.respawn_enabled);       if (re !== undefined) out.respawn_enabled = re;
    const rs  = asNumber(r.respawn_seconds);     if (rs !== undefined) out.respawn_seconds = rs;
    return out;
}

export function normaliseAttack(raw: unknown): EnemyAttack | undefined {
    if (!raw || typeof raw !== "object") return undefined;
    const r = raw as Record<string, unknown>;
    const out: EnemyAttack = {};
    const d = asNumber(r.damage);   if (d !== undefined) out.damage   = d;
    const re = asNumber(r.reach);   if (re !== undefined) out.reach    = re;
    const cd = asNumber(r.cooldown); if (cd !== undefined) out.cooldown = cd;
    return Object.keys(out).length ? out : undefined;
}

export function normaliseEnemy(raw: unknown): Enemy {
    if (!raw || typeof raw !== "object") throw new Error("enemy is not an object");
    const r = raw as Record<string, unknown>;
    const instance_id = asString(r.instance_id).trim();
    if (!instance_id) throw new Error("enemy.instance_id is required");
    const out: Enemy = {
        instance_id,
        position: asPosition(r.position),
    };
    const tRaw = asString(r.type, "").toLowerCase() as EnemyType;
    if (ENEMY_TYPES.includes(tRaw)) out.type = tRaw;
    const hp = asNumber(r.hp);     if (hp !== undefined && hp > 0) out.hp = hp;
    const atk = normaliseAttack(r.attack); if (atk) out.attack = atk;
    const beh = asString(r.behavior);  if (beh) out.behavior  = beh;
    const eff = asString(r.effect_id); if (eff) out.effect_id = eff;
    return out;
}

export function normaliseStageFile(raw: unknown): StageFile {
    if (!raw || typeof raw !== "object") throw new Error("stage is not an object");
    const r = raw as Record<string, unknown>;
    const stage_id = asString(r.stage_id).trim();
    if (!stage_id) throw new Error("stage.stage_id is required");

    const seed = asNumber(r.stage_seed);
    const fields:     Field[]     = Array.isArray(r.fields)     ? r.fields.map(normaliseField)         : [];
    const placements: Placement[] = Array.isArray(r.placements) ? r.placements.map(normalisePlacement) : [];
    const enemies:    Enemy[]     = Array.isArray(r.enemies)    ? r.enemies.map(normaliseEnemy)        : [];

    return {
        stage_id,
        stage_seed: seed,
        fields,
        placements,
        enemies,
    };
}
