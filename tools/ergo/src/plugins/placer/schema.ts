/// Placer plugin schema — block-based level design.
///
/// Domain model:
///   - A **Block** is a 3×10 or 5×10 grid; each Cell holds 0+ PlacedObjects.
///   - A **Stage** is an ordered list of Block IDs.
///   - An **Enemy** carries a SkillBlock reference + level (authored
///     independently of blocks so the same enemy can appear in many).
///   - A **SkillBlock** is a bag of skill identifiers; Enemies and
///     SkillBox placements both reference it.
///   - A **Cell** object has a typed discriminant (`Enemy` / `SkillBlock`
///     / `SkillBox` / `Special`) so the UI can render a focused form.
///
/// Deliberately tool-agnostic: skill IDs are free-form strings so this
/// plugin can author levels for any game built on the Actio / Cernere
/// skill catalog, not only AdventureCube.

export const SCHEMA_VERSION = 2;

/** Vertical height a block may take. Horizontal is always 10. */
export type BlockRows = 3 | 5;
export const BLOCK_COLS = 10 as const;
export const BLOCK_ROWS_ALLOWED: readonly BlockRows[] = [3, 5] as const;

// ─── PlacedObject — typed variants (v2) ──────────────────────

/** Allowed discriminants for `PlacedObject.type`. */
export const PLACED_OBJECT_TYPES = ["Enemy", "SkillBlock", "SkillBox", "Special"] as const;
export type PlacedObjectType = typeof PLACED_OBJECT_TYPES[number];

/** How a SkillBox decides the level of each drop. */
export type SkillBoxLevelMode =
    | { kind: "random" }                       // 1..5 均等
    | { kind: "random_in_set"; levels: number[] } // 1..5 の部分集合からランダム
    | { kind: "fixed"; level: number };           // 固定レベル

export interface PlacedObjectEnemy {
    type:     "Enemy";
    enemyId:  string;       // Store.enemies[*].id
    /** 表示ラベル (UI キャッシュ). 保存時は source-of-truth から再計算. */
    label?:   string;
}

export interface PlacedObjectSkillBlock {
    type:         "SkillBlock";
    skillBlockId: string;   // Store.skillBlocks[*].id
    label?:       string;
}

export interface PlacedObjectSkillBox {
    type:     "SkillBox";
    /** 出現し得るスキル ID (複数選択). 空なら「任意」。 */
    skills:   string[];
    level:    SkillBoxLevelMode;
    label?:   string;
}

export interface PlacedObjectSpecial {
    type:    "Special";
    /** TBD. 自由な payload. UI は JSON 編集で扱う. */
    payload?: Record<string, unknown>;
    label?:   string;
}

export type PlacedObject =
    | PlacedObjectEnemy
    | PlacedObjectSkillBlock
    | PlacedObjectSkillBox
    | PlacedObjectSpecial;

export interface Cell {
    /** 0+ objects; more than one is a "combination" (rendered as A+B+...) */
    objects: PlacedObject[];
}

/// Trigger for when a block becomes active in a stage.
export interface BlockTrigger {
    /**
     *   "start"  — active from stage start (default)
     *   "time"   — after `value` seconds from stage start
     *   "after"  — after block with id == `value` is cleared
     *   "score"  — after the player score reaches `value`
     *   "manual" — host-side decides
     */
    kind: "start" | "time" | "after" | "score" | "manual";
    value?: string | number;
}

export interface Block {
    id:   string;
    name: string;
    rows: BlockRows;
    cols: typeof BLOCK_COLS;
    /** grid[row][col] */
    grid: Cell[][];

    appears:  BlockTrigger;
    /** Free-form tag for "what appears in this block" (e.g. "enemies", "reward"). */
    contents: string;
    /** TBD per spec — parked as free-form JSON for now. */
    special?: Record<string, unknown>;
}

export interface Stage {
    id:      string;
    name:    string;
    blocks:  string[];   // ordered Block IDs; repeating an id is allowed.
}

// ─── Enemy / SkillBlock — new top-level sheets ───────────────

export interface Enemy {
    id:           string;
    name:         string;
    /** 所持する SkillBlock — `Store.skillBlocks[*].id` を指す. */
    skillBlockId: string;
    /** 1..5 整数. 将来広げる可能性があるので min/max は validation 側で. */
    level:        number;
    /** 任意 notes / HP / スプライト等. */
    notes?:       string;
}

export interface SkillBlock {
    id:     string;
    name:   string;
    /** 含有スキルの ID 一覧 (free-form). Foundation スキルカタログの ID を想定. */
    skills: string[];
    notes?: string;
}

// ─── Store ───────────────────────────────────────────────────

export interface Store {
    version:     number;
    blocks:      Block[];
    stages:      Stage[];
    enemies:     Enemy[];
    skillBlocks: SkillBlock[];
}

// ─── Helpers / constructors ──────────────────────────────────

export function emptyGrid(rows: BlockRows): Cell[][] {
    const g: Cell[][] = [];
    for (let r = 0; r < rows; ++r) {
        const row: Cell[] = [];
        for (let c = 0; c < BLOCK_COLS; ++c) row.push({ objects: [] });
        g.push(row);
    }
    return g;
}

export function resizeGrid(grid: Cell[][], nextRows: BlockRows): Cell[][] {
    const out = emptyGrid(nextRows);
    for (let r = 0; r < nextRows && r < grid.length; ++r) {
        const src = grid[r];
        if (!src) continue;
        for (let c = 0; c < BLOCK_COLS && c < src.length; ++c) {
            const srcCell = src[c];
            if (srcCell) out[r]![c] = { objects: [...srcCell.objects] };
        }
    }
    return out;
}

export function makeBlock(id: string, rows: BlockRows = 3): Block {
    return {
        id,
        name:     id,
        rows,
        cols:     BLOCK_COLS,
        grid:     emptyGrid(rows),
        appears:  { kind: "start" },
        contents: "",
    };
}

export function makeStage(id: string): Stage {
    return { id, name: id, blocks: [] };
}

export function makeEnemy(id: string): Enemy {
    return { id, name: id, skillBlockId: "", level: 1 };
}

export function makeSkillBlock(id: string): SkillBlock {
    return { id, name: id, skills: [] };
}

export function emptyStore(): Store {
    return {
        version:     SCHEMA_VERSION,
        blocks:      [],
        stages:      [],
        enemies:     [],
        skillBlocks: [],
    };
}

// ─── Normalisation (runtime validation of disk / HTTP input) ─

export function normaliseStore(raw: unknown): Store {
    if (!raw || typeof raw !== "object") throw new Error("store is not an object");
    const r = raw as Record<string, unknown>;
    const out: Store = emptyStore();
    out.version = typeof r.version === "number" ? r.version : SCHEMA_VERSION;

    if (Array.isArray(r.skillBlocks)) {
        for (const s of r.skillBlocks) out.skillBlocks.push(normaliseSkillBlock(s));
    }
    if (Array.isArray(r.enemies)) {
        for (const e of r.enemies) out.enemies.push(normaliseEnemy(e));
    }
    if (Array.isArray(r.blocks)) {
        for (const b of r.blocks) out.blocks.push(normaliseBlock(b));
    }
    if (Array.isArray(r.stages)) {
        for (const s of r.stages) out.stages.push(normaliseStage(s));
    }

    // Cross-reference pruning.
    const blockIds = new Set(out.blocks.map((b) => b.id));
    for (const s of out.stages) {
        s.blocks = s.blocks.filter((bid) => blockIds.has(bid));
    }
    const skillBlockIds = new Set(out.skillBlocks.map((sb) => sb.id));
    for (const e of out.enemies) {
        if (e.skillBlockId && !skillBlockIds.has(e.skillBlockId)) {
            console.warn(`[placer] enemy ${e.id} references missing skillBlock ${e.skillBlockId}; clearing.`);
            e.skillBlockId = "";
        }
    }
    return out;
}

export function normaliseBlock(raw: unknown): Block {
    if (!raw || typeof raw !== "object") throw new Error("block is not an object");
    const r = raw as Record<string, unknown>;
    const id = String(r.id ?? "").trim();
    if (!id) throw new Error("block.id is required");

    const rowsIn = Number(r.rows);
    const rows: BlockRows = rowsIn === 5 ? 5 : 3;

    const grid = normaliseGrid(r.grid, rows);
    const appears = normaliseTrigger(r.appears);
    return {
        id,
        name:     String(r.name ?? id),
        rows,
        cols:     BLOCK_COLS,
        grid,
        appears,
        contents: String(r.contents ?? ""),
        special:  r.special && typeof r.special === "object" ? (r.special as Record<string, unknown>) : undefined,
    };
}

export function normaliseTrigger(raw: unknown): BlockTrigger {
    if (!raw || typeof raw !== "object") return { kind: "start" };
    const r = raw as Record<string, unknown>;
    const kind = String(r.kind ?? "start");
    const allowed: BlockTrigger["kind"][] = ["start", "time", "after", "score", "manual"];
    const k = allowed.includes(kind as BlockTrigger["kind"]) ? (kind as BlockTrigger["kind"]) : "start";
    const t: BlockTrigger = { kind: k };
    if (r.value !== undefined && r.value !== null && r.value !== "") {
        t.value = typeof r.value === "number" ? r.value : String(r.value);
    }
    return t;
}

export function normaliseGrid(raw: unknown, rows: BlockRows): Cell[][] {
    const out = emptyGrid(rows);
    if (!Array.isArray(raw)) return out;
    for (let r = 0; r < rows && r < raw.length; ++r) {
        const srcRow = raw[r];
        if (!Array.isArray(srcRow)) continue;
        for (let c = 0; c < BLOCK_COLS && c < srcRow.length; ++c) {
            out[r]![c] = normaliseCell(srcRow[c]);
        }
    }
    return out;
}

export function normaliseCell(raw: unknown): Cell {
    if (!raw || typeof raw !== "object") return { objects: [] };
    const r = raw as Record<string, unknown>;
    if (!Array.isArray(r.objects)) return { objects: [] };
    const objects: PlacedObject[] = [];
    for (const o of r.objects) {
        const normalised = normalisePlacedObject(o);
        if (normalised) objects.push(normalised);
    }
    return { objects };
}

/** Typed discriminator-based normaliser. Unknown types are dropped
 *  (returns `null`). Legacy v1 free-form objects (pre Enemy/SkillBlock)
 *  are rewritten into `Special` with the original payload preserved. */
export function normalisePlacedObject(raw: unknown): PlacedObject | null {
    if (!raw || typeof raw !== "object") return null;
    const r = raw as Record<string, unknown>;
    const rawType = String(r.type ?? "").trim();
    const label = typeof r.label === "string" ? r.label : undefined;

    if (PLACED_OBJECT_TYPES.includes(rawType as PlacedObjectType)) {
        switch (rawType as PlacedObjectType) {
            case "Enemy": {
                const enemyId = String(r.enemyId ?? "").trim();
                if (!enemyId) return null;
                return { type: "Enemy", enemyId, label };
            }
            case "SkillBlock": {
                const skillBlockId = String(r.skillBlockId ?? "").trim();
                if (!skillBlockId) return null;
                return { type: "SkillBlock", skillBlockId, label };
            }
            case "SkillBox": {
                const skills = Array.isArray(r.skills)
                    ? (r.skills as unknown[]).map(String).filter(Boolean)
                    : [];
                return { type: "SkillBox", skills, level: normaliseLevel(r.level), label };
            }
            case "Special": {
                const payload = r.payload && typeof r.payload === "object"
                    ? (r.payload as Record<string, unknown>)
                    : undefined;
                return { type: "Special", payload, label };
            }
        }
    }
    // Legacy v1 fallback — keep free-form payload as Special.
    if (rawType) {
        return {
            type: "Special",
            payload: { legacyType: rawType, legacyFields: r },
            label: label ?? rawType,
        };
    }
    return null;
}

export function normaliseLevel(raw: unknown): SkillBoxLevelMode {
    if (!raw || typeof raw !== "object") return { kind: "random" };
    const r = raw as Record<string, unknown>;
    const k = String(r.kind ?? "random");
    if (k === "fixed") {
        const n = Number(r.level);
        const lv = clampLevel(Number.isFinite(n) ? Math.round(n) : 1);
        return { kind: "fixed", level: lv };
    }
    if (k === "random_in_set") {
        const levels = Array.isArray(r.levels)
            ? Array.from(new Set(
                (r.levels as unknown[]).map((x) => clampLevel(Math.round(Number(x)))),
              )).filter((n) => Number.isFinite(n)).sort((a, b) => a - b)
            : [];
        return { kind: "random_in_set", levels };
    }
    return { kind: "random" };
}

function clampLevel(n: number): number {
    if (!Number.isFinite(n)) return 1;
    if (n < 1) return 1;
    if (n > 5) return 5;
    return n;
}

export function normaliseStage(raw: unknown): Stage {
    if (!raw || typeof raw !== "object") throw new Error("stage is not an object");
    const r = raw as Record<string, unknown>;
    const id = String(r.id ?? "").trim();
    if (!id) throw new Error("stage.id is required");
    const blocks: string[] = Array.isArray(r.blocks)
        ? r.blocks.map((b) => String(b)).filter((b) => b.length > 0)
        : [];
    return { id, name: String(r.name ?? id), blocks };
}

export function normaliseEnemy(raw: unknown): Enemy {
    if (!raw || typeof raw !== "object") throw new Error("enemy is not an object");
    const r = raw as Record<string, unknown>;
    const id = String(r.id ?? "").trim();
    if (!id) throw new Error("enemy.id is required");
    const level = clampLevel(Math.round(Number(r.level ?? 1)));
    return {
        id,
        name:         String(r.name ?? id),
        skillBlockId: String(r.skillBlockId ?? ""),
        level,
        notes:        typeof r.notes === "string" ? r.notes : undefined,
    };
}

export function normaliseSkillBlock(raw: unknown): SkillBlock {
    if (!raw || typeof raw !== "object") throw new Error("skillBlock is not an object");
    const r = raw as Record<string, unknown>;
    const id = String(r.id ?? "").trim();
    if (!id) throw new Error("skillBlock.id is required");
    const skills = Array.isArray(r.skills)
        ? Array.from(new Set((r.skills as unknown[]).map(String).filter((s) => s.length > 0)))
        : [];
    return {
        id,
        name:   String(r.name ?? id),
        skills,
        notes:  typeof r.notes === "string" ? r.notes : undefined,
    };
}
