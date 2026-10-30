/** RULE_CODE_CPP §C9.5 規則表: コメント・文字列リテラルの空白化 (行番号と列を保つ)。 */

const STRING_PREFIX = new Set(["u8", "u", "U", "L"]);
const RAW_PREFIX = new Set(["R", "u8R", "uR", "UR", "LR"]);

/**
 * コメントと文字列・文字リテラルを空白にする。改行 (\n, \r) は残すので、行番号と列位置が保たれる。
 * Revisor `src/praesidium/rules.mjs` と同じ方針に、C++ の次を足したもの:
 * raw 文字列 `R"d(...)d"`、桁区切り `1'000` (文字リテラルとして扱わない)、行コメントの `\` 継続。
 * `#include "x.h"` の header-name は文字列リテラルではない (C++ の文法上も別 token) ので残す。
 *
 * @param {string} source
 * @param {{ language?: "cpp"|"shader"|"cmake"|"json", keepComments?: boolean }} [options]
 *   keepComments: true ならリテラルだけを空白化してコメントを残す (NOLINT など注釈の検査用)。
 *   cmake はコメント (`#`・`#[[ ]]`) だけを空白化し、引用引数は残す (`"/utf-8"` 等を検査するため)。
 */
export function maskCommentsAndLiterals(source, { language = "cpp", keepComments = false } = {}) {
  const src = String(source);
  if (language === "json") return src;
  if (language === "cmake") return keepComments ? src : maskCMake(src);
  return maskCLike(src, keepComments);
}

function blankInto(out, src, from, to) {
  for (let i = from; i < to; i++) {
    const c = src[i];
    out[i] = c === "\n" || c === "\r" ? c : " ";
  }
}

function lineCommentEnd(src, from) {
  let j = from;
  for (;;) {
    const nl = src.indexOf("\n", j);
    if (nl < 0) return src.length;
    let k = nl - 1;
    if (src[k] === "\r") k--;
    if (src[k] === "\\") { j = nl + 1; continue; }
    return nl;
  }
}

function quotedEnd(src, from, quote) {
  let j = from + 1;
  while (j < src.length) {
    const c = src[j];
    if (c === "\\") { j += 2; continue; }
    if (c === quote) return j + 1;
    if (c === "\n") return j; // 閉じていないリテラルは行末までにする
    j++;
  }
  return src.length;
}

function maskCLike(src, keepComments) {
  const out = src.split("");
  const n = src.length;
  let i = 0;
  let atLineStart = true;
  let includeLine = false;
  while (i < n) {
    const c = src[i];
    if (c === "\n") { atLineStart = true; includeLine = false; i++; continue; }
    if (atLineStart) {
      if (c === " " || c === "\t" || c === "\r" || c === "\f" || c === "\v") { i++; continue; }
      atLineStart = false;
      if (c === "#") includeLine = /^#\s*(?:include|include_next|import)\b/.test(src.slice(i, i + 40));
    }
    const d = src[i + 1];
    if (c === "/" && d === "/") {
      const end = lineCommentEnd(src, i + 2);
      if (!keepComments) blankInto(out, src, i, end);
      i = end;
      continue;
    }
    if (c === "/" && d === "*") {
      const close = src.indexOf("*/", i + 2);
      const end = close < 0 ? n : close + 2;
      if (!keepComments) blankInto(out, src, i, end);
      i = end;
      continue;
    }
    if (/[0-9]/.test(c) || (c === "." && /[0-9]/.test(d ?? ""))) {
      // pp-number: 桁区切りの ' を文字リテラルと誤認しない
      const m = /\.?[0-9](?:'[0-9A-Za-z_]|[eEpP][+-]|[0-9A-Za-z_.])*/y;
      m.lastIndex = i;
      const hit = m.exec(src);
      i += hit ? hit[0].length : 1;
      continue;
    }
    if (/[A-Za-z_$]/.test(c)) {
      const m = /[A-Za-z_$][A-Za-z0-9_$]*/y;
      m.lastIndex = i;
      const word = m.exec(src)[0];
      const next = src[i + word.length];
      if (next === "\"" && RAW_PREFIX.has(word)) {
        const open = src.indexOf("(", i + word.length + 1);
        const delim = open < 0 ? null : src.slice(i + word.length + 1, open);
        if (delim !== null && delim.length <= 16 && !/[\s\\()]/.test(delim)) {
          const close = src.indexOf(`)${delim}"`, open + 1);
          const end = close < 0 ? n : close + delim.length + 2;
          blankInto(out, src, i, end);
          i = end;
          continue;
        }
      }
      if ((next === "\"" || next === "'") && STRING_PREFIX.has(word)) {
        const end = quotedEnd(src, i + word.length, next);
        blankInto(out, src, i, end);
        i = end;
        continue;
      }
      i += word.length;
      continue;
    }
    if (c === "\"" || c === "'") {
      const end = quotedEnd(src, i, c);
      if (!(includeLine && c === "\"")) blankInto(out, src, i, end);
      i = end;
      continue;
    }
    i++;
  }
  return out.join("");
}

function maskCMake(src) {
  const out = src.split("");
  const n = src.length;
  let i = 0;
  while (i < n) {
    const c = src[i];
    if (c === "\"") { i = quotedEnd(src, i, "\""); continue; }
    if (c === "[") {
      const m = /\[(=*)\[/y;
      m.lastIndex = i;
      const hit = m.exec(src);
      if (hit) {
        const close = src.indexOf(`]${hit[1]}]`, i + hit[0].length);
        i = close < 0 ? n : close + hit[1].length + 2;
        continue;
      }
    }
    if (c === "#") {
      const m = /#\[(=*)\[/y;
      m.lastIndex = i;
      const hit = m.exec(src);
      let end;
      if (hit) {
        const close = src.indexOf(`]${hit[1]}]`, i + hit[0].length);
        end = close < 0 ? n : close + hit[1].length + 2;
      } else {
        end = src.indexOf("\n", i);
        if (end < 0) end = n;
      }
      blankInto(out, src, i, end);
      i = end;
      continue;
    }
    i++;
  }
  return out.join("");
}
