# LUDIARS パッケージ・ハブ (ホスト向け手順)

Ergo が Pictor / Tela などの LUDIARS 製 C++ パッケージの版を 1 か所で固定し、
取り込み・照合・記録を行う仕組み。設計の正本は
[`spec/feature/package-hub.md`](../feature/package-hub.md)、マニフェストの形式は
[`spec/data/ergo-packages.md`](../data/ergo-packages.md)。

- 対象: ホストにリンクされる LUDIARS 製 C++ パッケージ (link)、取得だけするもの (tool: Lapilli)、
  読むだけの関連リポ (observe: MUSA 系・Curare)
- 対象外: 第三者ライブラリ (kazmath / curl など) は従来どおり
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
| `ERGO_PACKAGES` | 使う link パッケージの列 (例 `pictor;tela`)。**未設定なら M0** |
| `ERGO_PACKAGE_FEATURES_<NAME>` | 機能の足し引き (`+web;-vulkan`、`web` は `+web` と同じ)。不明な名前・予約機能は PKG-13 |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | ローカルソースを明示 (worktree・オフライン) |
| `ERGO_PACKAGES_SIBLING_ROOT` | 兄弟チェックアウト探索の明示 opt-in (`<root>/<Repo>` だけ。`.worktrees` は見ない) |
| `ERGO_PACKAGE_REV_<NAME>` | pin の一時上書き (検証用)。照合はこの値で行う。PKG-06 の警告、STRICT では error。値も 40 桁 SHA であること |
| `ERGO_PACKAGE_PROVIDED_<NAME>` | ホストが自分で作ると明示 (恒久の逃げ道。照合は行う) |
| `ERGO_PACKAGES_FETCH` | 取得を許すか。**既定 OFF** (ネットワークに出るのはこれが ON のときだけ) |
| `ERGO_PACKAGES_STRICT` | 警告扱いを error にする (CI と出荷構成で ON) |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | ホスト側の追加マニフェスト (非公開パッケージ用。形式は同じ schema 1、`cmake.wiring` はそのファイルからの相対)。公開マニフェストと同名は PKG-15 の error |

`<NAME>` はパッケージ名を大文字にし `-` を `_` にしたもの (`pictor` → `PICTOR`)。

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

## 検証

| 何を | どうやって |
|---|---|
| 規則 (PKG 各項目の真偽・循環・機能の足し引き・drift 判定・resolved.json の決定性・追加マニフェストのマージ) | `ctest -R test_packages` (`tests/packages/test_*.cmake`、`cmake -P`。コンパイラ不要) |
| M0 でターゲット集合と compile definitions が変わらない (偽ホスト) | `ctest -R test_packages_m0_invariance` (`tests/packages/fixtures/m0_host`) |
| 6 経路・drift 5 種・STRICT・同名衝突・Emscripten 模擬 (偽パッケージ) | `ctest -R test_packages_hub_routes` (`tests/packages/fixtures/{alpha,beta,gamma,hub,hub_host}`。git が要る。テスト内で git リポジトリを作る) |
| M0 不変 (実ツリー全体、基準チェックアウトとの比較) | `cmake -DBASE_SOURCE=<基準> -DNEW_SOURCE=<新> -DWORK_DIR=<作業> [-DGENERATOR=...] -P tests/packages/compare_m0_build.cmake` |
