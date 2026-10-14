# LUDIARS パッケージ・ハブ (ホスト向け手順)

Ergo が Pictor / Tela などの LUDIARS 製 C++ パッケージの版を 1 か所で固定し、
取り込み・照合・記録を行う仕組み。設計の正本は
[`spec/feature/package-hub.md`](../feature/package-hub.md)、マニフェストの形式は
[`spec/data/ergo-packages.md`](../data/ergo-packages.md)。

- 対象: ホストにリンクされる LUDIARS 製 C++ パッケージ (link)、取得だけするもの (tool: Lapilli)、
  読むだけの関連リポ (observe: MUSA 系・Curare)
- 対象外: 第三者ライブラリ (curl など) は従来どおり
  [`third-party-deps.md`](third-party-deps.md) の仕組み

## 必要なもの

- CMake **3.20 以上** (Ergo の `cmake_minimum_required` は 3.20。Pictor / Tela も 3.20)
- 照合 (M1 / M2) には git。git が無い・checkout でない場合は `drift: unknown` (PKG-09) になる

## モード

| モード | ホストの状態 | Ergo の動き |
|---|---|---|
| **M0 互換 (既定)** | `ERGO_PACKAGES` 未設定。今までどおりホストが Pictor などを取り込む | 取り込まない。既にあるターゲットを `host-unmanaged` として記録するだけ。**ビルド結果は変わらない** (ターゲット集合と compile definitions が同じ)。構成を止めない |
| **M1 照合** | `ERGO_PACKAGES` を設定。取り込みはまだホスト (Ergo より前にターゲットを作る) | ホスト供給経路で受け、ターゲットの `SOURCE_DIR` の git HEAD を pin と照合する (PKG-05) |
| **M2 委譲** | ホストは自前の取り込み・SHA 検証・`PICTOR_BUILD_*` の FORCE を消す | ハブがローカルソース / 兄弟チェックアウト / インストール済み / 取得で取り込む |

モードはパッケージごとに決まる (resolved.json の `packages.<n>.mode`)。全体の `mode` は
`M0` / `M1` / `M2` / `M1+M2` (両方が混ざる)。

## 解決の順序 (最初に当たった経路を使う)

| # | 経路 (`route`) | 条件 | 備考 |
|---|---|---|---|
| 1 | `host` | 宣言ターゲット (`cmake.targets` / feature の `provides`) が Ergo の include 時点で既にある | M1。`SOURCE_DIR` の checkout を照合。imported ターゲットは SHA を読めず `unknown` |
| 1' | `provided` | `ERGO_PACKAGE_PROVIDED_<NAME>=ON` | M1。ハブは取り込まない。ターゲットが Ergo より後にできる場合はディレクトリ末尾で検証 |
| 2 | `source` | `ERGO_PACKAGE_SOURCE_DIR_<NAME>=<dir>` | M2。`<dir>/CMakeLists.txt` が無ければ PKG-03 で止まる (次の経路へ進まない) |
| 3 | `sibling` | `ERGO_PACKAGES_SIBLING_ROOT=<root>` | M2。`<root>/<Repo>` だけ (`Repo` は `repository` の末尾名、例 `Pictor`)。`.worktrees` などは見ない |
| 4 | `installed` | マニフェストに `cmake.find_package` がある (今は Tela) | M2。`find_package(<Name> CONFIG)`。SHA は照合できず版だけ記録 (PKG-09 record)。**STRICT では使わない** |
| 5 | `fetch` | `ERGO_PACKAGES_FETCH=ON` | M2。`repository@rev` を `GIT_SHALLOW` で取得 (`cmake/ErgoDependencies.cmake` の `_ergo_fc_declare`、`ERGO_DEPENDENCY_CACHE_DIR` 共有)。取得後 HEAD == pin を検証 (PKG-04) |
| 6 | — | どれにも当たらない | PKG-03 (構成エラー。試した経路とソースの指し方を表示) |

取り込んだパッケージは `add_subdirectory(<src> <build>/_ergo_packages/<name> EXCLUDE_FROM_ALL)` で組む。
パッケージごとの結線は `cmake/packages/<name>.cmake` (Pictor / Tela) に閉じる:

- `cmake.cache` を「ホスト未設定なら既定値」として設定 (**FORCE しない**。ホストが別値を入れていれば PKG-15 の警告でホストの値が勝つ)
- 選ばれた機能の `cache` を結線のスコープ内の通常変数として流す (Pictor の `web` → `PICTOR_BUILD_WEBGL=ON`)。
  Pictor の `vulkan` を外すと Pictor の配下だけで `find_package(Vulkan)` を無効にする
- `targets` / 選んだ機能の `provides` / `contract` を検証 (PKG-10)
- `exports` (`PICTOR_SHADER_DIR` など) を Ergo のディレクトリの通常変数として公開する。ホストからは
  `get_directory_property(<var> DIRECTORY <ergo の build dir> DEFINITION <VAR>)` で読める

## M0 (何もしない既存ホスト)

既存ホストは変更不要。Ergo を `add_subdirectory` するだけで、構成の最後に次が書かれる。

- `<build>/ergo-packages.resolved.json`: パッケージごとの pin・状態・既定で選ばれる機能、モジュールごとの ON/OFF と理由、検査結果
- `<build>/.cmake/api/v1/query/client-ergo/query.json`: CMake File API (codemodel v2) の問い合わせ

```
-- ergo packages: mode=M0 (pictor=host-unmanaged, tela=absent), 1 finding(s) -> <build>/ergo-packages.resolved.json
```

M0 では検査結果は記録だけで、構成を止めない (PKG-14 も `record`)。例外は
`ERGO_PACKAGE_MANIFEST_EXTRA` の同名衝突で、ホストが明示的に設定した変数なので M0 でも止める。

## M1 への移行 (照合だけ足す)

兄弟ディレクトリを `add_subdirectory` するホスト (AdventureCube 型) の例:

```cmake
add_subdirectory(../Pictor pictor)          # 今までどおりホストが作る
set(ERGO_PACKAGES "pictor" CACHE STRING "")  # 追加: ハブに照合させる
add_subdirectory(../Ergo ergo)
```

- 構成ログに `mode=M1 (pictor=host-unmanaged/host)` が出る
- Pictor の checkout の HEAD が pin と違えば PKG-05 (`ahead` / `behind` / `diverged`、pin の commit が無ければ `unknown`) の警告。
  未コミットの変更は PKG-07 (記録)。`expect_tag` (Pictor は `v2.2.0`) があり pin がその子孫でなければ PKG-08 の警告
- ホストの `PICTOR_BUILD_*` がマニフェストの既定値と違えば PKG-15 の警告 (M1 では値は変えない)
- 食い違いは「Ergo の `rev` を上げる PR」か「ホストの checkout を pin に合わせる」のどちらかで解消する

Pictor と Ergo を別々の SHA で FetchContent し自前で検証するホストも同じく、`ERGO_PACKAGES=pictor` を足して照合結果を見る。

ホストが Pictor を Ergo の **後** に作る場合は `ERGO_PACKAGE_PROVIDED_PICTOR=ON` を足す (ハブは取りに行かず、
ディレクトリ末尾でターゲットと SHA を検証する。ただし Ergo のモジュール節からは `pictor` が見えない)。

## M2 への移行 (取り込みをハブに任せる)

ホスト側の `add_subdirectory(../Pictor)` / Pictor の FetchContent / 自前の SHA 検証 / `PICTOR_BUILD_*` の FORCE を消し、
ソースの在り処だけを渡す。

```cmake
# ローカル (worktree・オフライン)
set(ERGO_PACKAGES "pictor" CACHE STRING "")
set(ERGO_PACKAGE_SOURCE_DIR_PICTOR "${CMAKE_CURRENT_SOURCE_DIR}/../Pictor" CACHE PATH "")
add_subdirectory(../Ergo ergo)
```

```sh
# 兄弟チェックアウトをまとめて指す (開発機。パスは CMakeUserPresets.json など公開しない所に書く)
cmake -S <host> -B build -DERGO_PACKAGES="pictor;tela" -DERGO_PACKAGES_SIBLING_ROOT=<兄弟の親>

# CI・出荷 (マニフェストの SHA で取得し、警告扱いをすべて止める)
cmake -S <host> -B build -DERGO_PACKAGES="pictor;tela" -DERGO_PACKAGES_FETCH=ON -DERGO_PACKAGES_STRICT=ON
```

- Ergo 単体の CI はプリセット `ci-packages` (`ERGO_PACKAGES=pictor;tela` + `FETCH=ON` + `STRICT=ON`)
- 取得は `ERGO_FETCH_DEPENDENCIES` (第三者ライブラリの取得許可、既定 ON) も ON である必要がある (同じエンジンを使うため)
- Pictor より前に走る必要のあるモバイルの Vulkan 結線はホストに残す
- 一部モジュールだけを使い将来 wasm 化するホスト (Ludellus-Native 型) は今は M0 のまま。描画を Pictor 経由にする段階で最初から M2 で入る

## STRICT

`ERGO_PACKAGES_STRICT=ON` で次が error になる: PKG-05 (drift)、PKG-06 (`ERGO_PACKAGE_REV_<NAME>`)、PKG-07 (dirty)、
PKG-08 (expect_tag)、PKG-09 (git なし)、PKG-15 (cache 衝突・ターゲット重複)、PKG-16 (tool submodule 未初期化)。
`installed` 経路は使わない。非 STRICT で通した findings はすべて resolved.json に残る。

error はすべて並べてから構成を止める。取り込みの段階で error があれば include 時点で、それ以外
(モジュールの `needs`、`provided` の後検証、tool) はディレクトリ末尾で止まる。

## 変数 (すべて `add_subdirectory(<ergo>)` より前のキャッシュ変数)

| 変数 | 意味 |
|---|---|
| `ERGO_PACKAGES` | 使う link パッケージの列 (例 `pictor;tela`)。空ならホストの宣言の `host.use`、**それも無ければ M0** |
| `ERGO_PACKAGES_OPTIONAL` | 選んだうち、どの経路でも見つからなければ使わないもの (PKG-03 の error の代わりに record)。空なら `host.optional` |
| `ERGO_PACKAGE_FEATURES_<NAME>` | 機能の足し引き (`+web;-vulkan`、`web` は `+web` と同じ)。不明な名前・予約機能は PKG-13。空なら `host.features.<name>` |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | ローカルソースを明示 (worktree・オフライン) |
| `ERGO_PACKAGES_SIBLING_ROOT` | 兄弟チェックアウト探索の明示 opt-in (`<root>/<Repo>` だけ。`.worktrees` は見ない) |
| `ERGO_PACKAGE_REV_<NAME>` | pin の一時上書き (検証用)。照合はこの値で行う。PKG-06 の警告、STRICT では error。値も 40 桁 SHA であること |
| `ERGO_PACKAGE_PROVIDED_<NAME>` | ホストが自分で作ると明示 (恒久の逃げ道。照合は行う) |
| `ERGO_PACKAGES_FETCH` | 取得を許すか。**既定 OFF** (ネットワークに出るのはこれが ON のときだけ) |
| `ERGO_PACKAGES_STRICT` | 警告扱いを error にする (CI と出荷構成で ON) |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | ホストの宣言 (または `host` 節の無い旧来の追加マニフェスト) の場所。非公開パッケージ用で形式は同じ schema 1、`cmake.wiring` はそのファイルからの相対。公開マニフェストと同名は PKG-15 の error。空ならホスト直下の `ergo-packages.json` が `host` 節を持つときそれを読む |
| `ERGO_PACKAGES_HOST_ID` | resolved.json の `host.id`。`host.code` が無いときだけ使う (空なら `CMAKE_PROJECT_NAME`) |

`<NAME>` はパッケージ名を大文字にし `-` を `_` にしたもの (`pictor` → `PICTOR`)。

## ホストの宣言と `ergo_pkg` (パッケージマネージャ W1 / W2)

設計は `spec/feature/package-manager.md` §2、形式は `spec/data/ergo-packages.md`「ホストの宣言」。

```bash
# Ergo をビルドすると ergo_pkg (ターゲット ergo_pkg_cli) ができる
cd <host>
ergo_pkg init --code <ProjectCode>   # ergo-packages.json (host 節)・cmake/ergo-bootstrap.cmake・.gitignore の .ergo/
ergo_pkg doctor                      # 宣言の検査・ブートストラップの古さ・漏れ検査。--strict で warning も止める
ergo_pkg list [--build <build>]      # 宣言・Ergo のマニフェスト・resolved.json を合わせた一覧
ergo_pkg fetch --build <build>       # host.use を FetchContent の置き場所へ事前取得 (ネットワーク)
ergo_pkg add <name> [--optional] [--features +vulkan]   # host.use に足す (共有パッケージ)
ergo_pkg add <name> --repository <url> --rev <sha|tag> [--kind link|tool]   # 非公開パッケージ (link は結線ファイルが先に要る)
ergo_pkg remove <name>               # host.use / optional / features / 非公開 packages から外す (needs を壊すなら拒否)
ergo_pkg update ergo --to <tag>      # host.ergo の rev と expect_tag を書く (ネットワーク)
ergo_pkg update <shared> --to <tag> --ergo <ergo>   # 共有パッケージの pin は Ergo のマニフェスト側
```

- ホストの `CMakeLists.txt` は `add_subdirectory(<ergo>)` の代わりに `include(cmake/ergo-bootstrap.cmake)` にする。
  Ergo は `ERGO_SOURCE_DIR` → 兄弟チェックアウト `<host>/../Ergo` → 取得 (`-DERGO_BOOTSTRAP_FETCH=ON` のときだけ) の順で見つかる。
- 宣言の `packages` は非公開パッケージ (旧 `ERGO_PACKAGE_MANIFEST_EXTRA` の中身)。ホスト直下の `ergo-packages.json` が
  `host` 節を持てば、ハブは `ERGO_PACKAGE_MANIFEST_EXTRA` 無しでそれを読む。
- 宣言の `use` / `optional` / `features` は構成の既定値になる (空でないキャッシュ値が常に勝つ。どちらを使ったかは
  resolved.json の `host.selected_from` / `optional_from` / `features_from`)。`legacy_vars` の `<package>.source_dir` の
  旧変数は、`ERGO_PACKAGE_SOURCE_DIR_<NAME>` が空のときだけ DEPRECATION つきでその代わりになる。
- Ergo の後でパッケージを自分で作るホスト (Ludellus-Native の L2) は `ERGO_PACKAGE_PROVIDED_<NAME>=ON` を設定する。
  ハブの最終判定はトップのディレクトリの末尾で行うので、ブートストラップの後に作ったターゲットも照合される。
  `optional` にも入れておけば、作らなかった構成では使わない (M0 のまま)。
- 宣言に `host.ergo.rev` があれば Ergo 自身のチェックアウトも照合し、resolved.json の `host.ergo` に pin / head / 経路 /
  drift / dirty を書く (PKG-05 / PKG-07 / PKG-09、`fetch` では PKG-04)。
- `ergo_pkg fetch` の後は `-DERGO_PACKAGES_FETCH=ON` の構成がネットワーク無しで経路 5 に当たる (取得済みの
  `<FETCHCONTENT_BASE_DIR>/ergo_pkg_<name>-src` が pin と同じ HEAD ならそれを使う)。
- `ergo_pkg` はコミット・ブランチ作成・PR をしない。宣言とブートストラップの変更はホストの feat ブランチで PR にする。
- Ergo は公開リポ。宣言・ブートストラップに開発機の絶対パスやトークンを書かない (`ergo_pkg doctor` の LEAK で止まる)。
- `add` / `remove` / `update` は該当の値だけを書き換え、キー順・改行・字下げは変えない。`update` は main の祖先でない
  commit (feature ブランチの先頭など) を拒む (`--allow-off-main` で通す)。`--to main` / `--to <sha>` はタグの無い pin なので
  STRICT で通らない旨を警告し、`expect_tag` を外す。版の解決はリポごとのツリーレス bare ミラー
  (`<cache>/mirrors/<Repo>.git`、`<cache>` は `--cache` → `ERGO_PKG_CACHE_DIR` → `ERGO_DEPENDENCY_CACHE_DIR` → 一時ディレクトリ) で行う。

## 既知の制約

- Pictor は desktop で glfw を暗黙に FetchContent する。ハブでは直せないため、オフラインで完結する
  構成は保証できない (Pictor 側に opt-out を依頼する。統合設計 §8 / N6)。
- `GIT_SHALLOW` の取得は pin が取得元のブランチ先端かタグであることを前提とする (CMake の shallow clone は任意の過去 commit を
  直接取れない)。過去の commit を固定するときは取得経路ではなくローカルソース経路を使う。shallow な checkout では
  祖先関係を判定しない (drift は HEAD == pin だけを見る、PKG-08 は判定しない)。
- Windows のパス長: 取得では `ERGO_DEPENDENCY_CACHE_DIR` を短いパスに置く。
- Emscripten の実 wasm ビルドは未確認 (multi-RHI 待ち)。現 pin の Pictor は Emscripten で `pictor` を作らず
  `pictor_webgl` だけを作る。マニフェストでは `pictor` を `vulkan` の `provides` にしているので、`vulkan` が外れる
  Emscripten では `pictor` を要求しない。

## 図にする (S3: P1 パッケージ版一覧 / P2 ターゲットのリンク図)

構成するとハブが `<build>/ergo-packages.resolved.json` と File API の問い合わせを置く。generate の後に:

```sh
cmake --build build --target ergo_package_graphs          # build/graphs/packages.tela と targets.tela
ergo_package_viz build --view packages --out packages.tela
ergo_package_viz build --view targets --focus ergo --out targets.tela   # 焦点: パッケージ / 層 / ターゲット
ergo_package_viz build --view targets --expand pictor --out targets.tela
ergo_package_viz build --check                            # PKG-17 宣言漏れがあれば終了コード 1
tela_view --font <file.ttf> --graph packages.tela         # Tela は別インストール (Excubitor 経由で起動)
```

- 図が Tela の上限 (8 グループ / 128 ノード / 256 辺) を超えるときは、推移簡約 → 畳み込み (host と Ergo 以外の
  パッケージを 1 ノード) → 分割 (`packages.1.tela` … と索引 `packages.index.tela`) の順に自動で落とす。
  上限を超えた 1 枚は書かない。
- 版の札は `revisor version show --repo <checkout>`、使えなければ `git describe --tags --long`、どちらも無ければ
  resolved.json の project(VERSION) を使い、出所を `[revisor]` / `[git]` / `[cmake]` で添える
  (`--no-version-probe` で照会しない)。
- `.tela` はビルドディレクトリに置き、コミットしない。

## 使用関係図 (S4: P3 usage)

マニフェストの `observe` が指す兄弟チェックアウトを読んで、MUSA 系・関連・共有 TS (Lapilli など)・Ergo のモジュール・
`tools/ergo` の使用関係を描く。読むのは `<root>/<path_hint>` の `package.json` / `.gitmodules` / `.git/index`
(`.git` がチェックアウト内のディレクトリのときだけ) と、Ergo 自身の `.gitmodules` / `tools/ergo/package.json` / マニフェストだけ。

```sh
ergo_package_viz build --view usage --root <兄弟チェックアウトの親> --out usage.tela
ergo_package_viz build --scan --root <兄弟チェックアウトの親>     # 辺を根拠 (ファイル:行 キー) 付きで表示
ergo_package_viz build --view usage --root <dir> --manifest <ergo>/ergo-packages.json --out usage.tela
```

- `ergo_package_graphs` は `ERGO_PACKAGES_SIBLING_ROOT` が設定されていればそれを `--root` に渡す。未設定なら観測対象は
  すべて「未観測」として描く (存在は示す)。
- 実線 = submodule・`file:` / `workspace:` / `@ludiars/*` 依存、破線 = 移植 (`ported_from`)・語彙共有 (`relations`)。
  Tela の辺は札を持たないので、根拠は使う側のノードの札に 3 件まで載せる (全件は `--scan`)。
- `--manifest` を省くと resolved.json に記録されたマニフェストを File API の Ergo の根で解く。

## 検証

| 何を | どうやって |
|---|---|
| 規則 (PKG 各項目の真偽・循環・機能の足し引き・drift 判定・resolved.json の決定性・追加マニフェストのマージ) | `ctest -R test_packages` (`tests/packages/test_*.cmake`、`cmake -P`。コンパイラ不要) |
| M0 でターゲット集合と compile definitions が変わらない (偽ホスト) | `ctest -R test_packages_m0_invariance` (`tests/packages/fixtures/m0_host`) |
| 6 経路・drift 5 種・STRICT・同名衝突・Emscripten 模擬 (偽パッケージ) | `ctest -R test_packages_hub_routes` (`tests/packages/fixtures/{alpha,beta,gamma,hub,hub_host}`。git が要る。テスト内で git リポジトリを作る) |
| M0 不変 (実ツリー全体、基準チェックアウトとの比較) | `cmake -DBASE_SOURCE=<基準> -DNEW_SOURCE=<新> -DWORK_DIR=<作業> [-DGENERATOR=...] -P tests/packages/compare_m0_build.cmake` |
| 観測の辺抽出・根拠・範囲外を読まない・未観測 (同梱の見本ディレクトリ) | `ctest -R test_package_scan` (`tests/package_scan/fixtures`)。gitlink はテスト内で一時ディレクトリに index を作って確かめる |
| 配置・P1 / P2 / P3・簡約・畳み込み・焦点・分割・PKG-17 (同梱の File API 見本) | `ctest -R "test_graph_layout|test_package_model|test_package_viz"` (`tests/package_model/fixtures`)。Tela がある構成では `test_package_viz_tela` が Tela 本体の `load_graph` での受理も確かめる |
