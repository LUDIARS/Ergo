/** RULE_CODE_CPP §C9.4 の形式の抑制注釈 (NOLINT / NOLINTNEXTLINE / NOLINTBEGIN・END) の解決。 */

import { escapeRegExp } from "./syntax.mjs";

export function checkListMatches(list, check) {
  return list.split(",").map((s) => s.trim()).filter(Boolean).some((item) => {
    const re = new RegExp(`^${item.split("*").map(escapeRegExp).join(".*")}$`);
    return re.test(check);
  });
}

/**
 * 規則表の finding の抑制は `// NOLINT(cpp-rules-<id>): <理由> (§<id>)` (同形式の NEXTLINE・BEGIN/END)。
 * 戻り値は抑制に使われた注釈の行テキスト (無ければ null)。注釈そのものの形式は §C9.4 の規則が検査する。
 */
export function suppressionFor(ctx, line, check) {
  const lines = ctx.commentLines;
  const same = lines[line - 1] ?? "";
  for (const m of same.matchAll(/\bNOLINT\(([^)]*)\)/g)) if (checkListMatches(m[1], check)) return same;
  const prev = lines[line - 2] ?? "";
  for (const m of prev.matchAll(/\bNOLINTNEXTLINE\(([^)]*)\)/g)) if (checkListMatches(m[1], check)) return prev;
  let open = null;
  for (let i = 0; i < line - 1; i++) {
    for (const m of lines[i].matchAll(/\bNOLINT(BEGIN|END)\(([^)]*)\)/g)) {
      if (!checkListMatches(m[2], check)) continue;
      open = m[1] === "BEGIN" ? lines[i] : null;
    }
  }
  return open;
}
