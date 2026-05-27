# 品質保証レビュー (共通) — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo |
| 対象ブランチ / PR | main (HEAD: 576f2c7) |
| レビュー実施日 | 2026-05-17 |
| 対象コミット範囲 | c3996f9..576f2c7 |

---

## 1. テスト戦略・カバレッジ (Test Strategy & Coverage)

| 評価 | 観点 | 所見 |
|------|------|------|
| B | unit テストの網羅性 | 16 C++ module × 平均 5～10 ケース (total ~13k 行). core logic 覆蔽率高. tool-side (TypeScript) unit は非存在 |
| D | integration テストの存在 | C++ module の cross-module test 有. tool-side (external plugin loader + visus routes) の integration test 欠如 |
| D | E2E テストの存在 | CLI 起動テスト非存在. WS upgrade / JSON roundtrip の end-to-end flow 無. smoke test なし |
| B | エッジケース・境界値テスト | C++ side: path edge case あり (例 empty string). visus path traversal test 未確認. JSON parse edge case 不明 |
| A | CI でのテスト自動実行 | GitHub Actions. ctest + SPIR-V compile. 全 commit で green 要求. tool-side test run 対応要望 |

### チェック項目

- [x] コアロジックに対する unit テストが存在するか → Partial. C++ modules OK. tool-side (external.ts, visus/index.ts) なし
- [x] 外部 I/O (DB, ファイル, ネットワーク) を含む integration テストがあるか → No. file I/O (visus scan/load/save) + WS broadcast の統合テスト欠如
- [x] 主要ユーザーフローを通す E2E テスト or smoke テストがあるか → No
- [x] 並行性・タイミング依存のロジックに timing-safe なテストがあるか → Partial. C++ side の actor/world_time では race test あり. tool-side WS concurrent client 未確認
- [x] 失敗系・例外系のテストが網羅されているか → Partial. C++ error path あり. tool-side file 読込失敗 / malformed JSON / root inaccessible など未テスト
- [x] CI で全テストが毎コミット green を求められているか → Yes. ctest exit code で gate
- [x] flaky test の検出・隔離プロセスがあるか → Not applicable. test count 少なく flaky 可能性低い
- [x] カバレッジ計測ツールが組み込まれていて, 目標値が定義されているか → No. coverage tool 非統合. target % undefined
- [x] モック・スタブが現実の挙動からドリフトしていないか → Yes. C++ side FMOD / dummy backend で mock 実装. drift 低い

---

## 2. ライセンス遵守・OSS 帰属表示 (License Compliance)

| 該当依存 | ライセンス | 配布形態 | 互換性評価 | 帰属表示状態 |
|---------|----------|---------|-----------|-------------|
| Hono (Node.js framework) | MIT | dynamic link | OK | 未対応 |
| ws (WebSocket library) | MIT | dynamic link | OK | 未対応 |
| tsx (TypeScript executor) | MIT | dev dependency | OK | 未対応 |
| mini-gtest (vendored test) | BSD-3-Clause | static | OK | 未対応 |
| nanosvg (vendored SVG) | Zlib | static | OK | 未対応 |
| stb (vendored utilities) | MIT / Public Domain | static | OK | 未対応 |

### チェック項目

- [x] プロジェクトのライセンスが明記されているか → No. LICENSE ファイル非存在. README に記載なし
- [x] 依存パッケージのライセンスが許諾範囲を超えていないか → OK. MIT/BSD/Zlib は permissive. GPL dependency なし
- [x] バンドル配布する OSS について `NOTICE` / `THIRD_PARTY_LICENSES` 等で帰属表示しているか → No
- [x] 商用配布前提なら CLA / DCO の運用が定まっているか → Not applicable. internal LUDIARS tool
- [x] プロプライエタリ依存が利用規約を満たしているか → N/A
- [x] 配布バイナリに copyleft 由来のコード混入が無いか → OK. no GPL/AGPL
- [x] OSS のフォントやアイコン・アセットの再配布条件を満たしているか → OK. SVG patterns は自作
- [x] AI 生成コードの取り込みについてプロジェクト方針が明文化されているか → Yes. CLAUDE.md に Claude Code attribution comment 記載

**推奨対応**:
1. LICENSE (MIT or proprietary LUDIARS) 追加
2. NOTICE / THIRD_PARTY_LICENSES ファイル作成 (Hono, ws, tsx, mini-gtest, nanosvg, stb 記載)
3. README に license section 追加

---

## 3. ドキュメント完備性 (Documentation Completeness)

| 評価 | 観点 | 所見 |
|------|------|------|
| A | README の網羅性 | 運用方針, モジュール一覧, ビルド手順, ホストアプリ取り込み方法. 充実. ただし tool 起動方法は spec/tool/ergo.md に委任 |
| A | DESIGN / アーキテクチャ図 | DESIGN.md 欠如. 但し CLAUDE.md + spec/tool/ergo.md + spec/module/*.md で運用・設計が詳細記述. 図表は README/spec に有 |
| A | API / インターフェースリファレンス | spec/tool/ergo.md で plugin I/F (Plugin interface, PluginFactory, routes/onUpgrade) 詳細記載. REST routes + WS protocol ドキュメント完備 |
| A | inline コメントの粒度 | external.ts:1-14 冒頭に概要 comment. visus/index.ts:1-18 protocol 説明. 粒度適切. C++ module に doc comment 豊富 |
| A | 開発者向け CONTRIBUTING / ランブック | CLAUDE.md で branch + PR workflow 明記. module 追加手順まで記載. ランブック十分 |

### チェック項目

- [x] README にプロジェクト概要・前提・最短起動手順があるか → Yes. README.md (1-75 行)
- [x] DESIGN.md / ADR が重要決定について残されているか → Partial. DESIGN.md ファイルなし. CLAUDE.md + spec/tool/ergo.md で代替
- [x] API (REST / gRPC / IPC / 内部 trait) のリファレンスが自動生成 or 手書きで整備されているか → Yes. spec/tool/ergo.md:49-61 で routes / WS protocol table 記載
- [x] 公開関数・公開 trait に doc コメント (`///`, JSDoc 等) が付いているか → Yes. TypeScript type definition. JSDoc in HTML UI
- [x] CHANGELOG / リリースノートが運用されているか → N/A. internal development. review/ に commit summary 記載
- [x] 障害発生時のランブック / トラブルシューティングがあるか → Partial. CLAUDE.md に error path recovery (例 main revert) のみ
- [x] サンプルコード / examples がビルド可能で陳腐化していないか → N/A. module samples は C++ header only
- [x] ドキュメントが実装と乖離していないか → Yes. CLAUDE.md update (ea259bb) recent. spec/tool/ergo.md update (9706551) PR 同時

---

## 総合評価

| # | レビュー観点 | 評価 | 重大指摘数 |
|---|------------|------|-----------|
| 1 | テスト戦略・カバレッジ | B | 1 (tool-side E2E test 欠如) |
| 2 | ライセンス遵守・OSS 帰属表示 | C | 1 (LICENSE/NOTICE 未記載) |
| 3 | ドキュメント完備性 | A | 0 |
