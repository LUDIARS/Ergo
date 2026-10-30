/** RULE_CODE_CPP §C9.6: merge-base からの変更集合 (ファイルごとの追加行) を git から集める。 */

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { languageOf, normalizePath } from "./paths.mjs";

export function unquoteGitPath(text) {
  if (!text.startsWith("\"")) return text;
  const bytes = [];
  const body = text.slice(1, text.lastIndexOf("\""));
  const simple = { n: 10, t: 9, r: 13, "\"": 34, "\\": 92, a: 7, b: 8, f: 12, v: 11 };
  for (let i = 0; i < body.length; i++) {
    const c = body[i];
    if (c !== "\\") { bytes.push(...Buffer.from(c, "utf8")); continue; }
    const oct = /^[0-7]{3}/.exec(body.slice(i + 1));
    if (oct) { bytes.push(parseInt(oct[0], 8)); i += 3; continue; }
    const e = body[++i];
    bytes.push(simple[e] ?? e.charCodeAt(0));
  }
  return Buffer.from(bytes).toString("utf8");
}

/**
 * `git diff --unified=0` の出力から、ファイルごとの追加行番号を取り出す。
 * @returns {Map<string, Set<number>>} 削除されたファイルは含まない
 */
export function parseUnifiedDiff(diff) {
  const files = new Map();
  let current = null;
  let oldLeft = 0, newLeft = 0, newLine = 0;
  for (const line of String(diff).split("\n")) {
    if (oldLeft > 0 || newLeft > 0) {
      if (line.startsWith("+")) { current?.add(newLine++); newLeft--; continue; }
      if (line.startsWith("-")) { oldLeft--; continue; }
      if (line.startsWith("\\")) continue; // "\ No newline at end of file"
      if (line.startsWith(" ")) { newLine++; oldLeft--; newLeft--; continue; }
      oldLeft = 0; newLeft = 0; // 想定外の行で hunk を閉じる
    }
    if (line.startsWith("diff --git ")) { current = null; continue; }
    if (line.startsWith("+++ ")) {
      const target = line.slice(4).replace(/\t.*$/, "");
      if (target === "/dev/null") { current = null; continue; }
      const p = unquoteGitPath(target).replace(/^b\//, "");
      current = files.get(p) ?? new Set();
      files.set(p, current);
      continue;
    }
    const hunk = /^@@ -\d+(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/.exec(line);
    if (hunk) {
      oldLeft = hunk[1] === undefined ? 1 : Number(hunk[1]);
      newLine = Number(hunk[2]);
      newLeft = hunk[3] === undefined ? 1 : Number(hunk[3]);
    }
  }
  return files;
}

export function git(repo, args) {
  return execFileSync("git", ["-C", repo, "-c", "core.quotePath=false", ...args], { encoding: "utf8", maxBuffer: 1 << 28, stdio: ["ignore", "pipe", "pipe"] });
}

/** merge-base から作業ツリーまでの変更集合 (未追跡ファイルは全行を追加行とする)。 */
export function collectChanges(repo, base) {
  let mergeBase;
  try {
    mergeBase = git(repo, ["merge-base", base, "HEAD"]).trim();
  } catch {
    throw new Error(`merge-base を取得できない (base: ${base})。全域検査には切り替えない (§C9.6)。--base で比較先を指定する`);
  }
  const diff = git(repo, ["diff", "--unified=0", "--no-color", "--no-ext-diff", "--src-prefix=a/", "--dst-prefix=b/", "--diff-filter=ACMR", mergeBase]);
  const added = parseUnifiedDiff(diff);
  const untracked = git(repo, ["ls-files", "--others", "--exclude-standard", "-z"]).split("\0").filter(Boolean);
  for (const f of untracked) added.set(f, null);
  const tracked = git(repo, ["ls-files", "-z"]).split("\0").filter(Boolean);
  const changes = [];
  for (const [file, lines] of added) {
    if (!languageOf(normalizePath(file))) continue;
    const abs = path.join(repo, file);
    if (!existsSync(abs)) continue;
    changes.push({ file, source: readFileSync(abs, "utf8"), addedLines: lines });
  }
  return { mergeBase, changes, repoFiles: [...tracked, ...untracked] };
}
