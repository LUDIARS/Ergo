/** RULE_CODE_CPP §C9.5 規則表: C/C++ ソース 1 本に掛ける、pattern で書けない検査。各関数は ctx を受けて { line, anyOf? }[] を返す。 */

import { OS_MACRO } from "./paths.mjs";
import { suppressionFor } from "./suppression.mjs";
import {
  classBodies, escapeRegExp, inExpressionPosition, matchAngle, matchClose, splitTopLevel,
} from "./syntax.mjs";

export const CALLABLE_PARAM = /^(?:Fn|F|Func|Function|Callback|Visitor|Pred|Predicate|Handler|Callable)\d*$/;

/** §C1.1: 呼び出す型引数 / 転送パックを持つ関数テンプレートに requires が無い。 */
export function checkUnconstrainedTemplate(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(/\btemplate\s*</g)) {
    const open = m.index + m[0].length - 1;
    const close = matchAngle(text, open);
    if (close < 0) continue;
    const callables = [], packs = [];
    for (const param of splitTopLevel(text.slice(open + 1, close), ",", { angles: true })) {
      const p = param.trim();
      if (/^template\b/.test(p)) continue; // テンプレートテンプレート引数
      const hit = /^(?:typename|class)\s*(\.\.\.)?\s*([A-Za-z_]\w*)/.exec(p);
      if (!hit) continue; // concept で制約済み (`std::invocable F` 等) か非型引数
      if (hit[1]) packs.push(hit[2]);
      else if (CALLABLE_PARAM.test(hit[2])) callables.push(hit[2]);
    }
    if (!callables.length && !packs.length) continue;
    const after = text.slice(close + 1);
    if (/^\s*(?:concept|class|struct|union|using|friend\s+(?:class|struct))\b/.test(after)) continue;
    let end = close + 1, paren = 0;
    for (; end < text.length; end++) {
      const c = text[end];
      if (c === "(") paren++;
      else if (c === ")") paren--;
      else if ((c === "{" || c === ";") && paren === 0) break;
    }
    const decl = text.slice(close + 1, end);
    if (/\brequires\b/.test(decl)) continue;
    const forwarding = packs.some((p) => new RegExp(`\\b${escapeRegExp(p)}\\s*&&\\s*\\.\\.\\.`).test(decl));
    if (!callables.length && !forwarding) continue;
    const first = ctx.lineAt(m.index), last = ctx.lineAt(end);
    hits.push({ line: first, anyOf: range(first, last) });
  }
  return hits;
}

export function range(a, b) {
  const out = [];
  for (let i = a; i <= b; i++) out.push(i);
  return out;
}

/** §C1.5: `I<Name>` の本体に非 static データメンバ。 */
export function checkInterfaceDataMember(ctx) {
  const hits = [];
  for (const cls of classBodies(ctx)) {
    if (!/^I[A-Z]\w*$/.test(cls.name)) continue;
    for (const member of cls.members) if (member.isData) hits.push({ line: member.line });
  }
  return hits;
}

/** §C2.4: vkCmdPushConstants / memcpy(mapped...) に渡す、このファイルで定義した型にレイアウト検査が無い。 */
export function checkGpuStructLayout(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(/\b(?:vkCmdPushConstants\s*(?=\()|memcpy\s*(?=\(\s*mapped))/g)) {
    const open = m.index + m[0].length;
    const close = matchClose(text, open);
    if (close < 0) continue;
    for (const s of text.slice(open + 1, close).matchAll(/\bsizeof\s*\(\s*([A-Za-z_]\w*)\s*\)/g)) {
      let type = s[1];
      const definedHere = (t) => new RegExp(`\\b(?:struct|class)\\s+${escapeRegExp(t)}\\b\\s*(?:final\\s*)?(?::[^;{]*)?\\{`).test(text);
      if (!definedHere(type)) {
        const decl = new RegExp(`\\b([A-Za-z_][\\w:]*)\\s+${escapeRegExp(type)}\\s*[;{=(]`).exec(text);
        if (!decl) continue;
        type = decl[1].split("::").pop();
        if (!definedHere(type)) continue; // ヘッダ側の型は §C2.12 の reflection CTest が見る
      }
      const t = escapeRegExp(type);
      const sized = new RegExp(`static_assert\\s*\\(\\s*sizeof\\s*\\(\\s*${t}\\s*\\)`).test(text);
      const offsets = new RegExp(`offsetof\\s*\\(\\s*${t}\\s*,`).test(text);
      if (!sized || !offsets) hits.push({ line: ctx.lineAt(m.index) });
    }
  }
  return hits;
}

/** §C2.5: 許可の注釈 `NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): <理由> (§C2.5)` の無い reinterpret_cast。 */
export function checkReinterpretCast(ctx) {
  const hits = [];
  for (const m of ctx.masked.matchAll(/\breinterpret_cast\s*</g)) {
    const line = ctx.lineAt(m.index);
    const note = suppressionFor(ctx, line, "cppcoreguidelines-pro-type-reinterpret-cast");
    if (note && /\(§C2\.5\)/.test(note)) continue;
    hits.push({ line });
  }
  return hits;
}

export const PRIM_TYPE = String.raw`(?:(?:const|volatile)\s+)*(?:(?:unsigned|signed)\s+)?(?:(?:std::)?(?:u?int(?:8|16|32|64)_t|u?int_(?:fast|least)(?:8|16|32|64)_t|size_t|ptrdiff_t|u?intptr_t)|long\s+long|long\s+double|int|long|short|char|char8_t|char16_t|char32_t|wchar_t|float|double|bool|unsigned|signed)`;
export const PRIM_CAST = new RegExp(String.raw`\(\s*${PRIM_TYPE}(?:\s*\*+)?\s*\)\s*(?=[\w(~!+\-&*]|::)`, "g");
export const POINTER_CAST = new RegExp(String.raw`\(\s*(?:(?:const|volatile)\s+)*(?:struct\s+)?[A-Za-z_][\w:]*(?:\s*<[^()<>;]*(?:<[^()<>;]*>[^()<>;]*)*>)?(?:\s+const)?\s*[*&]+\s*(?:const\s*)?\)\s*(?=[\w(]|::)`, "g");
export const FUNCTIONAL_CAST = /\b(?:(?:std::)?(?:u?int(?:8|16|32|64)_t|size_t|ptrdiff_t|u?intptr_t)|int|long|short|char|float|double|bool|unsigned)\s*(?=\()/g;
export const TYPE_WORD = /^(?:const|volatile|void|unsigned|signed|struct|class|typename|auto|int|long|short|char|float|double|bool|wchar_t|char8_t|char16_t|char32_t|(?:std::)?(?:u?int(?:8|16|32|64)_t|size_t))\b/;

/** §C2.6: C スタイルキャスト `(int)x` / `(Foo*)p`。 */
export function checkCStyleCast(ctx) {
  const text = ctx.masked;
  const seen = new Set();
  const hits = [];
  for (const re of [PRIM_CAST, POINTER_CAST]) {
    for (const m of text.matchAll(re)) {
      if (seen.has(m.index) || !inExpressionPosition(text, m.index)) continue;
      seen.add(m.index);
      hits.push({ line: ctx.lineAt(m.index) });
    }
  }
  return hits;
}

/** §C2.6: 関数形式キャスト `int(x)`。 */
export function checkFunctionalCast(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(FUNCTIONAL_CAST)) {
    if (!inExpressionPosition(text, m.index)) continue;
    const open = m.index + m[0].length;
    const close = matchClose(text, open);
    if (close < 0) continue;
    const inner = text.slice(open + 1, close).trim();
    if (!inner || /^(?:[*&]|\.\.\.)/.test(inner) || TYPE_WORD.test(inner)) continue;
    if (/^[A-Za-z_][\w:]*(?:<.*>)?\s*[*&]*\s+[A-Za-z_]\w*\s*(?:,|=|$)/.test(inner)) continue; // 関数型の引数宣言
    hits.push({ line: ctx.lineAt(m.index) });
  }
  return hits;
}

/** §C4.9: 1 つのソースにプラットフォーム条件の #if が 3 箇所以上。 */
export function checkPlatformIfCount(ctx) {
  const lines = [];
  const re = new RegExp(String.raw`^[ \t]*#[ \t]*(?:if|ifdef|ifndef)\b[^\n]*\b${OS_MACRO}\b`, "gm");
  for (const m of ctx.masked.matchAll(re)) lines.push(ctx.lineAt(m.index));
  return lines.length >= 3 ? [{ line: lines[2], anyOf: lines }] : [];
}

/**
 * §C6.4: `.release()` の戻り値を受け取っているか (= 所有権を取り出している)。
 * `video.release();` のような戻り値を捨てる呼び出し (OpenCV 等の解放 API) と、
 * 文字列引数が空白化されて `release(   )` に見えるものは除く。
 */
export function releaseResultUsed(m, ctx) {
  if (!/\(\s*\)$/.test(ctx.source.slice(m.index, m.index + m[0].length))) return false;
  const text = ctx.masked;
  let k = m.index - 1;
  while (k >= 0 && /[\w.\[\]>:-]/.test(text[k])) k--; // オブジェクト式 (a.b->c[i]) を遡る
  while (k >= 0 && /\s/.test(text[k])) k--;
  if (k < 0) return false;
  if (/[=(,{?:]/.test(text[k])) return text[k] !== "{" || /=\s*\{?\s*$/.test(text.slice(Math.max(0, k - 8), k + 1));
  return /\breturn$/.test(text.slice(Math.max(0, k - 6), k + 1));
}

/** §C6.5: string_view / span をメンバに持つ型の名前が `*View` で終わらない。 */
export function checkViewMember(ctx) {
  const hits = [];
  for (const cls of classBodies(ctx)) {
    if (/View$/.test(cls.name)) continue;
    for (const member of cls.members) {
      if (!member.isData) continue;
      if (/^(?:(?:mutable|const)\s+)*(?:std::)?(?:basic_string_view\b|w?string_view\b|u(?:8|16|32)string_view\b|span\s*<)/.test(member.text)) {
        hits.push({ line: member.line });
      }
    }
  }
  return hits;
}

export const THREAD_MEMBER = /\bstd::(?:thread|jthread|mutex|shared_mutex|recursive_mutex|timed_mutex|recursive_timed_mutex|condition_variable(?:_any)?|atomic(?:_flag)?)\b|\b\w+::Mutex\b/;

/** §C6.6: スレッド系メンバを持つヘッダに `Thread-safety:` 行が無い。 */
export function checkThreadSafetyNote(ctx) {
  if (/\bThread-safety\s*:/.test(ctx.comments)) return [];
  for (const cls of classBodies(ctx)) {
    const member = cls.members.find((mb) => mb.isData && THREAD_MEMBER.test(mb.text));
    if (member) return [{ line: member.line }];
  }
  return [];
}

/** §C6.7: マーカー `// <PREFIX>_ALLOW_GLOBAL: <理由>` の無い `static T& instance()`。 */
export function checkSingleton(ctx) {
  const marker = new RegExp(`\\b${ctx.config.prefix ? escapeRegExp(ctx.config.prefix) : "[A-Z][A-Z0-9]*"}_ALLOW_GLOBAL:\\s*\\S`);
  const hits = [];
  for (const m of ctx.masked.matchAll(/\bstatic\s+(?:inline\s+)?(?:const\s+)?[A-Za-z_][\w:]*(?:\s*<[^;{}()]*>)?\s*&\s*instance\s*\(/g)) {
    const line = ctx.lineAt(m.index);
    let prev = line - 1;
    while (prev >= 1 && !ctx.commentLines[prev - 1].trim()) prev--;
    if (marker.test(ctx.commentLines[line - 1]) || (prev >= 1 && marker.test(ctx.commentLines[prev - 1]))) continue;
    hits.push({ line });
  }
  return hits;
}

/** §C6.8: rename の直前 (8 行以内) に remove_all。 */
export function checkRemoveBeforeRename(ctx) {
  const hits = [];
  for (const m of ctx.masked.matchAll(/\bremove_all\s*\(/g)) {
    const line = ctx.lineAt(m.index);
    const window = ctx.maskedLines.slice(line - 1, line + 8).join("\n");
    if (/\brename\s*\(/.test(window)) hits.push({ line });
  }
  return hits;
}

/** §C8.3: `*Config/*Desc/*Info` を 2 要素以上の位置だけで集約初期化。 */
export function checkPositionalConfig(ctx) {
  const text = ctx.masked;
  const hits = [];
  for (const m of text.matchAll(/\b(\w*(?:Config|Desc|Info))\b(?:\s+[A-Za-z_]\w*)?\s*=?\s*\{/g)) {
    const before = text.slice(Math.max(0, m.index - 160), m.index);
    if (/(?:\b(?:struct|class|union|enum(?:\s+class)?|namespace|public|protected|private)\s+|->\s*)$/.test(before)) continue;
    if (/\b(?:struct|class)\s+\w+(?:\s+final)?\s*:[^;{}()]*$/.test(before)) continue; // 基底クラス指定
    const open = m.index + m[0].length - 1;
    const close = matchClose(text, open);
    if (close < 0) continue;
    const elems = splitTopLevel(text.slice(open + 1, close)).map((e) => e.trim()).filter(Boolean);
    if (elems.length >= 2 && !elems[0].startsWith(".")) hits.push({ line: ctx.lineAt(m.index) });
  }
  return hits;
}

/** §C9.4: NOLINT の形式 `NOLINT(<check>): <理由> (§Cn)`。 */
export function checkNolintForm(ctx) {
  const hits = [];
  ctx.commentLines.forEach((text, i) => {
    for (const m of text.matchAll(/\bNOLINT(NEXTLINE|BEGIN|END)?\b(\([^)]*\))?([^\n]*)/g)) {
      const checks = (m[2] ?? "").slice(1, -1).trim();
      if (!checks) { hits.push({ line: i + 1 }); continue; }
      if (m[1] === "END") continue;
      const reason = /^:\s*(\S.*?)\s*\(§C\d+(?:\.\d+)*\)/.exec(m[3]);
      if (!reason || reason[1].startsWith("(§")) hits.push({ line: i + 1 });
    }
  });
  return hits;
}

/** §C9.4: `#pragma warning(disable)` / `diagnostic ignored` は直前の行に理由、push / pop で囲む。 */
export function checkPragmaDisable(ctx) {
  const lines = ctx.maskedLines;
  const hits = [];
  const family = (t) => (/#\s*pragma\s+warning\b/.test(t) ? "msvc" : /#\s*pragma\s+(?:clang|GCC)\s+diagnostic\b/.test(t) ? "gcc" : null);
  const isPush = (t) => /#\s*pragma\s+warning\s*\(\s*push\b/.test(t) || /#\s*pragma\s+(?:clang|GCC)\s+diagnostic\s+push\b/.test(t);
  const isPop = (t) => /#\s*pragma\s+warning\s*\(\s*pop\b/.test(t) || /#\s*pragma\s+(?:clang|GCC)\s+diagnostic\s+pop\b/.test(t);
  lines.forEach((t, i) => {
    if (!/#\s*pragma\s+(?:warning\s*\(\s*disable\b|(?:clang|GCC)\s+diagnostic\s+ignored\b)/.test(t)) return;
    const fam = family(t);
    let depth = 0;
    for (let k = 0; k < i; k++) if (family(lines[k]) === fam) { if (isPush(lines[k])) depth++; else if (isPop(lines[k])) depth--; }
    const popped = lines.slice(i + 1).some((u) => family(u) === fam && isPop(u));
    let prev = i - 1;
    while (prev >= 0 && !ctx.commentLines[prev].trim()) prev--;
    const reasoned = prev >= 0 && /\/\/.*:\s*\S.*\(§C\d+(?:\.\d+)*\)/.test(ctx.commentLines[prev]);
    if (depth <= 0 || !popped || !reasoned) hits.push({ line: i + 1 });
  });
  return hits;
}

/** §C1.7: モジュールのインターフェース単位ファイルそのもの。 */
export function checkModuleUnitFile() {
  return [{ line: 1 }];
}
