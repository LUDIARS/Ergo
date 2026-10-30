/** RULE_CODE_CPP §C9.5 規則表: 変更集合全体を見る検査 (対応するテスト・ハーネス・C++ 側の変更が伴うか)。各関数は cs を受けて { file, line }[] を返す。 */

import { maskCommentsAndLiterals } from "./mask.mjs";
import { PATHS, TESTS_DIR } from "./paths.mjs";
import { escapeRegExp, lineIndex, matchClose } from "./syntax.mjs";

export function moduleOf(file) {
  const segs = file.split("/");
  let k = segs.lastIndexOf("include");
  if (k >= 0 && segs.length > k + 3) return { root: segs.slice(0, k).join("/"), name: segs[k + 2] };
  k = segs.lastIndexOf("src");
  if (k >= 0 && segs.length > k + 2) return { root: segs.slice(0, k).join("/"), name: segs[k + 1] };
  if (k >= 0) return { root: segs.slice(0, k).join("/"), name: segs[k + 1].replace(/\.[^.]+$/, "") };
  return { root: "", name: segs.length >= 2 ? segs[segs.length - 2] : segs[0].replace(/\.[^.]+$/, "") };
}

export const firstLine = (change) => (change.addedLines && change.addedLines.size ? Math.min(...change.addedLines) : 1);

/** §C3.5: concepts.h の変更に `tests/<module>/test_<module>_concepts.cpp` の変更が伴わない。 */
export function checkConceptTests(cs) {
  const hits = [];
  for (const c of cs.changes) {
    const m = /(?:^|\/)([^/]+)\/concepts\.h$/.exec(c.file);
    if (!m || TESTS_DIR.test(c.file)) continue;
    const mod = escapeRegExp(m[1]);
    const test = new RegExp(`(?:^|/)tests/${mod}/test_${mod}_concepts\\.(?:cpp|cc|cxx)$`);
    if (!cs.changes.some((o) => test.test(o.file))) hits.push({ file: c.file, line: firstLine(c) });
  }
  return hits;
}

/**
 * §C3.8 のデコーダ: 外部由来のバイト列を読むファイル (音声・JSON・wire・シーン・セーブ)。
 * `loader` 単独は §C1.8 の実行時ロード (`*_loader.cpp`) と衝突するので、シーンとアセットのローダに限る。
 */
export const DECODER_PATH = /decoder|parser|codec|protocol|deserial|reader|save|(?:scene|asset)_?loader/i;

/** §C3.8: デコーダ類の変更に fuzz ハーネスが無い (変更集合にもリポにも)。 */
export function checkFuzzHarness(cs) {
  const hits = [];
  for (const c of cs.changes) {
    if (c.lang !== "cpp" || !DECODER_PATH.test(c.file) || TESTS_DIR.test(c.file) || /(?:^|\/)fuzz\//.test(c.file)) continue;
    const mod = moduleOf(c.file);
    const harness = new RegExp(`(?:^|/)fuzz/${escapeRegExp(mod.name)}/fuzz_[^/]+\\.(?:c|cc|cpp|cxx)$`);
    if (cs.changes.some((o) => harness.test(o.file)) || cs.repoFiles.some((f) => harness.test(f))) continue;
    hits.push({ file: c.file, line: firstLine(c) });
  }
  return hits;
}

export const SHADER_LAYOUT_LINE = /\b(?:struct|layout\s*\(|buffer|uniform|push_constant)\b/;

/** GPU 型の宣言 (struct / layout / buffer / uniform の行と、その { } の中) の行番号。 */
export function shaderLayoutLines(masked) {
  const lineAt = lineIndex(masked);
  const lines = new Set();
  for (const m of masked.matchAll(new RegExp(SHADER_LAYOUT_LINE.source, "g"))) lines.add(lineAt(m.index));
  let start = 0;
  for (let i = 0; i < masked.length; i++) {
    const c = masked[i];
    if (c === ";" || c === "}") { start = i + 1; continue; }
    if (c !== "{") continue;
    const close = matchClose(masked, i);
    if (SHADER_LAYOUT_LINE.test(masked.slice(start, i)) && close > 0) {
      for (let l = lineAt(i); l <= lineAt(close); l++) lines.add(l);
      i = close;
    }
    start = i + 1;
  }
  return lines;
}

/** §C2.12: シェーダの GPU 型 (struct / layout / buffer) の変更に、C++ ヘッダとレイアウトテストの変更が伴わない。 */
export function checkShaderLayoutSync(cs) {
  const header = cs.changes.some((o) => PATHS.header.test(o.file) && /(?:^|\/)(?:include|src)\//.test(o.file));
  const test = cs.changes.some((o) => /shader_layout/i.test(o.file) && TESTS_DIR.test(o.file));
  if (header && test) return [];
  const hits = [];
  for (const c of cs.changes) {
    if (c.lang !== "shader") continue;
    const layout = shaderLayoutLines(maskCommentsAndLiterals(c.source, { language: "shader" }));
    const touched = [...layout].filter((l) => !c.addedLines || c.addedLines.has(l)).sort((a, b) => a - b);
    if (touched.length) hits.push({ file: c.file, line: touched[0] });
  }
  return hits;
}
