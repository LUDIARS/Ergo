# ergo-packages.json — パッケージ・ハブのマニフェスト (schema 1)

- 正本の設計: [`spec/feature/package-hub.md`](../feature/package-hub.md) §2 / §4
- ホスト向け手順: [`spec/setup/packages.md`](../setup/packages.md)
- 置き場所: リポ直下 `ergo-packages.json`。`module_list.yaml` と並ぶ機械処理用一覧
- 読む側: CMake (`cmake/packages/ErgoPackageManifest.cmake`、`string(JSON)`)。後段で C++ (`ergo_package_model`) と Node も同じファイルを読む

マニフェストは **ロックを兼ねる**。別のロックファイルは作らない。構成結果は
`<build>/ergo-packages.resolved.json` (追跡しない) に書かれる。

## トップレベル

| キー | 型 | 意味 |
|---|---|---|
| `schema` | 数値 | `1` 固定。違えば `MANIFEST` の error を記録する |
| `packages` | オブジェクト | 名前 → パッケージ。`kind` で link / tool を分ける |
| `modules` | オブジェクト | Ergo のモジュール名 → そのモジュールが要るパッケージの宣言 |
| `observe` | オブジェクト | 読むだけの関連リポ (MUSA 系・Curare)。固定も検査もしない |

## `packages.<name>` (kind: `link`)

ホストにリンクされる LUDIARS 製 C++ / CMake パッケージ (今は Pictor / Tela)。

| キー | 型 | 意味 |
|---|---|---|
| `kind` | `"link"` | |
| `code` | 文字列 | LUDIARS のプロジェクトコード (図の札用) |
| `repository` | 文字列 | 取得元の URL (取得は S2) |
| `rev` | 文字列 | **40 桁の小文字 commit SHA だけ**。ブランチ名・短縮 SHA・タグは PKG-02 |
| `expect_tag` | 文字列 / なし | 人向け注釈。checkout にあれば pin がその子孫かを照合する (PKG-08、S2) |
| `platforms` | 文字列配列 | `desktop` / `android` / `ios` / `emscripten` |
| `cmake.minimum` | 文字列 | パッケージが要る CMake。実行中の CMake より新しければ PKG-11 |
| `cmake.wiring` | 文字列 | 1 パッケージ 1 ファイルの結線 (`cmake/packages/<name>.cmake`、S2) |
| `cmake.targets` | 文字列配列 | パッケージが作るターゲット。M0 ではこの有無を記録する |
| `cmake.find_package` | オブジェクト / null | `{ "name": "<Pkg>" }`。インストール済み経路を許すもの (S2) |
| `cmake.cache` | オブジェクト | パッケージ側キャッシュ変数の既定値 (ホスト未設定なら設定、FORCE しない。S2) |
| `features` | オブジェクト | 機能名 → 機能 (下表) |
| `exports` | オブジェクト | 親スコープへ渡す名前付きパス (ソースからの相対。S2) |
| `requires` | オブジェクト | 依存する他の link パッケージ名をキーに持つ。未選択なら PKG-11、循環は PKG-12 |

### `features.<feature>`

| キー | 型 | 意味 |
|---|---|---|
| `default` | `"auto"` / `"on"` / `"off"` | `auto` は `platforms` が空か、今の platform を含むとき ON。それ以外の値は PKG-13 |
| `platforms` | 文字列配列 | 機能が成り立つ platform。外れた platform で選ぶと PKG-13 |
| `cache` | オブジェクト | 有効時にパッケージ側へ流すキャッシュ変数 (S2) |
| `provides` | 文字列配列 | 有効時に存在すべきターゲット。空でない機能を「描画 feature」として扱う |
| `contract` | 文字列 | ターゲットが公開する compile definition (例 `PICTOR_HAS_VULKAN`) |
| `requires` | 文字列配列 | 同じパッケージの機能名 (または `<package>` / `<package>/<feature>`)。未選択は PKG-11、循環は PKG-12 |
| `conflicts` | 文字列配列 | 同時に選べない機能名。両方選ぶと PKG-13 |
| `reserved` | 真偽 | 名前だけの予約。選ぶと PKG-13。Pictor の webgpu / d3d12 / metal は multi-RHI の段階に合わせて外す |

ハブは描画方式の可否を判定しない。選ばれた機能を CMake 変数へ流すだけ。

## `packages.<name>` (kind: `tool`)

取得するがリンクしないもの (今は Lapilli)。

| キー | 型 | 意味 |
|---|---|---|
| `kind` | `"tool"` | |
| `code` | 文字列 | プロジェクトコード |
| `acquire` | `"submodule"` | 取得方法。既存 submodule を維持する |
| `path` | 文字列 | Ergo からの相対パス。版は **マニフェストに書かず**、Ergo ツリーの gitlink SHA を resolved.json に記録する |

## `modules.<module>`

今 `CMakeLists.txt` に `if(TARGET pictor)` として散らばる暗黙の依存を、検査と図に出すための宣言。

| キー | 型 | 意味 |
|---|---|---|
| `needs` | 文字列配列 | 無いのにモジュールが ON なら PKG-14 (M0 では記録だけ) |
| `need_kind` | `"real-path"` | `needs` の意味を「ターゲットがある」から「実描画経路が確保できた」へ強める |
| `real_path_var` | 文字列 | `real-path` の判定に使う変数名 (ergo_render は `ERGO_RENDER_BACKEND_ENABLED`) |
| `reason_var` | 文字列 | resolved.json の `reason` に写す変数名 (ergo_render は `ERGO_RENDER_BACKEND_REASON`) |
| `optional` | 文字列配列 | あれば使う。検査しない |
| `when` | 文字列 | `optional` を使う条件のオプション名。resolved.json の `reason` に `<名>=ON/OFF` で写す |
| `ported_from` | `[{ "package", "rev" }]` | 移植元。版のずれ判定は後回し (U5) |

`real_path_var` / `reason_var` は統合設計 §2 の見本に無い追記で、「`real-path` は既存
`ergo_render_enforce_backend` の規則に従う」「`ERGO_RENDER_BACKEND_REASON` などを写す」を
エンジンに具体名を持たせずに実現するためのもの。

## `observe.<name>`

| キー | 型 | 意味 |
|---|---|---|
| `code` | 文字列 | プロジェクトコード |
| `path_hint` | 文字列 | `ERGO_PACKAGES_SIBLING_ROOT` (または CLI の `--root`) からの相対。そこだけを読む (S4) |
| `group` | `"musa"` / `"related"` | 図のグループ |
| `relations` | `[{ "from", "kind" }]` | Ergo 側の起点と関係の種類 (`shares-vocabulary` など)。図では破線 |

## 検査の記録形式 (findings)

規則 (`cmake/packages/ErgoPackageRules.cmake`) は 1 件を
`<id>|<severity>|<subject>|<message>` の文字列で返し、resolved.json では
`{ "id", "severity", "subject", "message" }` になる。

- `id`: `PKG-01`〜`PKG-17` (統合設計 §4)、マニフェスト自体の不備は `MANIFEST`
- `severity`: `error` / `warning` / `record`
- `subject`: パッケージ名、`<package>/<feature>`、モジュール名、またはキャッシュ変数名

S1 で判定するのは PKG-01 / 02 / 06 / 11 / 12 / 13 / 14 / 15。

## `<build>/ergo-packages.resolved.json`

構成の最後 (Ergo を含むディレクトリの末尾、`cmake_language(DEFER)`) に `file(CONFIGURE)` で 1 回書く。
時刻を持たず、キーと配列はすべて整列済みなので **同じ構成なら同じバイト列** になり、内容が同じなら書き直さない。

| キー | 意味 |
|---|---|
| `schema` | `1` |
| `mode` | `M0` (`ERGO_PACKAGES` 未設定) / `selected` (設定済み。S1 は検査と記録だけ) |
| `strict` | `ERGO_PACKAGES_STRICT` |
| `platform` | `desktop` / `android` / `ios` / `emscripten` |
| `cmake_version` | 実行中の CMake |
| `manifest` | 読んだマニフェスト (Ergo ルートからの相対) |
| `selected` | `ERGO_PACKAGES` の中身 |
| `packages.<n>` (link) | `pin` / `expect_tag` / `rev_override` / `state` / `source_dir` / `imported` / `targets_present` / `features` / `version` |
| `packages.<n>` (tool) | `path` / `gitlink` / `initialized` / `state` (= `acquire`) |
| `modules.<m>` | `enabled` (ターゲットの有無) / `needs` / `optional` / `ported_from` / `reason` |
| `observe.<o>` | マニフェストの写し |
| `findings` | 上の形式の配列 (整列済み) |

link パッケージの `state`:

| 値 | 意味 |
|---|---|
| `host-unmanaged` | Ergo を include する前にホストが作っていた (M0 の記録対象) |
| `module-found` | include の後に Ergo のモジュール節が見つけた (例: `ergo_tela_graph` の `find_package(Tela)`) |
| `absent` | 構成の終わりまで `cmake.targets` のどれも無い |

## File API

ハブは `<build>/.cmake/api/v1/query/client-ergo/query.json` に codemodel v2 の問い合わせを置く。
返答は generate の後に `<build>/.cmake/api/v1/reply/` にでき、可視化 (S3) と PKG-17 が読む。
生成されるビルドそのものは変わらない。
