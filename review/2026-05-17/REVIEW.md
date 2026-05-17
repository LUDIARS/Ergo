# AI Code Review — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo (E:\Document\Ars\ergo) |
| 対象ブランチ / PR | main (HEAD: 576f2c7) |
| レビュー実施日 | 2026-05-17 |
| 対象コミット範囲 | c3996f9..576f2c7 (3 commits, 2026-05-15～2026-05-17) |
| 対象期間の変更概要 | 外部プラグインシステム安定化 + ui_kit/shuriken_migrator 復元 + visus plugin 追加 |

---

## 総合評価表

| # | レビュー観点 | 評価 | 重大指摘数 | 所見 |
|---|------------|------|-----------|------|
| 1 | 脆弱性 | A | 0 | path traversal 防止厳格. WS/HTTP input validation 堅牢. DoS 耐性確保 |
| 2 | 設計強度 | A | 0 | plugin pack 分離明確. single responsibility 徹底. 障害分離・縮退設計完備 |
| 3 | 設計思想の一貫性 | A | 0 | main aggregation, ブランチ運用 (feat/PR 必須) 統一. plugin contract 遵守 |
| 4 | モジュール分割度 | A | 0 | external.ts が plugin ロード責務を隔離. registry.ts が builtin 管理. 凝集度高い |
| 5 | コード品質 | B | 0 | TypeScript + tsx loader 統一. error path 適切. わずかな style 小改善点あり |
| 6 | データスキーマ | A | 0 | JSON validate 済. visus 構造化フォーム化予定 (Phase 2). path relative 表現統一 |
| 7 | セキュリティ | A | 0 | path traversal 強固 (resolveSafe + relative check). ファイルアクセス要求制限 |
| 8 | テスト戦略・カバレッジ | B | 1 | unit 13 module × 複数ケース. E2E/integration tool-side 不在. CI 有 |
| 9 | ライセンス遵守 | C | 1 | LICENSE/NOTICE/THIRD_PARTY_LICENSES 未記載. Hono/ws/tsx 依存あり |
| 10 | ドキュメント完備性 | A | 0 | spec/tool/ergo.md 詳細. plugin contract 明記. external plugin ガイド完備 |
| 11 | パフォーマンス・ベンチマーク | B | 0 | ベンチ基盤あり (opt-in). tool-side WS broadcast リアルタイム. 性能目標なし |
| 12 | クロスプラットフォーム | B | 0 | CI Ubuntu のみ. Windows/macOS/ARM64 未テスト. Electron main.cjs platform 判別不十分 |
| 13 | 入力バリデーション・エラー対応 | A | 0 | visus: roots index 検証. path traversal 検証. JSON parse 失敗 graceful skip |
| 14 | 機能改善提案 | — | — | visus UI Phase 2 (構造化フォーム). plugin pack error recovery. |
| 15 | 不足機能 | — | — | tool-side E2E test. plugin discovery logging. network-local firewall check. |
| 16 | 開発者体験 | A | 0 | CLAUDE.md 明確. npm run dev/serve/start 選択肢. tsx ESM loader 透過的 |

---

## 重大指摘の優先度別集計

| 重大度 | 件数 | 内容 |
|--------|------|------|
| Critical | 0 | — |
| High | 1 | LICENSE / NOTICE / THIRD_PARTY_LICENSES ファイル未記載 |
| Medium | 1 | E2E/integration テスト (tool 側) 欠如 |
| Low | 2 | CI: Windows/macOS/ARM64 マトリクス未実装. plugin pack error logging 冗長性 |

---

## プロジェクトスナップショット

**直近 3 コミット (2026-05-15～17) の成果**:
- external plugin loading (#19: 9706551) — plugin pack 動的ロード, AdventureCube/KuzuSurvivors 固有 plugin 分離, core 負荷軽減
- ui_kit / shuriken_migrator 復元 (#21: c3996f9) — PR cleanup 後の再統合
- visus plugin (576f2c7) — Pictor visus asset エディタ (Phase 1: JSON 直編集)

**モジュール総数**: 16 本 + shared lib (ergo_common)

**Tool Side**: 統合 Ergo server (Hono + Electron). builtin plugin (particle / variable / visus). external pack ロード (ERGO_PLUGIN_DIR)
