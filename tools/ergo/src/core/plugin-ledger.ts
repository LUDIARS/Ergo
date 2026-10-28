// @implements SPEC-ergo-studio-plugins
/// 取得済みプラグインパックの台帳。
///
/// Studio のプラグイン画面 (plugins/studio) が GitHub から取得したパックを
/// `<studio home>/plugins/ledger.json` に記録し、起動時に external.ts が
/// 有効なパックを `ERGO_PLUGIN_DIR` と同じ扱いで読み込む。台帳は core に置き、
/// core → plugin の逆依存を作らない。
///
/// 台帳は「承認して入れたもの」の記録であり、ここで検証や取得はしない
/// (取得元の許可・リビジョン固定は plugins/studio/domain/plugin-source.ts)。

import { existsSync, readFileSync } from "node:fs";
import { mkdir, rename, writeFile } from "node:fs/promises";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";

export const LEDGER_VERSION = 1;

export interface LedgerPack {
    /** `<owner>/<repo>:<subpath>` — 台帳内で一意。 */
    id:          string;
    owner:       string;
    repo:        string;
    /** パック (プラグインのサブディレクトリを並べた親) のリポ内パス。 */
    subpath:     string;
    /** 固定したコミット (40 桁の sha)。 */
    rev:         string;
    /** 展開先 (`<home>/plugins/packs/<owner>/<repo>/<rev>`)。 */
    checkoutDir: string;
    enabled:     boolean;
    installedAt: string;
    /** 承認時に見せたプラグイン名の一覧。 */
    plugins:     string[];
}

export interface Ledger {
    version: number;
    packs:   LedgerPack[];
}

/// Studio のデータ置き場。`ERGO_STUDIO_HOME` が無ければ `<home>/.ergo-studio`。
export function studioHome(env: NodeJS.ProcessEnv = process.env): string {
    const explicit = env.ERGO_STUDIO_HOME?.trim();
    return resolve(explicit ? explicit : join(homedir(), ".ergo-studio"));
}

export function ledgerPath(home: string): string {
    return join(home, "plugins", "ledger.json");
}

/// 台帳 JSON を検査して読む。壊れた行は捨てる (起動を止めない)。
export function parseLedger(text: string): Ledger {
    let raw: unknown;
    try {
        raw = JSON.parse(text);
    } catch {
        return { version: LEDGER_VERSION, packs: [] };
    }
    const packs: LedgerPack[] = [];
    const list = (raw as { packs?: unknown })?.packs;
    if (Array.isArray(list)) {
        for (const p of list) {
            if (!p || typeof p !== "object") continue;
            const o = p as Record<string, unknown>;
            if (typeof o.id !== "string" || typeof o.owner !== "string" || typeof o.repo !== "string" ||
                typeof o.subpath !== "string" || typeof o.rev !== "string" ||
                typeof o.checkoutDir !== "string") continue;
            packs.push({
                id:          o.id,
                owner:       o.owner,
                repo:        o.repo,
                subpath:     o.subpath,
                rev:         o.rev,
                checkoutDir: o.checkoutDir,
                enabled:     o.enabled === true,
                installedAt: typeof o.installedAt === "string" ? o.installedAt : "",
                plugins:     Array.isArray(o.plugins) ? o.plugins.filter((x): x is string => typeof x === "string") : [],
            });
        }
    }
    return { version: LEDGER_VERSION, packs };
}

export function readLedger(home: string): Ledger {
    const path = ledgerPath(home);
    if (!existsSync(path)) return { version: LEDGER_VERSION, packs: [] };
    try {
        return parseLedger(readFileSync(path, "utf8"));
    } catch (err) {
        console.warn(`[ergo] plugin ledger not readable: ${path} (${(err as Error).message})`);
        return { version: LEDGER_VERSION, packs: [] };
    }
}

/// 一時ファイルに書いてから置き換える (書きかけの台帳を残さない)。
export async function writeLedger(home: string, ledger: Ledger): Promise<void> {
    const path = ledgerPath(home);
    await mkdir(dirname(path), { recursive: true });
    const tmp = `${path}.tmp`;
    await writeFile(tmp, JSON.stringify({ version: LEDGER_VERSION, packs: ledger.packs }, null, 2) + "\n", "utf8");
    await rename(tmp, path);
}

/// 有効なパックのディレクトリ (`<checkoutDir>/<subpath>`)。
export function enabledPackDirs(ledger: Ledger): string[] {
    return ledger.packs.filter((p) => p.enabled).map((p) => join(p.checkoutDir, p.subpath));
}
