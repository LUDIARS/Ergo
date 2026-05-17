# 不足機能評価 (共通) — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo |
| 対象ブランチ / PR | main (HEAD: 576f2c7) |
| レビュー実施日 | 2026-05-17 |
| 対象コミット範囲 | c3996f9..576f2c7 |

---

## 1. 機能の改善提案 (Feature Improvement)

| 対象機能 | 改善提案 | 期待効果 | 優先度 |
|---------|---------|---------|--------|
| visus plugin UI | Phase 2: JSON エディタから構造化フォーム (geometry / materials CRUD) への移行 | UX 向上. validation 強化. typo 削減 | High |
| external plugin loading | error recovery: broken pack は retry or reload-on-demand 機能 | 開発時の edit-reload cycle 高速化 | Medium |
| WS broadcast | plugin discovery announce (新 plugin load → client refresh) | plugin list 動的更新 (現在は restart 必須) | Medium |
| tool-side logging | plugin pack load 結果の詳細 logging (成功/失敗別件数) | 起動診断性向上. CI log 分析用 | Low |

---

## 2. 不足機能の提案 (Missing Feature Proposal)

| 提案機能 | 必要性の根拠 | 実装優先度 | 想定影響範囲 |
|---------|------------|-----------|------------|
| E2E テスト (tool-side) | plugin routes / WS upgrade の integration test 欠如. CLI tool test で coverage 未定義 | High | tools/ergo/, spec/tool/ergo.md |
| plugin pack dependency 宣言 | 現在は直列ロード. plugin A → plugin B に依存する場合の定義なし | Medium | spec/tool/ergo.md + external.ts |
| cross-platform CI (Windows/macOS/ARM64) | Ubuntu only. native file path (sep, delimiter) のバグ検知不可 | High | GitHub Actions config |
| benchmarks (tool-side) | Node WS throughput / JSON serialize latency の baseline 測定なし | Low | tools/ergo/ |

---

## 総合評価

| # | レビュー観点 | 指摘数 | 優先度別内訳 |
|---|------------|--------|------------|
| 1 | 機能改善 | 4 | High: 1 / Medium: 2 / Low: 1 |
| 2 | 不足機能 | 4 | High: 2 / Medium: 1 / Low: 1 |
