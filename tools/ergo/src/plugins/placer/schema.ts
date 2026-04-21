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

/** SkillBlock の立方体サイズ (エッジ長). 3/5/7/9 の 4 択. */
export type SkillBlockSize = 3 | 5 | 7 | 9;
export const SKILL_BLOCK_SIZES: readonly SkillBlockSize[] = [3, 5, 7, 9] as const;

/** 3D voxel shape. `shape[z][y][x]` は 0/1 の整数. 各軸長 = size. */
export type SkillBlockShape = number[][][];

export interface SkillBlock {
    id:     string;
    name:   string;
    /** 含有 Skill ID 一覧. `SKILL_CATALOG` の `id` (数値文字列) のみが
     *  UI の multi-select から選ばれる前提. 重複は許可 (攻撃系は
     *  同じブロックを集めると実ゲームでレベルアップするため). */
    skills: string[];
    /** 立方体のエッジ長 (3/5/7/9). Default 3. */
    size:   SkillBlockSize;
    /** N×N×N voxel 配列. 1 = 埋まっている, 0 = 空. */
    shape:  SkillBlockShape;
    /** インターバル (cooldown) in 秒. 0 以上. */
    interval: number;
    notes?: string;
}

/** 指定サイズの空の shape を作る (全 0). */
export function emptyShape(size: SkillBlockSize): SkillBlockShape {
    const out: SkillBlockShape = [];
    for (let z = 0; z < size; ++z) {
        const layer: number[][] = [];
        for (let y = 0; y < size; ++y) {
            const row: number[] = new Array(size).fill(0);
            layer.push(row);
        }
        out.push(layer);
    }
    return out;
}

/** 既存 shape を新しいサイズにリサイズする.
 *  重なる領域の値は保持、はみ出しは捨て、足りない領域は 0 埋め. */
export function resizeShape(src: SkillBlockShape, nextSize: SkillBlockSize): SkillBlockShape {
    const out = emptyShape(nextSize);
    const minZ = Math.min(src.length, nextSize);
    for (let z = 0; z < minZ; ++z) {
        const sL = src[z];
        const dL = out[z]!;
        const minY = Math.min(sL?.length ?? 0, nextSize);
        for (let y = 0; y < minY; ++y) {
            const sR = sL?.[y];
            const dR = dL[y]!;
            const minX = Math.min(sR?.length ?? 0, nextSize);
            for (let x = 0; x < minX; ++x) {
                dR[x] = (sR?.[x] ? 1 : 0);
            }
        }
    }
    return out;
}

/** shape の埋まっている voxel 数を数える. */
export function countFilledVoxels(shape: SkillBlockShape): number {
    let n = 0;
    for (const layer of shape)
        for (const row of layer)
            for (const v of row) if (v) ++n;
    return n;
}

// ─── Skill catalog (Issue 追加要求: SkillID は実装済みリストから
//      プルダウン / 番号帯は 攻撃系 100000〜, パッシブ系 500000〜,
//      特殊系 900000〜) ───────────────────────────────────────────

export type SkillCategory = "attack" | "ranged" | "buff" | "special";

export interface SkillDefinition {
    /** 数値文字列. `100000`〜`199999` 攻撃系, `500000`〜 パッシブ系,
     *  `900000`〜 特殊系. 番号は実装順. */
    id:          string;
    /** UI 表示名 (日本語). */
    name:        string;
    /** 分類. UI で見出し/フィルタに使う. */
    category:    SkillCategory;
    /** 短い説明. */
    description: string;
    /** N が効果量 (攻撃力 / マス数 / 延長時間など) に入るパラメトリック
     *  スキルなら true. 特殊の "レベル固定 Lv1-5" は false. */
    parametric:  boolean;
    /** 特殊系の「レベル固定」スキル専用. true のときは該当固定値。 */
    fixedLevel?: 1 | 2 | 3 | 4 | 5;
}

/**
 * 実装済み Skill のカタログ. 新規スキルはこの末尾に追加し、
 * `id` は直前 id の次の未使用番号にする (= 実装順).
 *
 * カテゴリ別の ID 帯:
 *   - 100000〜199999  攻撃系 (近接 / 遠隔)
 *   - 500000〜599999  パッシブ系 (強化 / 防御)
 *   - 900000〜999999  特殊系
 */
export const SKILL_CATALOG: readonly SkillDefinition[] = [
    // 攻撃系 ─────────────────────────────────
    { id: "100000", name: "前方3マス攻撃",   category: "attack",
      description: "前方3マスに攻撃. 攻撃力 N", parametric: true },
    { id: "100001", name: "前方Nマスに攻撃", category: "attack",
      description: "前方N マスに攻撃. 攻撃力 2", parametric: true },
    { id: "100002", name: "遠距離 直線",     category: "ranged",
      description: "射程5 の直線状に遠距離攻撃. 攻撃力 N", parametric: true },
    { id: "100003", name: "遠距離 範囲",     category: "ranged",
      description: "射程5 の範囲内に遠距離攻撃. 攻撃体数 N", parametric: true },
    { id: "100004", name: "側面攻撃",        category: "attack",
      description: "左右 1 マスずつ同時攻撃. 攻撃力 N", parametric: true },
    { id: "100005", name: "後方攻撃",        category: "attack",
      description: "真後ろ 2 マスに攻撃 (振り向き不要). 攻撃力 N", parametric: true },
    { id: "100006", name: "貫通攻撃",        category: "ranged",
      description: "前方 5 マス一直線に貫通ダメージ. 攻撃力 N", parametric: true },
    { id: "100007", name: "カウンター",      category: "attack",
      description: "被弾直後 1 ターン攻撃力 N 倍", parametric: true },
    { id: "100008", name: "連撃",            category: "attack",
      description: "同じ敵に N 回追加ヒット", parametric: true },
    { id: "100009", name: "チャージ攻撃",    category: "attack",
      description: "N 拍溜めて 1 発巨大攻撃 (攻撃力 ∝ N)", parametric: true },
    { id: "100010", name: "突進",            category: "attack",
      description: "前方 N マス踏破、通過した敵にダメージ", parametric: true },

    // パッシブ系 ─────────────────────────────
    { id: "500000", name: "攻撃力アップ",        category: "buff",
      description: "攻撃力が N アップ", parametric: true },
    { id: "500001", name: "敵攻撃を防ぐ",        category: "buff",
      description: "敵の攻撃を N 回防ぐ", parametric: true },
    { id: "500002", name: "クリティカル率",      category: "buff",
      description: "クリティカル発生率 +N%", parametric: true },
    { id: "500003", name: "HP アップ",           category: "buff",
      description: "最大 HP +N", parametric: true },
    { id: "500004", name: "回復",                category: "buff",
      description: "1 拍ごと HP 1 回復、N 拍持続", parametric: true },
    { id: "500005", name: "移動速度",            category: "buff",
      description: "X 方向の自動走行 +N% 速度", parametric: true },
    { id: "500006", name: "視界",                category: "buff",
      description: "画面前方の fog を N マス先まで晴らす", parametric: true },
    { id: "500007", name: "再生",                category: "buff",
      description: "キューブドロップ時 HP 全回復を N 回まで可能", parametric: true },
    { id: "500008", name: "行動ボーナス",        category: "buff",
      description: "N 拍ごとに無料攻撃 (追加行動)", parametric: true },

    // 特殊系 ─────────────────────────────────
    { id: "900000", name: "パッシブ延長",        category: "special",
      description: "パッシブ効果を N 分延長", parametric: true },
    { id: "900001", name: "レベル固定 Lv1",      category: "special",
      description: "所持ブロックのレベルを 1 に固定", parametric: false, fixedLevel: 1 },
    { id: "900002", name: "レベル固定 Lv2",      category: "special",
      description: "所持ブロックのレベルを 2 に固定", parametric: false, fixedLevel: 2 },
    { id: "900003", name: "レベル固定 Lv3",      category: "special",
      description: "所持ブロックのレベルを 3 に固定", parametric: false, fixedLevel: 3 },
    { id: "900004", name: "レベル固定 Lv4",      category: "special",
      description: "所持ブロックのレベルを 4 に固定", parametric: false, fixedLevel: 4 },
    { id: "900005", name: "レベル固定 Lv5",      category: "special",
      description: "所持ブロックのレベルを 5 に固定", parametric: false, fixedLevel: 5 },
    { id: "900006", name: "スロー",              category: "special",
      description: "N 秒間 周囲を 0.5× 倍速化", parametric: true },
    { id: "900007", name: "時間巻き戻し",        category: "special",
      description: "直前 N 秒の被ダメージを帳消し (1 回限定)", parametric: true },
    { id: "900008", name: "ブロック合成",        category: "special",
      description: "同カテゴリ 3 つ → 1 上位スキル自動生成", parametric: false },
    { id: "900009", name: "ブロック分解",        category: "special",
      description: "1 高レベル → 複数低レベルに分解 (分解数 N)", parametric: true },
    { id: "900010", name: "敵変換",              category: "special",
      description: "接触した雑魚 1 体をキューブ化 (確率 N%)", parametric: true },
    { id: "900011", name: "確率倍増",            category: "special",
      description: "N 秒間 ドロップ率を 2 倍に", parametric: true },
    { id: "900012", name: "ターン固定スキル",    category: "special",
      description: "起動時レベルを ±1 段に固定 (昇降 1 段縛り)", parametric: false },
    { id: "900013", name: "レア度固定",          category: "special",
      description: "特定レア度でしかドロップしない縛り", parametric: false },
    { id: "900014", name: "カテゴリ封印",        category: "special",
      description: "指定カテゴリのスキルだけ無効化 (デバフ試練)", parametric: false },
];

/** 文字列 ID を catalog に照合. 未知 ID なら `null`. */
export function lookupSkill(id: string): SkillDefinition | null {
    return SKILL_CATALOG.find((s) => s.id === id) ?? null;
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

export function makeSkillBlock(id: string, size: SkillBlockSize = 3): SkillBlock {
    return {
        id,
        name:     id,
        skills:   [],
        size,
        shape:    emptyShape(size),
        interval: 1.0,
    };
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

    // skills は旧挙動: 文字列化、空削除。重複は **許可** (attack の
    // 「同じブロック集めるとレベルアップ」仕様を表現できるよう).
    const skills = Array.isArray(r.skills)
        ? (r.skills as unknown[]).map(String).filter((s) => s.length > 0)
        : [];

    // size: 3/5/7/9 のどれか. それ以外は 3 にフォールバック.
    const sizeRaw = Number(r.size);
    const size: SkillBlockSize = SKILL_BLOCK_SIZES.includes(sizeRaw as SkillBlockSize)
        ? (sizeRaw as SkillBlockSize) : 3;

    // shape: 3D 配列. 欠落 / 形状不一致は emptyShape(size) で置換.
    const rawShape = r.shape;
    let shape = emptyShape(size);
    if (Array.isArray(rawShape)) {
        const maybe: number[][][] = [];
        for (const layer of rawShape) {
            if (!Array.isArray(layer)) { maybe.push([]); continue; }
            const mLayer: number[][] = [];
            for (const row of layer) {
                if (!Array.isArray(row)) { mLayer.push([]); continue; }
                mLayer.push((row as unknown[]).map((v) => (v ? 1 : 0)));
            }
            maybe.push(mLayer);
        }
        shape = resizeShape(maybe, size);
    }

    // interval: 0 以上の数値, それ以外は 1.0.
    const ivRaw = Number(r.interval);
    const interval = Number.isFinite(ivRaw) && ivRaw >= 0 ? ivRaw : 1.0;

    return {
        id,
        name:     String(r.name ?? id),
        skills,
        size,
        shape,
        interval,
        notes:    typeof r.notes === "string" ? r.notes : undefined,
    };
}
