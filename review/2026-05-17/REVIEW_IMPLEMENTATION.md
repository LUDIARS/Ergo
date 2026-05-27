# コード品質レビュー (共通) — LUDIARS/Ergo

| 項目 | 値 |
|------|-----|
| リポジトリ | LUDIARS/Ergo |
| 対象ブランチ / PR | main (HEAD: 576f2c7) |
| レビュー実施日 | 2026-05-17 |
| 対象コミット範囲 | c3996f9..576f2c7 |

---

## 1. コード品質 (Code Quality)

| 該当箇所 | 問題分類 | 説明 | 推奨修正 |
|----------|---------|------|---------|
| `tools/ergo/src/core/external.ts:43` | スタイル | `mod.default ?? mod.factory` の fallback. TypeScript duck-type なので運用 OK だが, 明示的な warning が ideal | console.warn で type check failure を明記 (現在は warn あり) |
| `tools/ergo/src/plugins/visus/ui/index.html:156-180` | マジックナンバー | zoom / scroll threshold (例 `e.deltaY * 0.01`) の hardcode | const SCROLL_ZOOM_FACTOR = 0.01 で定数化 |
| `tools/ergo/src/plugins/visus/ui/index.html:200+` | ネスト複雑性 | event handler 内 if-elseif chain (file select / editor change / button click). mid-level complexity | event delegation または switch(event.type) で可読性向上可能. 現在は可読範囲内 |
| `tools/ergo/src/plugins/visus/index.ts:65-100` | コード重複 | walk() recursive call の同期パターン. async/await 統一は + | 現在の実装でも保守性 OK |

### チェック項目

- [x] マジックナンバー・マジックストリングが使用されていないか → B. UI threshold hardcode
- [x] 過度にネストした条件分岐がないか → A. if-else は shallow
- [x] 未使用のコード・デッドコードが残存していないか → A. ts compiler strict 推奨
- [x] コピー&ペーストによる重複コードがないか (DRY 違反) → A
- [x] 変数・関数のスコープが必要以上に広くないか → A. closure scope 適切
- [x] 例外の握りつぶし (空の catch ブロック) がないか → A. catch ブロックは console.error / try-catch
- [x] 不適切な型変換・暗黙的型変換がないか → A. TypeScript strict
- [x] ログ出力が適切なレベルで記録されているか → A. console.log (info) / console.warn (warn) / console.error (error) 分離
- [x] 命名が役割を正しく表しているか → A. `loadPack`, `resolveSafe`, `listVisusFiles` 明確
- [x] 関数・メソッドが過度に長大化していないか → A. 最長 233 行 (visus/index.ts), 適切

---

## 総合評価

| # | レビュー観点 | 評価 | 重大指摘数 |
|---|------------|------|-----------|
| 1 | コード品質 | B | 0 |
