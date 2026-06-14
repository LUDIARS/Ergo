# Vinculum — コード ↔ 仕様 トレーサビリティ KG (決定的キャッシュ)

> 作業名 **Vinculum** (仮称、= コードと仕様の結合)。ソースコードと仕様を結びつけ、
> 決定的にキャッシュ可能なトレーサビリティを提供する。正式名・略称は `project-codes` で別途確定。
>
> **土台は Anatomia** (`ergo/spec/tool/anatomia.md`) の Anchor ID (正規化 Merkle ハッシュ)。
> 本書はその上に「コード ↔ 仕様」の結合層だけを定義する (Anatomia から分離)。

---

## 1. 目的とスコープ

「どのコードがどの仕様クレームを実装しているか」を双方向に・決定的に引けるようにする。

- 用途: AI への「仕様 + 実装」セット文脈の供給、仕様カバレッジ可視化、設計逸脱の検出。
- 前提: コード側は Anatomia の Merkle-AST から得た **Anchor ID** で識別済み (§2)。
- 本書が扱うのは **結合 (binding) の生成・保管・配信** のみ。コードの構造解析・機構抽出・
  実行トレースは Anatomia 側。

---

## 2. Anatomia との関係 (土台)

| Anatomia が提供 | Vinculum が利用 |
|---|---|
| Merkle-AST / 正規化 Anchor ID | コード側ノードの安定 ID |
| 機構カード | 結合の粒度 (機構単位の束) |
| 決定的キャッシュ原理 | 束のキャッシュ安定性 |

Vinculum は Anatomia の **下層 DAG (コンテンツアドレスの不変構造) を真実**とし、その上に
仕様ノードと結合辺を足す。

---

## 3. なぜ KG (射影) か / なぜ embedding-RAG でないか

「キャッシュ機構」前提では **決定的 retrieval が必須**。

> embedding 検索は入力が少し変わると回収結果が揺れる → 文脈束が揺れる → `cache_read` が死ぬ。
> コード ↔ 仕様の結合は本質的に **グラフ (多対多・双方向・traversable)**。グラフ探索は決定的。
> よってこの条件では KG (グラフ) が背骨で、embedding は結合の *信号* に留める。

ただし **KG は結合を保管・配信するだけで、生成はしない**。価値の大半はリンカ (§5) の質で決まる。
KG 単体を「最適」と見ると本丸 (リンカ) を見落とす。

---

## 4. データモデル

三部グラフ。

```
[CodeUnit]  ──implements/describes──▶  [SpecClause]
   │ Anchor ID (Anatomia 由来)              │ clause ID + 本文 + embedding
   │ 機構カード参照                          │ 出自 (spec/*.md, DESIGN.md, …)
   └────────────── 辺: confidence + evidence ┘
```

- **CodeUnit**: Anatomia の Anchor ID で識別 (関数・System・機構カード単位)。決定的。
- **SpecClause**: 仕様の段落/箇条 1 件。本文 + embedding + 出自を持つ。
- **辺 (implements / describes)**: 各辺は **confidence (0–1)** と **evidence (明示/構造/意味)** を持つ。
  「ハードリンクか推測か」が常に分かる。embedding は SpecClause 内のリンク信号として使う
  (競合する保管庫にしない)。

---

## 5. 3 段リンカ (結合の生成)

異種混合 (構造的コード × 自然言語仕様) を 3 段で当てる。

| 段 | 手法 | 強さ | 鮮度 |
|---|---|---|---|
| 明示 | コードに仕様 ID / 仕様にシンボル参照 (`@implements SPEC-123`, `tier-routing.ts` 等) | 最強・確実 | 安い |
| 構造 | 命名・配置のヒューリスティック | 中 | 中 |
| 意味 | embedding/LLM で段落 ↔ コードを照合 | 弱・曖昧 | 高い |

明示 > 構造 > 意味。意味リンクは「提案」に過ぎず、確定ではない。

---

## 6. 硬化ループ (Hardening Loop)

結合を時間とともに決定的にする中核機構。

```
意味リンカが低 confidence の辺を提案
   → 人間/CI が批准
   → 明示リンク (高 confidence・変更時に安く検証可) へ昇格
   → KG が "硬化" する
```

- 使うほど結合が決定的になり、retrieval が安定 → **キャッシュ命中率が上がる**。
- 明示リンクは変更時に Anchor ID の照合だけで検証できる (安い)。
- **コード ↔ 仕様の結合が、自己強化するキャッシュになる**。

---

## 7. KG 基盤 (射影)

- DAG (Anatomia の不変構造) が真実、**KG (Kuzu) はその上の問い合わせ用 materialized view**。
  可変グラフを別途バージョン管理しない。
- traceability クエリ (「§4 を実装する全コード」「System X に触る全仕様」) は graph 探索で。
- 集計・近傍は Kuzu。複雑な導出ルール (将来) は **query 層を抽象化**して Datalog 風層を差せるように
  (CodeQL/Glean/Soufflé の先行技術)。

---

## 8. 決定的キャッシュ束

- **「仕様 + 実装」束 = `f(query, DAG スナップショット)` の純関数**。安定ソートで決定的に組む。
- 同じ問い → 同じ束 → prefix 安定 → `cache_read` が効く。
- 変更時は Merkle で影響範囲だけ無効化 (Anatomia と同じ Anchor で連動)。
- 束は「不変 (仕様クレーム本文 + 機構カード) を前、可変 (今回の問い) を後ろ」に並べる
  → `@ludiars/llm-gateway` の `orderSegments` をそのまま使う。

---

## 9. 留保・未決定

- **小規模なら過剰**。コードも仕様も小さいなら、`@implements` 注釈の平坦な索引や
  その場 grep (エージェント検索) で十分。KG の元が取れるのは「大きいコード × 大きい仕様 ×
  多対多 × 繰り返し問い合わせ × キャッシュしたい」が揃った時。
- **未決定**: KG 基盤 (Kuzu 採否)、明示注釈の規約 (`@implements` の書式)、
  硬化ループの批准フロー (人間 / CI / 両方)。

---

## 10. 参照

- 土台: `ergo/spec/tool/anatomia.md` (Merkle-AST / Anchor ID / 機構カード / 実行トレース)
- トークン節約原理: `Lapilli/packages/llm-gateway/DESIGN.md` (orderSegments / 決定的束)
- 先行技術: CodeQL / Glean / Soufflé (コード = 事実 + 再帰ルールによるトレーサビリティ)
