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
| `group` | 文字列 / なし | P1 (パッケージ版一覧) のグループ。`render` (描画) / `viz` (可視化)。無ければ `render`。検査には使わない (S3) |
| `repository` | 文字列 | 取得元の URL。末尾名 (`.git` を除く) が兄弟チェックアウトのディレクトリ名 (`Pictor`) |
| `rev` | 文字列 | **40 桁の小文字 commit SHA だけ**。ブランチ名・短縮 SHA・タグは PKG-02 |
| `expect_tag` | 文字列 / なし | 人向け注釈。checkout にあれば pin がその子孫かを照合する (PKG-08) |
| `platforms` | 文字列配列 | `desktop` / `android` / `ios` / `emscripten` |
| `cmake.minimum` | 文字列 | パッケージが要る CMake。実行中の CMake より新しければ PKG-11 |
| `cmake.wiring` | 文字列 | 1 パッケージ 1 ファイルの結線 (`cmake/packages/<name>.cmake`)。マニフェストのあるディレクトリからの相対 (絶対パスも可) |
| `cmake.targets` | 文字列配列 | パッケージが作るターゲット。M0 ではこの有無を記録する。どの機能の `provides` にも無いものは常に必須、ある機能の `provides` にも載るもの (Pictor の `pictor` は `vulkan`) はその機能が選ばれたときだけ必須 (PKG-10) |
| `cmake.find_package` | オブジェクト / null | `{ "name": "<Pkg>" }`。インストール済み経路 (`find_package(<Pkg> CONFIG)`) を許すもの。STRICT では使わない |
| `cmake.cache` | オブジェクト | パッケージ側キャッシュ変数の既定値 (ホスト未設定なら設定、FORCE しない。ホストの別値とは PKG-15) |
| `features` | オブジェクト | 機能名 → 機能 (下表) |
| `exports` | オブジェクト | Ergo のディレクトリへ通常変数として渡す名前付きパス (パッケージのルートからの相対、渡すのは絶対パス) |
| `requires` | オブジェクト | 依存する他の link パッケージ名をキーに持つ。未選択なら PKG-11、循環は PKG-12 |

### `features.<feature>`

| キー | 型 | 意味 |
|---|---|---|
| `default` | `"auto"` / `"on"` / `"off"` | `auto` は `platforms` が空か、今の platform を含むとき ON。それ以外の値は PKG-13 |
| `platforms` | 文字列配列 | 機能が成り立つ platform。外れた platform で選ぶと PKG-13 |
| `cache` | オブジェクト | 有効時にパッケージ側へ流す変数 (結線のスコープ内の通常変数。`option()` は CMP0077 NEW で従う) |
| `provides` | 文字列配列 | 有効時に存在すべきターゲット。空でない機能を「描画 feature」として扱う |
| `contract` | 文字列 | `provides` の先頭ターゲットが `INTERFACE_COMPILE_DEFINITIONS` に公開すべき定義 (例 `PICTOR_HAS_VULKAN`、`=値` 付きも可)。無ければ PKG-10 |
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
| `group` | 文字列 / なし | P1 のグループ (`tool`)。検査には使わない (S3) |
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
| `path_hint` | 文字列 | `ERGO_PACKAGES_SIBLING_ROOT` (または CLI の `--root`) からの相対。そこだけを読む (S4、 `ergo_package_scan`)。絶対パス・`..`・`:` を含むものは範囲外として読まず「未観測」 |
| `group` | `"musa"` / `"related"` | 図のグループ |
| `relations` | `[{ "from", "kind" }]` | Ergo 側の起点と関係の種類 (`shares-vocabulary` / `port-of`)。図では破線。他の `kind` は辺にせず注記に残す |

## 検査の記録形式 (findings)

規則 (`cmake/packages/ErgoPackageRules.cmake`) は 1 件を
`<id>|<severity>|<subject>|<message>` の文字列で返し、resolved.json では
`{ "id", "severity", "subject", "message" }` になる。

- `id`: `PKG-01`〜`PKG-17` (統合設計 §4)、マニフェスト自体の不備は `MANIFEST`
- `severity`: `error` / `warning` / `record`
- `subject`: パッケージ名、`<package>/<feature>`、モジュール名、またはキャッシュ変数名

構成時に判定するのは PKG-01〜16 (S1: 01 / 02 / 06 / 11 / 12 / 13 / 14 / 15、S2: 03 / 04 / 05 / 07 / 08 / 09 / 10 / 16。
規則は `cmake/packages/ErgoPackageRules.cmake` と `ErgoPackageRulesAcquire.cmake`)。PKG-17 は可視化段 (S3)。

## `<build>/ergo-packages.resolved.json`

構成の最後 (Ergo を含むディレクトリの末尾、`cmake_language(DEFER)`) に `file(CONFIGURE)` で 1 回書く
(取り込みの段階で error があり include 時点で止める場合は、そのときに 1 回書く)。
時刻を持たず、キーと配列はすべて整列済みなので **同じ構成なら同じバイト列** になり、内容が同じなら書き直さない。

| キー | 意味 |
|---|---|
| `schema` | `1` |
| `mode` | `M0` (`ERGO_PACKAGES` 未設定) / `M1` (選んだパッケージがすべてホスト供給) / `M2` (すべてハブが取り込み) / `M1+M2` |
| `strict` | `ERGO_PACKAGES_STRICT` |
| `fetch` | `ERGO_PACKAGES_FETCH` |
| `manifest_extra` | `ERGO_PACKAGE_MANIFEST_EXTRA` の絶対パス (なければ null) |
| `platform` | `desktop` / `android` / `ios` / `emscripten` |
| `cmake_version` | 実行中の CMake |
| `manifest` | 読んだマニフェスト (Ergo ルートからの相対) |
| `selected` | `ERGO_PACKAGES` の中身 |
| `packages.<n>` (link) | `pin` / `expect_tag` / `rev_override` / `state` / `source_dir` / `imported` / `targets_present` / `features` / `version`、S2: `route` (`host` / `provided` / `source` / `sibling` / `installed` / `fetch` / `none`) / `mode` (`M1` / `M2`) / `head` / `drift` (`none` / `ahead` / `behind` / `diverged` / `unknown`) / `dirty` / `exports` (`<VAR>=<絶対パス>`)、S3: `group` / `requires` (マニフェストの `requires` のキー、整列済み) |
| `packages.<n>` (tool) | `path` / `gitlink` / `initialized` / `state` (= `acquire`)、S3: `group` |
| `modules.<m>` | `enabled` (ターゲットの有無) / `needs` / `optional` / `ported_from` / `reason` |
| `observe.<o>` | マニフェストの写し |
| `findings` | 上の形式の配列 (整列済み) |

link パッケージの `state`:

| 値 | 意味 |
|---|---|
| `host-unmanaged` | Ergo を include する前にホストが作っていた (M0 の記録対象) |
| `hub-managed` | ハブが取り込んだ (`source` / `sibling` / `installed` / `fetch`) |
| `module-found` | include の後に Ergo のモジュール節が見つけた (例: `ergo_tela_graph` の `find_package(Tela)`) |
| `absent` | 構成の終わりまで `cmake.targets` のどれも無い |

## File API

ハブは `<build>/.cmake/api/v1/query/client-ergo/query.json` に codemodel v2 の問い合わせを置く。
返答は generate の後に `<build>/.cmake/api/v1/reply/` にでき、可視化 (S3) と PKG-17 が読む。
生成されるビルドそのものは変わらない。

読み手は `ergo_package_model` (`spec/module/package_model.md`)。最新の `index-*.json` の `client-ergo` の
応答 (無ければ `objects`) から codemodel を取り、`version.major` が 2 でなければ明示エラーにする。
ターゲットは定義したディレクトリの最長一致でパッケージへ帰属させ (どれにも当たらなければ host、ソースの外や
ビルドツリーの中は project 名の第三者)、テスト・ダミー・ベンチ・`ergo_gtest_main` を既定で除外する。

## PKG-17 (宣言漏れ、構成時には判定しない)

File API の直接の依存のうち「Ergo または link パッケージのターゲット → 別の link パッケージのターゲット」で、
Ergo 側はターゲット名に最長一致するモジュール (`<module>` か `<module>_…`) の `needs` / `optional`、link 側は
`requires` に相手が無いもの。図 (P1 のパッケージ札・P2 のターゲット札) に印を付け、
`ergo_package_viz <build> --check` が 1 件以上で終了コード 1 を返す (CI の 1 段)。
