# パッケージマネージャ — Ludellus-Native L2 / L3 の確認欄を PKG-15 の仕様に合わせる

- 日付: 2026-10-15
- タスク参照: `actio:2fc21e95-7741-4da2-9e5f-12f62fe31d0d` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §5.2、[`spec/feature/package-hub.md`](../feature/package-hub.md) の PKG-15
- ブランチ: `docs/package-manager-l2-check-fix`
- 種別: 文書のみ (コード・テストの変更なし)

## 経緯

Ludellus-Native の L2 (Ludellus-Native 側の local PR #3028) で、確認欄の「PKG-15 (`PICTOR_BUILD_*` の FORCE) が記録される」を満たせなかった。
PKG-15 (キャッシュ) は、ホストと結線が同じキャッシュ変数を **別の値で** 設定したときだけ記録される
(`cmake/packages/ErgoPackageRules.cmake` の `ergo_pkg_rule_collisions`、結線の値は `cmake/packages/ErgoPackageWiring.cmake` の
`_ergo_pkg_wire_collisions` が渡し、機能のキャッシュは選ばれた機能の分だけ)。Ludellus-Native の FORCE は次のとおり、どれも値がぶつからない。

| 変数 | Ludellus-Native の FORCE | 比べられ方 |
|---|---|---|
| `PICTOR_BUILD_DEMO` / `PICTOR_BUILD_TESTS` | `OFF` | Ergo のマニフェストの既定と同じ値なので記録しない |
| `PICTOR_ENABLE_RIVE` | `OFF` | 既定 off の機能 `rive` の中にある。選ばれていないので比べない |

記録されないのが正しい。直すのは確認欄の書き方の方。

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | L2 の確認欄: PKG-15 は値がぶつかるときだけ記録され、Ludellus-Native の現状の値では記録されないと書く。代わりに確かめる内容を `mode=M1 (pictor=provided)`・PKG-05・`host.ergo` の drift にする | `spec/feature/package-manager.md` §5.2 |
| 2 | L3 の確認欄: 「PKG-15 が消える」を、ホストの CMake から Pictor の `add_subdirectory` と `PICTOR_BUILD_*` の `FORCE` が消えたこと (`git grep` で 0 件)・`mode=M2 (pictor=source)`・描画ターゲットと compile definitions が L2 と同じ、に置き換える | `spec/feature/package-manager.md` §5.2 |
| 3 | PKG-15 の説明に「値が同じなら記録しない」(と、選ばれていない機能は比べないこと) を 1 行足す | `spec/feature/package-hub.md` (PKG 表の後) |

## 受け入れ条件 (契約書式)

- C-1 package-manager.md §5.2 L2: 確認欄が PKG-15 の記録を求めず、`mode=M1 (pictor=provided)`・PKG-05・`host.ergo` の drift を確かめる
- C-2 package-manager.md §5.2 L3: 確認欄が「PKG-15 が消える」を使わず、`add_subdirectory` / `PICTOR_BUILD_*` の `FORCE` の `git grep` 0 件・`mode=M2 (pictor=source)`・L2 と同じ描画ターゲットと compile definitions を確かめる
- C-3 package-hub.md: PKG-15 のキャッシュ変数は値が同じなら記録しないと書いてある
- C-4 差分: 開発機の絶対パスと秘匿語を含まない (秘匿語辞書との照合で 0 件)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意しない。文書だけの変更で、Augur の `contract-wrap` が包む
TypeScript の関数が無い。上の条件は差分のレビューで確かめる。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| package-hub.md の PKG-15 の表の行 | 採用 (行は変えず、表の後に注記を 1 行) | 表の行は「別値で FORCE」とすでに書いており、意味は変わらない。抜けていた「同じ値は記録しない」だけを足す |
