/** `node cpp-rules.mjs <repo> [--base <ref>] [--json]` (Revisor 登録テスト cpp-build-policy) の本体。 */

import path from "node:path";
import { collectChanges } from "./git-diff.mjs";

export function formatFinding(f) {
  return `${f.file}:${f.line}: ${f.severity} [§${f.id} ${f.rule}] ${f.message}`;
}

export const USAGE = `usage: node cpp-rules.mjs <repo> [--base <ref>] [--json]
  merge-base(<ref>, HEAD) からの変更 (作業ツリー・未追跡を含む) の追加行に規則表を掛ける。
  finding が 1 件でもあれば (warn を含む) exit 1。--base の既定は origin/main。`;

/**
 * CLI。戻り値は終了コード (0: finding なし / 1: finding あり・実行できない)。
 * 規則表への依存は引数で受ける (cpp-rules.mjs が規則表を束ねて渡す)。
 */
export function runCli(argv, { scanChangeset, loadRepoConfig, stdout = process.stdout, stderr = process.stderr }) {
  const args = [...argv];
  let repo = null, base = "origin/main", json = false;
  while (args.length) {
    const a = args.shift();
    if (a === "--base") { base = args.shift(); if (!base) { stderr.write(`${USAGE}\n`); return 1; } }
    else if (a === "--json") json = true;
    else if (a === "-h" || a === "--help") { stdout.write(`${USAGE}\n`); return 0; }
    else if (a.startsWith("-") || repo !== null) { stderr.write(`未知の引数: ${a}\n${USAGE}\n`); return 1; }
    else repo = a;
  }
  if (repo === null) { stderr.write(`${USAGE}\n`); return 1; }
  try {
    const root = path.resolve(repo);
    const config = loadRepoConfig(root);
    const { mergeBase, changes, repoFiles } = collectChanges(root, base);
    const findings = scanChangeset(changes, { config, repoFiles });
    if (json) stdout.write(`${JSON.stringify({ base, mergeBase, files: changes.length, findings }, null, 2)}\n`);
    else {
      for (const f of findings) stdout.write(`${formatFinding(f)}\n`);
      stdout.write(`cpp-rules: ${changes.length} ファイル、finding ${findings.length} 件 (base ${base} @ ${mergeBase.slice(0, 12)})\n`);
    }
    return findings.length ? 1 : 0;
  } catch (error) {
    stderr.write(`cpp-rules: ${error.message}\n`);
    return 1;
  }
}
