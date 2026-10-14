# ergo_package_scan モジュール定義

## 概要

パッケージ・ハブの観測 (spec/feature/package-hub.md §5 入力 3、 段階 S4) を受け持つ読み取り専用の層。
マニフェスト (`ergo-packages.json`) の `observe` が指す兄弟チェックアウトと Ergo 自身から、 使用関係を
**根拠 (ファイル・行・キー) 付きの辺**にして `UsageModel` を返す。 図 (P3 usage) は `ergo_package_viz` が描く。

- 根拠の無い辺は作らない。 公開 npm パッケージの依存や、 マニフェストの `needs` のような宣言だけの関係は辺にしない。
- 宣言された範囲の外を読まない (総なめにしない)。 依存先が範囲外を指していても、 パスの計算で帰属させるだけで中身は開かない。
- 読めない観測対象は「未観測」ノードとして理由付きで残す。
- 情報源は兄弟チェックアウト (統合設計 N8)。 Concordia / Revisor への問い合わせ、 lock ファイル、 取得、 通知は持たない。

## カテゴリ

システム (開発ツールの下回り)

## 所属ドメイン

`observability` (`spec/domains/observability.domain.json`)。 層は `infrastructure` (`.anatomia/layers.json`)。

## 必要なデータ

| 入力 | 読む場所 | 取り出すもの |
|---|---|---|
| マニフェスト | `--manifest` (既定は resolved.json の記録) | `observe` (code / group / path_hint / relations)、 `modules.<m>.ported_from`、 tool パッケージの `path` |
| 観測対象 | `<root>/<path_hint>/package.json` | `name`、 `dependencies` / `devDependencies` のうち `file:` / `workspace:` / `@ludiars/*` |
| 観測対象 | `<root>/<path_hint>/.gitmodules` | `[submodule]` の `path` / `url` |
| 観測対象 | `<root>/<path_hint>/.git/index` | gitlink (mode 160000) の SHA。 `.git` がチェックアウトの中のディレクトリのときだけ (worktree の `.git` ファイルが指す先は範囲外なので読まない)。 index 版 2 / 3 のみ |
| Ergo | マニフェストのあるディレクトリの `.gitmodules` / `.git/index`、 `tools/ergo/package.json` (`ScanOptions::ergo_paths`) | 同上 |

## 依存

`ergo_common` (jsonm) のみ。 `ergo_package_model` とは互いに import しない (入力が違う: あちらはビルドディレクトリ、
こちらはチェックアウト)。 Pictor / Tela に依存せず常時ビルド・テストできる。

## 変数 (値型)

- `UsageNode`: `id` (観測名 / モジュール名 / `ergo` / `tools/ergo` / `npm:<名>` / `repo:<名>`)、 `kind`
  (Musa / Related / SharedTs / Ergo / Tool / Unobserved)、 `code`、 `declared_group`、 `reason` (未観測の理由)、 `package_name`。
- `UsageEdge`: `from` → `to` (使う側 → 使われる側)、 重なった `kinds`、 `evidence` (1 つ以上)。 実線の種類が 1 つでもあれば実線。
- `Evidence`: `file` (兄弟は `<path_hint>/<file>`、 Ergo は `ergo:<file>`)、 `line` (1 始まり)、 `key`
  (`submodule.<名>.url` / `dependencies.<名>` / `modules.<m>.ported_from[i]` / `observe.<o>.relations[i]`)、 `note`
  (`gitlink 1234567` / 依存の指定)。
- `UsageModel::files_read`: 実際に開いたファイル (名前順)。 範囲外を読まないことの確認に使う。

## 作業

### 入力 → 出力

`scan_usage(ScanOptions{manifest_path, ergo_root, root, ergo_paths}, UsageModel&)`。 マニフェストが読めなければ false。
観測対象が読めないのは失敗ではない。

### 辺の種類

| 種類 | 根拠 | 線 |
|---|---|---|
| `submodule` | `.gitmodules` の url 行 (+ gitlink の SHA7 を注に) | 実線 |
| `file` / `workspace` / `scoped` | `package.json` の依存の行 | 実線 |
| `port-of` | マニフェスト `modules.<m>.ported_from[i]` (または relation の kind `port-of`) | 破線 |
| `shares-vocabulary` | マニフェスト `observe.<o>.relations[i]` | 破線 |

relation の未知の kind は辺にせず `notes` に残す。 自己ループは作らない。

### 依存先の帰属 (パスの計算だけ)

`file:` は (1) 同じ読み取り元の submodule → (2) Ergo の tool パッケージ (`lib/lapilli`) → (3) `tools/ergo`・Ergo の他の場所
→ (4) 観測対象のチェックアウト → (5) 兄弟の根の直下 `<Seg>` (名前の一致する tool、 無ければ `repo:<Seg>`) → (6) `npm:<依存名>`。
submodule の url はリポ名で 観測対象 (path_hint の末尾) → tool パッケージ → `repo:<名>`。 それ以外の依存は、 読んだ
`package.json` の `name` の表 (例: `@ludiars/terpsichore` → terpsichore) → `npm:<依存名>`。

### 未観測

| 理由 | 条件 |
|---|---|
| `--root 未指定` | `root` が空 |
| `path_hint が範囲外` | 絶対パス・`..`・`:` を含む |
| `チェックアウトなし` | `<root>/<path_hint>` がディレクトリでない |

未観測でも、 マニフェストに根拠のある辺 (移植・語彙共有) は描く。

## テスト

`tests/package_scan/test_package_scan.cpp` (見本 `tests/package_scan/fixtures/` の合成した最小の `package.json` /
`.gitmodules` / マニフェスト): JSON の行索引、 `.gitmodules`、 `package.json` の依存の選別、 git index の gitlink
(テスト内で index を組み立てる)、 辺抽出と根拠、 根拠の無い辺を作らない、 範囲外を読まない (`root/Outside` は
開かれない)、 未観測の 3 理由、 決定性。 図の側は `tests/package_viz/test_usage_view.cpp`。
