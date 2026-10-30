/** RULE_CODE_CPP §C9.5 規則表: マスク済みテキストに対する字句の補助 (括弧の対応・template 引数・クラス本体)。 */

export function lineIndex(text) {
  const starts = [0];
  for (let i = 0; i < text.length; i++) if (text[i] === "\n") starts.push(i + 1);
  return (index) => {
    let lo = 0, hi = starts.length - 1;
    while (lo < hi) {
      const mid = (lo + hi + 1) >> 1;
      if (starts[mid] <= index) lo = mid; else hi = mid - 1;
    }
    return lo + 1;
  };
}

/** text[open] の括弧に対応する閉じ括弧の位置。見つからなければ -1。 */
export function matchClose(text, open) {
  const pairs = { "(": ")", "{": "}", "[": "]" };
  const want = pairs[text[open]];
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    const c = text[i];
    if (c === text[open]) depth++;
    else if (c === want && --depth === 0) return i;
  }
  return -1;
}

/** template ヘッダの `<` に対応する `>`。丸括弧の中の比較演算子は数えない。 */
export function matchAngle(text, open) {
  let depth = 0, paren = 0;
  for (let i = open; i < text.length; i++) {
    const c = text[i];
    if (c === "(") paren++;
    else if (c === ")") paren--;
    else if (paren > 0) continue;
    else if (c === "<") depth++;
    else if (c === ">" && --depth === 0) return i;
    else if (c === ";" || c === "{" || c === "}") return -1;
  }
  return -1;
}

/** 括弧の深さ 0 の区切り文字で分割する。 */
export function splitTopLevel(text, sep = ",", { angles = false } = {}) {
  const parts = [];
  let depth = 0, start = 0;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === "(" || c === "{" || c === "[" || (angles && c === "<")) depth++;
    else if (c === ")" || c === "}" || c === "]" || (angles && c === ">")) depth--;
    else if (c === sep && depth === 0) { parts.push(text.slice(start, i)); start = i + 1; }
  }
  parts.push(text.slice(start));
  return parts;
}

export function stripTemplateArgs(text) {
  let s = text;
  for (let k = 0; k < 12; k++) {
    const next = s.replace(/<[^<>;{}]*>/g, "");
    if (next === s) break;
    s = next;
  }
  return s;
}

export function escapeRegExp(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export const CAST_KEYWORDS = new Set(["return", "case", "co_return", "co_yield", "throw", "else", "do", "and", "or", "not", "xor"]);

/** text[at] の直前が「式の位置」か (= 括弧が関数呼び出しや宣言の引数リストではない)。 */
export function inExpressionPosition(text, at) {
  let k = at - 1;
  while (k >= 0 && /\s/.test(text[k])) k--;
  if (k < 0) return true;
  const ch = text[k];
  if (/\w/.test(ch)) {
    let s = k;
    while (s > 0 && /\w/.test(text[s - 1])) s--;
    return CAST_KEYWORDS.has(text.slice(s, k + 1));
  }
  if (ch === ")" || ch === "]" || ch === ">" || ch === "<") return false;
  if (ch === ":" && text[k - 1] === ":") return false;
  return true;
}

export const CLASS_HEAD =
  /\b(class|struct)\s+((?:\[\[[^\]]*\]\]\s*|alignas\s*\([^()]*\)\s*|[A-Z][A-Z0-9_]*\s+)*)([A-Za-z_]\w*)(?:\s+final)?\s*(?::(?!:)[^;{}]*)?\{/g;

/** クラス本体を列挙し、深さ 1 の文 (メンバ宣言) を返す。入れ子クラスも別要素として返す。 */
export function classBodies(ctx) {
  if (ctx._classes) return ctx._classes;
  const text = ctx.masked;
  const classes = [];
  for (const m of text.matchAll(CLASS_HEAD)) {
    if (/\benum\s+$/.test(text.slice(Math.max(0, m.index - 16), m.index))) continue;
    const open = m.index + m[0].length - 1;
    const close = matchClose(text, open);
    if (close < 0) continue;
    classes.push({ kind: m[1], name: m[3], line: ctx.lineAt(m.index), members: memberStatements(ctx, open + 1, close) });
  }
  ctx._classes = classes;
  return classes;
}

export function memberStatements(ctx, from, to) {
  const text = ctx.masked;
  const stmts = [];
  let brace = 0, paren = 0, start = from;
  for (let i = from; i < to; i++) {
    const c = text[i];
    if (c === "{") brace++;
    else if (c === "}") { if (--brace === 0 && paren === 0) { stmts.push([start, i + 1]); start = i + 1; } }
    else if (c === "(") paren++;
    else if (c === ")") paren--;
    else if (c === ";" && brace === 0 && paren === 0) { stmts.push([start, i]); start = i + 1; }
  }
  const members = [];
  for (const [a, b] of stmts) {
    const raw = text.slice(a, b);
    const lead = /^\s*(?:(?:public|protected|private)\s*:\s*)*(?:\[\[[^\]]*\]\]\s*)*/.exec(raw)[0];
    const decl = raw.slice(lead.length).trim();
    if (!decl) continue;
    members.push({ text: decl, line: ctx.lineAt(a + lead.length), isData: isDataMember(decl) });
  }
  return members;
}

export function isDataMember(decl) {
  if (/^(?:using|typedef|friend|enum|struct|class|union|template|virtual|explicit|static_assert|operator|return|namespace)\b/.test(decl)) return false;
  if (/^[~#]/.test(decl)) return false;
  if (/\b(?:static|operator)\b/.test(decl)) return false;
  const head = stripTemplateArgs(decl.replace(/\balignas\s*\([^()]*\)/g, " ")).split(/=|\{/)[0];
  if (head.includes("(")) return false;
  const clean = head.replace(/\b(?:mutable|const|volatile|constexpr|inline|thread_local)\b/g, " ").trim();
  if (!/^[\w:\s*&[\]]+(?::\s*\d+)?$/.test(clean)) return false;
  return (clean.match(/[A-Za-z_][\w:]*/g) ?? []).length >= 2;
}
