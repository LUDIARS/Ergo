/** RULE_CODE_CPP §C9.5 規則表: CMake 1 本に掛ける、pattern で書けない検査。 */

import { matchClose } from "./syntax.mjs";

/** §C4.2: target 個別 / ディレクトリスコープの `/utf-8`。 */
export function checkPerTargetUtf8(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(/\b(?:target_compile_options|add_compile_options)\s*(?=\()/gi)) {
    const open = m.index + m[0].length;
    const close = matchClose(text, open);
    if (close < 0) continue;
    for (const u of text.slice(open, close).matchAll(/\/utf-8\b/gi)) hits.push({ line: ctx.lineAt(open + u.index) });
  }
  return hits;
}

/** §C4.2: `_USE_MATH_DEFINES` を PUBLIC / INTERFACE で配布。 */
export function checkPublicMathDefines(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(/\btarget_compile_definitions\s*(?=\()/gi)) {
    const open = m.index + m[0].length;
    const close = matchClose(text, open);
    if (close < 0) continue;
    const body = text.slice(open, close);
    for (const d of body.matchAll(/\b_USE_MATH_DEFINES\b/g)) {
      const scopes = [...body.slice(0, d.index).matchAll(/\b(PUBLIC|INTERFACE|PRIVATE)\b/g)];
      const scope = scopes.at(-1)?.[1];
      if (scope === "PUBLIC" || scope === "INTERFACE") hits.push({ line: ctx.lineAt(open + d.index) });
    }
  }
  return hits;
}

/** §C4.7: 他リポの src/ を借りる。自リポの `${<prefix>_SOURCE_DIR}` と `${CMAKE_*}` は除く。 */
export function checkForeignSrc(ctx) {
  const text = ctx.masked;
  const own = ctx.config.prefix?.toLowerCase();
  const hits = [];
  for (const m of text.matchAll(/\$\{(\w+?)_(?:SOURCE_|BINARY_|ROOT_)?DIR\}\/(?:native\/)?src\b/g)) {
    const owner = m[1];
    if (/^(?:CMAKE|PROJECT)(?:_|$)/.test(owner) || /^CMAKE_CURRENT/.test(owner)) continue;
    if (own && owner.toLowerCase() === own) continue;
    hits.push({ line: ctx.lineAt(m.index) });
  }
  for (const m of text.matchAll(/(?:^|[\s"(;])((?:\.\.\/)+[A-Za-z][\w.-]*\/(?:native\/)?src)\b/g)) {
    hits.push({ line: ctx.lineAt(m.index + m[0].indexOf(m[1])) });
  }
  return hits;
}

/**
 * §C4.6: 自リポの option (同じファイルの option() か prefix 付き) を CACHE BOOL ... FORCE で書き換える。
 * 依存ライブラリ (FetchContent の curl 等) の option を構成のために FORCE するのは対象外。
 */
export function checkCacheForce(ctx) {
  const text = ctx.masked;
  const own = new Set([...text.matchAll(/\b(?:option|cmake_dependent_option)\s*\(\s*([A-Za-z_]\w*)/gi)].map((m) => m[1]));
  const prefix = ctx.config.prefix;
  const hits = [];
  for (const m of text.matchAll(/\bset\s*\(\s*([A-Za-z_]\w*)\s+[^()]*?\bCACHE\s+BOOL\s+"[^"\n]*"\s+FORCE\b/gi)) {
    if (own.has(m[1]) || (prefix && m[1].startsWith(`${prefix}_`))) hits.push({ line: ctx.lineAt(m.index) });
  }
  return hits;
}
