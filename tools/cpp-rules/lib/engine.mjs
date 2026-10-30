/** RULE_CODE_CPP §C9.5 規則表の実行: 規則をファイル 1 本と変更集合に掛け、finding (抜粋なし) を作る。 */

import { maskCommentsAndLiterals } from "./mask.mjs";
import { GLOBAL_EXCLUDE, languageOf, normalizePath } from "./paths.mjs";
import { suppressionFor } from "./suppression.mjs";
import { lineIndex } from "./syntax.mjs";

export const MAX_FINDINGS_PER_FILE = 200;

export function scanContext(file, source, lang, config) {
  const masked = maskCommentsAndLiterals(source, { language: lang });
  const comments = maskCommentsAndLiterals(source, { language: lang, keepComments: true });
  return {
    file, source, lang, config, masked, comments,
    maskedLines: masked.split("\n"),
    commentLines: comments.split("\n"),
    lineAt: lineIndex(masked),
  };
}

export function ruleEnabled(rule, config) {
  if (config.disable.has(rule.id) || config.disable.has(rule.name)) return false;
  return !rule.when || rule.when(config);
}

export function ruleApplies(rule, file, config) {
  if (!rule.files.test(file)) return false;
  if (rule.exclude && rule.exclude.test(file)) return false;
  if (rule.skip && rule.skip(file, config)) return false;
  return true;
}

export function patternHits(rule, ctx) {
  const text = rule.mask === "none" ? ctx.source : rule.mask === "comments" ? ctx.comments : ctx.masked;
  const flags = new Set(rule.pattern.flags);
  flags.add("g");
  flags.add("d");
  const re = new RegExp(rule.pattern.source, [...flags].join(""));
  const hits = [];
  for (const m of text.matchAll(re)) {
    if (rule.filter && !rule.filter(m, ctx)) continue;
    const at = m.indices?.groups?.at?.[0] ?? m.index;
    hits.push({ line: ctx.lineAt(at) });
  }
  return hits;
}

export function toAddedSet(addedLines) {
  if (addedLines === null || addedLines === undefined) return null;
  return addedLines instanceof Set ? addedLines : new Set(addedLines);
}

/**
 * 1 ファイルに単一ファイルの規則 (scope が added-lines / file) を掛ける。
 *
 * @param {object[]} rules 規則表
 * @param {string} file リポ相対パス
 * @param {string} source ファイル全体の内容
 * @param {{ addedLines?: Iterable<number>|null, config: object }} options
 *   addedLines: 追加行の行番号 (1 始まり)。null / 省略はファイル全体を追加行とみなす (新規ファイル・Write)。
 *   空なら (削除だけの変更) 何も報告しない。config: parseRepoConfig の結果。
 * @returns {{id: string, rule: string, file: string, line: number, severity: string, message: string}[]}
 */
export function scanSource(rules, file, source, { addedLines = null, config }) {
  const rel = normalizePath(file);
  if (GLOBAL_EXCLUDE.test(rel)) return [];
  const lang = languageOf(rel);
  if (!lang) return [];
  const added = toAddedSet(addedLines);
  if (added && added.size === 0) return [];
  const ctx = scanContext(rel, String(source), lang, config);
  const findings = [];
  const seen = new Set();
  for (const rule of rules) {
    if (rule.scope === "changeset" || !ruleEnabled(rule, config) || !ruleApplies(rule, rel, config)) continue;
    const hits = rule.check ? rule.check(ctx) : patternHits(rule, ctx);
    for (const hit of hits) {
      const lines = hit.anyOf ?? [hit.line];
      if (rule.scope === "added-lines" && added && !lines.some((l) => added.has(l))) continue;
      if (rule.suppressible !== false && suppressionFor(ctx, hit.line, `cpp-rules-${rule.id}`)) continue;
      const key = `${rule.name}:${hit.line}`;
      if (seen.has(key)) continue;
      seen.add(key);
      findings.push({ id: rule.id, rule: rule.name, file: rel, line: hit.line, severity: rule.severity, message: rule.message });
    }
  }
  findings.sort((a, b) => a.line - b.line || a.id.localeCompare(b.id, "en", { numeric: true }));
  if (findings.length > MAX_FINDINGS_PER_FILE) {
    // 打ち切っても pass にはしない (Revisor の finding-limit と同じ)。
    const kept = findings.slice(0, MAX_FINDINGS_PER_FILE);
    kept.push({ id: "C9.5", rule: "finding-limit", file: rel, line: kept.at(-1).line, severity: "error", message: `finding が ${MAX_FINDINGS_PER_FILE} 件を超えたので以降を省略した` });
    return kept;
  }
  return findings;
}

/**
 * 変更集合に、単一ファイルの規則と変更集合の規則 (scope: changeset) を掛ける。
 *
 * @param {object[]} rules 規則表
 * @param {{file: string, source: string, addedLines?: Iterable<number>|null}[]} changes
 * @param {{ config: object, repoFiles?: string[] }} options repoFiles: リポの追跡ファイル一覧 (§C3.8 の既存ハーネス判定)
 */
export function scanChangeset(rules, changes, { config, repoFiles = [] }) {
  const normalized = [];
  for (const c of changes) {
    const file = normalizePath(c.file);
    if (GLOBAL_EXCLUDE.test(file)) continue;
    const lang = languageOf(file);
    if (!lang) continue;
    const addedLines = toAddedSet(c.addedLines);
    if (addedLines && addedLines.size === 0) continue;
    normalized.push({ file, source: String(c.source), lang, addedLines });
  }
  const findings = normalized.flatMap((c) => scanSource(rules, c.file, c.source, { addedLines: c.addedLines, config }));
  const cs = { changes: normalized, config, repoFiles: repoFiles.map(normalizePath) };
  for (const rule of rules) {
    if (rule.scope !== "changeset" || !ruleEnabled(rule, config)) continue;
    for (const hit of rule.check(cs)) {
      if (!ruleApplies(rule, hit.file, config)) continue;
      findings.push({ id: rule.id, rule: rule.name, file: hit.file, line: hit.line, severity: rule.severity, message: rule.message });
    }
  }
  return findings;
}
