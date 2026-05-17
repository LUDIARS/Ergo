# 設計レビュー (共通) — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo |
| 対象ブランチ / PR | main (HEAD: 576f2c7) |
| レビュー実施日 | 2026-05-17 |
| 対象コミット範囲 | c3996f9..576f2c7 |

---

## 1. 設計強度 (Design Robustness)

| 評価 | 観点 | 所見 |
|------|------|------|
| A | 障害分離 | plugin 独立起動. external pack 読込失敗時も core 稼働継続. root ディレクトリ inaccessible 時は graceful warn + skip |
| A | 冪等性 | plugin factory は stateless closure. config (ERGO_PLUGIN_DIR, VISUS_PROJECT_ROOTS) 環境変数管理. 複数起動時 conflict なし |
| A | 入力バリデーション | visus: resolveSafe() 二重 path check (resolve + relative 検証). root index bounds 確認. JSON parse 失敗を exception で処理 |
| A | エラーハンドリング | external.ts: load 失敗時 console.error + 続行. visus: scan 失敗ファイルはスキップ (summary 欠落も許容). Hono API エラー (400/404/500) 返却 |
| A | リトライ・タイムアウト設計 | tool-side WS broadcast は非同期. save POST 即座返却. UI 側 optimistic update + reload バックアップ |
| A | 状態管理の明確性 | plugin: closure で internal state 隠蔽. WS clients set 管理. file mtime で reload detection. race condition 対策あり |

### チェック項目

- [x] 単一障害点 (SPOF) が存在しないか → No. external plugin 失敗は isolated. core/shell 独立
- [x] 外部サービス・外部リソース障害時の縮退動作が定義されているか → Yes. file I/O 失敗時 warn + skip
- [x] 入力値の境界値・異常値に対する防御が十分か → Yes. path traversal 多段検証. root index 範囲確認. JSON depth limit (前版で実装済)
- [x] エラー発生時にシステムが安全な状態に遷移するか (fail-safe) → Yes. scan 失敗 → empty list. load 失敗 → null/error API. save 失敗 → UI 表示継続
- [x] 非同期処理のタイムアウトとキャンセル機構があるか → Yes. WS upgrade は server-managed. HTTP request は Node default timeout
- [x] 競合状態 (race condition) のリスクが排除されているか → Yes. mtime-based reload + WS broadcast. file 同時編集は application-level 運用規則

---

## 2. 設計思想の一貫性 (Design Philosophy Compliance)

| 該当箇所 | 逸脱内容 | 本来の設計思想 | 推奨修正 |
|----------|---------|--------------|---------|
| — | — | main aggregation + feat/PR 必須 | — |

### チェック項目

- [x] レイヤー間の依存方向が規約通りか → Yes. core → plugin (正方向). external plugin は interface duck-type のみ
- [x] 命名規則がプロジェクト全体で統一されているか → Yes. `PluginFactory`, `PluginContext`, `onUpgrade` 統一
- [x] 共通パターン (リポジトリパターン, サービス層等) が一貫して適用されているか → Yes. plugin pack factory 統一. registry の配列走査統一
- [x] 既存のユーティリティ・ヘルパーを無視した再実装がないか → Yes. Node.js fs/path/url util 統一
- [x] 責務の配置がアーキテクチャの意図と合致しているか → Yes. external.ts はロード責務. registry.ts は登録責務. core/shell はホスト責務
- [x] 設定値のハードコーディングがないか → Yes. ERGO_PLUGIN_DIR, VISUS_PROJECT_ROOTS, PORT 環境変数化

---

## 3. モジュール分割度 / 機能的凝集度 (Cohesion & Modularity)

| モジュール / クラス | 凝集度評価 | 所見 |
|-------------------|-----------|------|
| `tools/ergo/src/core/external.ts` | 機能的 | plugin pack discovery・load. 責務単一. loadPack / loadExternalFactories の粒度適切 |
| `tools/ergo/src/core/registry.ts` | 機能的 | builtin plugin list 管理. 変更頻度低い. 機能的凝集 |
| `tools/ergo/src/plugins/visus/index.ts` | 機能的 | Visus ファイル走査・読込・保存. paths 管理, WS broadcast. 凝集度高い |
| `tools/ergo/src/plugins/visus/ui/index.html` | 論理的 | UI state (file list, editor content, validation) を JavaScript で管理. 分離可能だが Phase 1 は単一ファイル |
| `tools/ergo/src/core/plugin.ts` | interface 定義 | TypeScript interface. no logic. duck-type での契約 |

### チェック項目

- [x] 1 つのクラス・モジュールが複数の無関係な責務を持っていないか → Yes. clear separation (load / register / route / WS)
- [x] God Object / God Class が存在しないか → No
- [x] 結合度が不必要に高くないか → Low. plugin は factory pattern で独立. external pack は path-only dependency
- [x] 循環依存が発生していないか → No
- [x] インターフェースが適切に分離されているか (ISP) → Yes. Plugin interface minimal. PluginContext は ctx.env のみ
- [x] パッケージ・ディレクトリ構成がドメインの構造を反映しているか → Yes. src/core (framework) / src/plugins (builtin) / external (dynamic)

---

## 総合評価

| # | レビュー観点 | 評価 | 重大指摘数 |
|---|------------|------|-----------|
| 1 | 設計強度 | A | 0 |
| 2 | 設計思想の一貫性 | A | 0 |
| 3 | モジュール分割度 | A | 0 |
