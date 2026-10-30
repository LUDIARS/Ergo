/** RULE_CODE_CPP §C9.5 規則表: リポごとの設定 (`cpp-rules.json`) の検証と読み込み。 */

import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { escapeRegExp } from "./syntax.mjs";

/** リポのルートに置く規則表の設定ファイル名。 */
export const CONFIG_FILE = "cpp-rules.json";

/** 規約が名前を決めている helper の既定位置 (§C2.13 / §C6.4 / §C6.8)。設定で追加できる。 */
export const DEFAULT_IO_HELPERS = ["**/utf8_path.*", "**/path_utf8.*", "**/atomic_write.*", "**/atomic_write_file.*"];
export const DEFAULT_OWNERSHIP_HELPERS = ["**/ownership.*", "**/*_handoff.*"];

export function globToRegExp(glob) {
  let g = String(glob).replace(/\\/g, "/").replace(/^\.\//, "");
  if (g.endsWith("/")) g += "**";
  let re = "";
  for (let i = 0; i < g.length; i++) {
    const c = g[i];
    if (c === "*" && g[i + 1] === "*") {
      if (g[i + 2] === "/") { re += "(?:.*/)?"; i += 2; } else { re += ".*"; i += 1; }
    } else if (c === "*") re += "[^/]*";
    else if (c === "?") re += "[^/]";
    else re += escapeRegExp(c);
  }
  return new RegExp(`^${re}$`);
}

export function expectStrings(key, value) {
  if (!Array.isArray(value) || value.some((v) => typeof v !== "string" || !v.trim())) {
    throw new Error(`${CONFIG_FILE}: "${key}" は空でない文字列の配列にする`);
  }
  return value;
}

/**
 * 設定を検証して正規化する。未知のキーや型違いは例外にする (黙って既定値に戻さない。RULE_CODE §7.1)。
 *
 * {
 *   "prefix": "ERGO",                         // マクロ prefix (§C6.7 のマーカー、§C4.7 の自リポ判定)
 *   "logicDirs": ["src/world/"],              // §C7.2〜§C7.4 のロジック層 (glob。末尾 / はディレクトリ)
 *   "logger": { "files": ["src/log/log.cpp"] }, // §C8.4 を有効にし、ロガー実装を除外する
 *   "ioHelpers": ["src/io/path_utf8.cpp"],    // §C2.13 / §C6.8 の helper (既定に追加)
 *   "ownershipHelpers": ["src/net/owned.h"],  // §C6.4 の helper (既定に追加)
 *   "publicIncludeAllow": ["vulkan"],         // §C8.5 で公開ヘッダに許す SDK
 *   "disable": { "C3.1": "<理由>" }           // §C 番号か規則名 → 理由 (理由は必須。§C9.5)
 * }
 *
 * ruleKeys: 規則表の §C 番号と規則名の集合。渡すと disable のキーが実在するかを検査する。
 * lockedKeys: 無効にできない規則 (suppressible: false) の §C 番号と規則名の集合。
 *   渡すと、それを disable に書いた設定を例外にする (§C9.4「抑制せずにコードを直す」・§C9.5)。
 */
export function parseRepoConfig(raw = {}, { ruleKeys = null, lockedKeys = null } = {}) {
  if (raw === null || typeof raw !== "object" || Array.isArray(raw)) throw new Error(`${CONFIG_FILE}: オブジェクトにする`);
  const known = new Set(["prefix", "logicDirs", "logger", "ioHelpers", "ownershipHelpers", "publicIncludeAllow", "disable"]);
  for (const key of Object.keys(raw)) if (!known.has(key)) throw new Error(`${CONFIG_FILE}: 未知のキー "${key}"`);
  if (raw.prefix !== undefined && !(typeof raw.prefix === "string" && /^[A-Z][A-Z0-9]*$/.test(raw.prefix))) {
    throw new Error(`${CONFIG_FILE}: "prefix" は英大文字のマクロ prefix (例: ERGO) にする`);
  }
  let logger = null;
  if (raw.logger !== undefined) {
    if (raw.logger === null || typeof raw.logger !== "object" || Array.isArray(raw.logger)) throw new Error(`${CONFIG_FILE}: "logger" はオブジェクトにする`);
    for (const key of Object.keys(raw.logger)) if (key !== "files") throw new Error(`${CONFIG_FILE}: "logger" の未知のキー "${key}"`);
    logger = { files: expectStrings("logger.files", raw.logger.files).map(globToRegExp) };
  }
  const disable = new Map();
  if (raw.disable !== undefined) {
    if (raw.disable === null || typeof raw.disable !== "object" || Array.isArray(raw.disable)) throw new Error(`${CONFIG_FILE}: "disable" はオブジェクトにする`);
    for (const [key, reason] of Object.entries(raw.disable)) {
      if (ruleKeys && !ruleKeys.has(key)) throw new Error(`${CONFIG_FILE}: "disable" に存在しない規則 "${key}"`);
      if (lockedKeys && lockedKeys.has(key)) {
        throw new Error(`${CONFIG_FILE}: "disable" の "${key}" は無効にできない規則 (suppressible: false) を含む。コードを直す (RULE_CODE_CPP §C9.4 / §C9.5)`);
      }
      if (typeof reason !== "string" || !reason.trim()) throw new Error(`${CONFIG_FILE}: "disable.${key}" に理由を書く`);
      disable.set(key, reason);
    }
  }
  return {
    prefix: raw.prefix ?? null,
    logicDirs: raw.logicDirs === undefined ? [] : expectStrings("logicDirs", raw.logicDirs).map(globToRegExp),
    logger,
    ioHelpers: [...DEFAULT_IO_HELPERS, ...(raw.ioHelpers === undefined ? [] : expectStrings("ioHelpers", raw.ioHelpers))].map(globToRegExp),
    ownershipHelpers: [...DEFAULT_OWNERSHIP_HELPERS, ...(raw.ownershipHelpers === undefined ? [] : expectStrings("ownershipHelpers", raw.ownershipHelpers))].map(globToRegExp),
    publicIncludeAllow: (raw.publicIncludeAllow === undefined ? [] : expectStrings("publicIncludeAllow", raw.publicIncludeAllow)).map((s) => s.toLowerCase()),
    disable,
  };
}

/** `<repo>/cpp-rules.json` を読む。無ければ既定。壊れていれば例外。 */
export function loadRepoConfig(repo, options = {}) {
  const file = path.join(repo, CONFIG_FILE);
  if (!existsSync(file)) return parseRepoConfig({}, options);
  let raw;
  try {
    raw = JSON.parse(readFileSync(file, "utf8"));
  } catch (error) {
    throw new Error(`${CONFIG_FILE}: JSON として読めない (${error.message})`);
  }
  return parseRepoConfig(raw, options);
}

export const inAny = (patterns, file) => patterns.some((re) => re.test(file));
