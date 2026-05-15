# 不足機能評価 (Missing Feature Evaluation) — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo |
| 対象ブランチ / PR | main (HEAD: ecef95c) |
| レビュー実施日 | 2026-05-15 |
| 対象コミット範囲 | 0d8def8..ecef95c |

---

## 1. 機能の改善提案 (Feature Improvement)

| 対象機能 | 改善提案 | 期待効果 | 優先度 |
|---------|---------|---------|--------|
| uidoc.json round-trip | Enum 値の serialize で文字列化が冗長 | JSON size 削減、encode/decode 統一化 | Low |
| UIContext::find() | path string split を毎呼出し実行 | Trie/hash map キャッシュで O(1) lookup | Low |
| LayoutSpec padding | 4 辺を pad_l/t/r/b で個別指定 | Figma の unified/symmetric padding 対応 | Medium |
| error reporting | parse_document() で bool のみ | JSON syntax error の行番号・メッセージを返却 | Medium |
| large scene perf | 1000+ node tree での layout solve 未計測 | benchmark 追加、O(n) 保証確認 | Medium |

---

## 2. 不足機能の提案 (Missing Feature Proposal)

| 提案機能 | 必要性の根拠 | 実装優先度 | 想定影響範囲 |
|---------|------------|-----------|------------|
| 入力ディスパッチ (P3) | spec/module/ui_framework.md に明記 | High | ergo_ui_kit + ergo_input 横断 |
| プロパティアニメーション (P3) | Figma Smart Animate 相当 | High | UIContext に animator 追加 |
| Text layout エンジン (P2) | テキストレイアウト (wrap/truncate) 未実装 | High | ergo_ui との連携 |
| Figma REST インポータ (P2) | Figma REST API → uidoc 変換 | High | 新規モジュール |
| UIRenderer Pictor 実装 (P2) | Vulkan で描画する Pictor パイプライン | High | Pictor side |
| Component/Instance オーバーライド (P2) | Figma Instance の slot/property override | Medium | Node 構造体拡張 |
| Constraint solver | nested layout の correctness | Medium | test 拡張 |
| Performance profiler UI | レイアウト解決・描画リスト生成の bottleneck 可視化 | Low | debug feature |

---

## 総合評価

| # | レビュー観点 | 指摘数 | 優先度別内訳 |
|---|------------|--------|------------|
| 1 | 機能改善 | 5 | High: 1, Medium: 2, Low: 2 |
| 2 | 不足機能 | 8 | High: 4, Medium: 2, Low: 1 |
