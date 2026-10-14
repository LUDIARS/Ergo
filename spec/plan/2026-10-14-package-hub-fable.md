# 設計: Ergo を LUDIARS C++ パッケージの集約点にし、版・リンク・使用関係を Tela で可視化する (Fable 案)

- 日付: 2026-10-14
- 分類: plan (設計書。実装はしない)
- 依頼: `spec/tasks/2026-10-14-package-hub-design-request.md` (4 章 1〜10 をすべて扱う)
- 対象: Ergo (Eg)。可視化の出口は `ergo_tela_graph` → Tela (Tl)
- 立場: 二本立ての「Fable」側。もう一方の案は見ていない

---

## 1. 結論

1. **Ergo は「LUDIARS の C++ CMake パッケージ」の集約点になる。** ホストは Ergo だけを
   `add_subdirectory` (または FetchContent) すれば、Pictor・Tela など必要な C++ パッケージが
   **Ergo の宣言した commit** で揃う。TS パッケージ・サービス (MUSA 系・Lapilli など) は
   **取得もリンクもしない**。版と使用関係を「観測して図にする」だけの対象にする。
2. **宣言は 1 ファイルの JSON マニフェスト `ergo-packages.json`** (リポ直下、`module_list.yaml` の隣)。
   CMake (`string(JSON)`)、C++ (可視化 CLI)、tools/ergo (TS) が同じファイルを読む。
   取得エンジンは既存の `cmake/ErgoDependencies.cmake` を再利用し、パッケージごとの結線は
   `cmake/packages/<name>.cmake` に 1 ファイル 1 パッケージで置く (`cmake/deps/` と同じ三層)。
3. **版の正本は commit SHA**。Revisor の `.revisor-version` は skip-worktree でマシンローカル、
   リリースタグは major / minor のときだけ打たれるので、どちらも「依存の固定子」には使えない。
   Revisor の版は **表示用の注釈** として、構成時に `git describe` で導出して添える (二重管理しない)。
   マニフェストがすでに SHA 固定なので別ロックファイルは作らず、構成結果を
   `<build>/ergo-packages.resolved.json` に書き出して「実際に何が解決されたか」の正本にする。
4. **ホストの選択・上書きは CMake キャッシュ変数だけ**で受ける (`ERGO_PKG_<NAME>=ON/OFF`、
   `ERGO_PKG_<NAME>_SOURCE_DIR`、`ERGO_PKG_FEATURE_<NAME>_<FEATURE>`)。ホスト側の別マニフェストは作らない
   (正本を 2 つにしない)。開発中の兄弟チェックアウト (`E:/Document/Ars/Pictor` など) は
   **明示した場合だけ**使い、pin との食い違いは drift として記録する (CI では `ERGO_PKG_STRICT=ON` で停止)。
5. **検査は構成時に止める**。版の食い違い (strict)、足りない依存、宣言グラフの循環、方式 (feature) の
   組合せ不正、二重供給 (ホストが `pictor` を作りつつ Ergo も取得) を FATAL にする。黙って通さない。
6. **リンク関係は CMake File API (codemodel v2) から機械的に集める**。手書きしない。パッケージ間・MUSA 系の
   使用関係は、マニフェストの宣言 + 各リポの `package.json` / `.gitmodules` を読んで集める。
7. **可視化は新モジュール `ergo_package_viz`** (observability ドメイン、`ergo_render_viz` と同じ切り方:
   スナップショット + 配置 + 読取アダプタ + CLI)。図は 3 種 (パッケージ版一覧 / リンク関係 / 使用関係) を
   それぞれ別の `.tela` にし、Tela の上限 (8 グループ / 128 ノード / 256 辺) を超えるときは
   `--collapse` / `--focus` で畳む。超過時は何も書かない (`ergo_tela_graph` と同じ)。
8. **既存ホストは 3 段で移る**: 「ホストが Pictor を供給していると明示」→「Ergo に取得を任せる
   (ローカル上書き可)」→「ホスト側の pin / 検証コードを削除」。互換期間中はホスト供給の `pictor` を
   WARNING + 「版不明」として記録し、期間後は明示フラグ無しでは FATAL にする。

---

## 2. 解釈 (親の解釈への同意・異論)

| 親の解釈 | 判断 | 根拠 |
|---|---|---|
| 「コンテンツ」= ゲームなどの利用側ホスト | 同意 | AdventureCube・Ludellus-Native は `external/ergo` + `../Pictor` を個別に `add_subdirectory`、別のホストは Pictor と Ergo を **別々の SHA** で FetchContent している。Ergo の SHA と Pictor の SHA の組合せが Ergo の開発時と一致する保証が今は無い |
| Ergo を 1 つの入口にし、必要な LUDIARS パッケージ (C++) が決まった版で揃う | 同意。ただし **C++ CMake パッケージに限る** | TS (Lapilli / Thaleia / Terpsichore)・Python (Clio)・Tauri (Ars-Musa) は CMake の世界に無い。Ergo がそれらのパッケージマネージャになるのは責務超過 (SRP)。観測対象に留める |
| 「管理」= マニフェストで版を持ち、ホストごとの選択を受ける | 同意 | Ergo には `ERGO_BUILD_<MOD>` と `third_party/dependencies.cmake` (SHA/tag pin + 無言フォールバック禁止) の前例があり、同じ形で延ばせる |
| 版の正本は Revisor | **異論**: Revisor の版は固定子にならない | `.revisor-version` は skip-worktree で origin には `uninitialized` が載る (Revisor `spec/feature/remote-publication.md`)。タグは major/minor でしか打たれず、Pictor は `v2.2.0` 以降の全コミットが同じ版を名乗る。依存の同一性は commit SHA でしか言えない。Revisor の版は注釈として並記する (Revisor 自身が `2.2.0+N` の build metadata で距離を表すのと同じ考え) |
| 「可視化」= 版・リンク関係・MUSA の使用関係を Tela で | 同意 | `ergo_tela_graph` が共通出口として既にあり、`render_viz` が最初の利用者。同じ出口・同じ切り方で並べる |
| 「MUSA」= 女神名プロジェクト群 + Ars-Musa | 同意。ただし **MUSA 系は今 C++ ライブラリを 1 つも持たない** | Clio は Python、Thaleia / Terpsichore は TS サービス、Ars-Musa は Tauri。Ergo の中にある MUSA 要素は `ergo_resource` (Clio の C++ 移植) だけ。したがって「MUSA が何を使っているか」はリンク関係ではなく **宣言 + 各リポのメタデータから集める使用関係**になる |
| Pictor → Ergo の依存を作らない | 同意 | Pictor `AGENTS.md` / `framework-boundaries.md` PC-RULE-001。Ergo が上に立ち、下層は Ergo を知らない。Tela も同様 |
| 「依存を暗黙に取りに行かない」(Tela / Pictor) と両立 | 同意。ただし Pictor 自身が desktop で glfw を暗黙 FetchContent している事実を記録する | Pictor `CMakeLists.txt`: `find_package(glfw3)` 失敗時に `FetchContent(glfw 3.4)`。Ergo 側では直せない (§6) |

追加で立てた解釈:

- **Tela の表示側 (tela_view / tela_overlay) は Ergo が組まない。** Tela の Windows アダプタは Pictor を
  `find_path` / `find_library` でファイルとして要求し、CMake ターゲット `pictor` を受け取れない。Ergo が
  Tela を取り込むのは `Tela::Core` / `Tela::Graph` (ヘッドレス、`ergo_tela_graph_tela` 用) までとし、
  表示は別にインストールした Tela を Excubitor 経由で起動する (今の `render_viz` と同じ運用)。
- **Pictor の描画方式の選択は「feature」としてマニフェストに前方宣言する。** 並行設計 (multi-RHI) の
  結論が出るまでは `vulkan` (既存の暗黙既定) と `web` (`PICTOR_BUILD_WEBGL` / Emscripten) の 2 つだけを
  Pictor の実オプションに対応させる。残り (`webgpu` / `dx12` / `metal`) はスキーマ上の予約。

---

## 3. 比較した案

### 3.1 マニフェストの形

| 案 | 内容 | 利点 | 欠点 |
|---|---|---|---|
| A. CMake スクリプト宣言 | `third_party/dependencies.cmake` と同じ関数呼び出し (`ergo_declare_package(...)`) | 既存エンジンと完全に同じ。パーサ不要 | C++ / TS の可視化側が読めず、図のために同じ情報をもう 1 度書く (二重管理)。ホスト側ツールからも読めない |
| **B. JSON マニフェスト** (推奨) | `ergo-packages.json` を CMake `string(JSON)`、C++ (`ergo_common` jsonm)、TS が読む | 正本 1 つ。構成・検査・可視化が同じデータ。schema 化できる | CMake 3.19+ が要る (Ergo は 3.16。ただし Pictor / Tela が 3.20 要求なので実質 3.20 への引き上げは不可避)。`string(JSON)` の書き味は冗長 |
| C. git submodule 一括 | Pictor / Tela を `lib/` に submodule で固定 (`lib/lapilli` の前例) | pin が gitlink に乗る。オフライン・worktree に強い。CMake 最小構成 | 全ホストに Pictor 全体の checkout を強制。ホスト別の使う / 使わないを表現しにくい。Pictor と Ergo を並行開発する兄弟チェックアウトの流儀と衝突。`--recursive` 忘れが頻発 |
| D. 外部パッケージマネージャ (vcpkg / Conan / CPM.cmake) | ポート / レシピで LUDIARS 各リポを配布 | 版解決・キャッシュ・オフラインが既製 | LUDIARS にレジストリ基盤が無い。学習・運用コスト。CPM は FetchContent の薄皮で B と大差無く、第三者依存が増えるだけ |

**選択: B。** エンジンは A の既存実装を流用し、宣言だけを JSON に外出しする。TS ツール依存 (Lapilli) は
今の submodule を維持し、マニフェストには「取得方式 = submodule、記録のみ」として載せる (C の良さを部分採用)。

### 3.2 取り込みの仕組み

| 案 | 内容 | 利点 | 欠点 |
|---|---|---|---|
| A. 今のまま (ホストが各パッケージを個別に取り込む) | 変更なし | 変更コスト 0 | 版の組合せが保証されない。ホストごとに pin / 検証コードが重複 (別ホストは自前の exact-source 検証関数を持つ) |
| **B. Ergo が FetchContent で取得、ホストは上書きだけ** (推奨) | `ergo_packages_resolve()` が pin を解決して `add_subdirectory` | 入口 1 つ。版は Ergo が保証。オフラインは `SOURCE_DIR` 上書き。既存エンジンと同じ流儀 | Pictor の configure 時間が Ergo の configure に乗る (今もホストが払っているコストで総量は不変)。巨大リポの FetchContent は `GIT_SHALLOW` と共有キャッシュで緩和 |
| C. `find_package(<Pkg> CONFIG)` 必須 (インストール済み前提) | ホストが各パッケージを先にインストール | ビルド時間最短。Tela の `find_package(Tela CONFIG)` 前例 | Pictor に install/export が無い。SHA の同一性を保証できない。worktree 開発に不向き |
| D. 兄弟ディレクトリ自動探索 | `../Pictor` があれば使う | 開発者に便利 | Tela / Pictor の「兄弟を暗黙に取りに行かない」方針に反する。どの版を使ったか分からなくなる |

**選択: B。** D は **明示 opt-in** (`ERGO_PKG_SIBLING_ROOT=<dir>`) に限って認め、見つかった checkout の
HEAD と pin の差を drift として必ず記録する。C は将来 Pictor が install/export を持ったときの追加経路として
スキーマに `acquire: find_package` を予約しておく。

### 3.3 リンク関係の収集

| 案 | 内容 | 利点 | 欠点 |
|---|---|---|---|
| **A. CMake File API (codemodel v2)** (推奨) | `<build>/.cmake/api/v1/query/` に問い合わせを置き、generate 後の reply JSON を読む | 全ジェネレータで同じ。ターゲット・ソースディレクトリ・直接依存・リンクフラグが構造化されている。ホスト側ターゲットも含めて取れる | 生成後にしか読めない (= 別プロセスの CLI で読む)。reply は大きい |
| B. `cmake --graphviz` | dot ファイル | 手軽 | 1 枚の dot をテキスト解析する羽目になる。パッケージへの帰属 (ソースディレクトリ) が入らない |
| C. マニフェストの宣言のみ | `relations[]` を人が書く | 実装最小 | すぐ陳腐化する。「手書きの図にしない」に反する |
| D. ソース解析 (Anatomia) | `#include` / import を追う | ビルドに依らない | C++ の include 解析では「リンクしているか」は分からない。Anatomia は単リポが前提 |

**選択: A を CMake ターゲット層の正本にし、C はパッケージ層 (MUSA 系・TS) の補助、D は使わない。**
TS / サービスの使用関係は `package.json` の `dependencies` と `.gitmodules` の pin を読む専用の小さな
リーダで集める (宣言ではなく各リポのメタデータが正本)。

### 3.4 可視化の置き場

| 案 | 内容 | 判断 |
|---|---|---|
| **A. Ergo の C++ モジュール + CLI → `.tela`** (推奨) | `ergo_render_viz` と同じ形 | Tela が出口である要件に直結。構成時の `resolved.json` と File API reply を読むだけなので Pictor 不要・常時ビルド可 |
| B. tools/ergo のプラグイン (TS、vis-network) | Web UI で見る | 既存 `render_pipeline` プラグインと同じ見せ方で便利だが、依頼の出口は Tela。**段階計画の最後の任意項目**として残す (同じ `resolved.json` を読むだけなので後から足せる) |
| C. CMake の configure 内で `.tela` を直接書く | 外部プロセス不要 | 配置計算を CMake 言語で書くのは無理がある。File API は configure 中に読めない |

---

## 4. 推奨案の詳細

### 4.1 対象範囲 (依頼 4-1)

パッケージを **3 つの層 (tier)** に分ける。層はマニフェストの `kind` と `acquire` で表す。

| 層 | 何をするか | 対象 (初版) | 対象外 |
|---|---|---|---|
| **T1 管理** (取得 + 版固定 + リンク) | Ergo が pin した commit を取得し `add_subdirectory` して CMake ターゲットを供給する | **Pictor** (`pictor`、将来 `pictor_webgl` 等の方式別ターゲット)、**Tela** (`Tela::Core` / `Tela::Graph`、ヘッドレス)。将来: Accommodatio (C++、LLM 不使用)、Spatium (C++ 化されたとき)、Fundamentum (C++ 部分があれば) | Pictor の demo / tools / tests (既定 OFF で配布しない)、Tela の Windows 表示側 (Pictor をファイルで要求するため) |
| **T2 ツール依存** (取得するがリンクしない) | tools/ergo (Node) が使う TS パッケージ。取得方式は既存の git submodule を維持 | **Lapilli** (`lib/lapilli`、`@ludiars/one-shot`) | Lapilli の他パッケージの publish 運用 (GitHub Packages) は Lapilli 側の責務 |
| **T3 観測** (取得もリンクもしない) | 版と使用関係を読み取って図にするだけ | **MUSA 系**: Clio (`ergo_resource` の移植元)、Thaleia、Terpsichore、Ars-Musa。関連: Curare (`ergo_resource` のタグ語彙の共有先) | LUDIARS 全リポ。全体の俯瞰は Concordia (`/v1/project-codes`) / Anatomia の責務で、Ergo のマニフェストには **Ergo と具体的な関係を持つものだけ**を載せる |

シェーダ・資産は独立したパッケージにしない。パッケージが持つディレクトリ (Pictor の `shaders/` / `profiles/`)
をマニフェストの `exports` で名前付きパス (`PICTOR_SHADER_DIR` 等の CMake 変数) として公開し、ホストや
tools/ergo (`render_pipeline` プラグインは `Pictor/profiles/*.profile.json` を読む) が絶対パスを
ハードコードしないようにする。

MUSA 系の位置づけ: MUSA は「ゲームを支える開発支援道具」(Ars-Musa `docs/DESIGN.md`) で、Ergo (ゲームを作る
道具) の上に乗らない。Ergo の中の MUSA 要素は `ergo_resource` ↔ Clio の **移植 (port-of) 関係**だけ。
これは図では破線で表し、リンクではないことを形で区別する。

### 4.2 マニフェスト (依頼 4-2)

#### 置き場所と形式

- 正本: リポ直下 `ergo-packages.json` (機械処理用。`module_list.yaml` の隣)
- スキーマ説明: `spec/data/ergo-packages.md` (FORMAT_SPEC の `data/` = データスキーマ)
- 使い方: `spec/setup/packages.md` (ホスト向け手順。`third-party-deps.md` と並ぶ)
- 構成結果 (生成物、追跡しない): `<build>/ergo-packages.resolved.json`

```json
{
  "schema_version": 1,
  "packages": [
    {
      "name": "pictor",
      "code": "Pc",
      "kind": "cmake-library",
      "acquire": "fetch",
      "repo": "https://github.com/LUDIARS/Pictor.git",
      "pin": { "commit": "<40 桁 SHA>", "expect_tag": "v2.2.0" },
      "default": "off",
      "platforms": ["desktop", "android", "ios", "emscripten"],
      "cmake": {
        "minimum": "3.20",
        "cxx_standard": 20,
        "wiring": "cmake/packages/pictor.cmake",
        "defaults": { "PICTOR_BUILD_DEMO": "OFF", "PICTOR_BUILD_TESTS": "OFF", "PICTOR_BUILD_TOOLS": "OFF" }
      },
      "features": {
        "vulkan": { "default": "auto", "platforms": ["desktop", "android", "ios"],
                    "provides": ["pictor"], "contract": "PICTOR_HAS_VULKAN" },
        "web":    { "default": "auto", "platforms": ["emscripten"], "option": "PICTOR_BUILD_WEBGL",
                    "provides": ["pictor_webgl"], "contract": "PICTOR_HAS_WEBGL" },
        "webgpu": { "reserved": true }, "dx12": { "reserved": true }, "metal": { "reserved": true }
      },
      "exports": { "PICTOR_SHADER_DIR": "shaders", "PICTOR_PROFILE_DIR": "profiles" },
      "required_by": {
        "ergo_render":            "real-path",
        "ergo_render_viz_pictor": "build",
        "ergo_particle":          "feature:ERGO_PARTICLE_HAS_RENDERER"
      }
    },
    {
      "name": "tela", "code": "Tl", "kind": "cmake-library", "acquire": "fetch",
      "repo": "https://github.com/LUDIARS/Tela.git",
      "pin": { "commit": "<SHA>" },
      "default": "off", "platforms": ["desktop"],
      "cmake": { "minimum": "3.20", "cxx_standard": 20, "wiring": "cmake/packages/tela.cmake",
                 "defaults": { "TELA_BUILD_WINDOWS": "OFF", "TELA_BUILD_PICTOR": "OFF", "BUILD_TESTING": "OFF" },
                 "provides": ["Tela::Core", "Tela::Graph"] },
      "required_by": { "ergo_tela_graph_tela": "build" },
      "tools": [{ "name": "tela_view", "launch": "excubitor", "note": "表示側は Ergo が組まない" }]
    },
    {
      "name": "lapilli", "code": "La", "kind": "ts-workspace", "acquire": "submodule",
      "path": "lib/lapilli",
      "relations": [{ "from": "tools/ergo", "to": "lapilli/one-shot", "kind": "tool-dependency" }]
    },
    {
      "name": "clio", "code": "Cl", "kind": "service", "acquire": "none", "musa": true,
      "relations": [{ "from": "ergo_resource", "to": "clio", "kind": "port-of" }]
    },
    { "name": "thaleia",     "code": "Th", "kind": "service", "acquire": "none", "musa": true,
      "observe": { "package_json": true } },
    { "name": "terpsichore", "code": "Tp", "kind": "ts-package", "acquire": "none", "musa": true,
      "observe": { "package_json": true } },
    { "name": "ars-musa",    "code": "Au", "kind": "desktop-app", "acquire": "none", "musa": true }
  ]
}
```

フィールドの規則:

- `pin.commit` は **40 桁 SHA 必須** (ブランチ名・短縮 SHA は構成エラー)。`third_party/dependencies.cmake`
  の「`master`/`main` 禁止」と同じ。
- `pin.expect_tag` は任意の **人向け注釈**。構成時に checkout にそのタグがあれば `git merge-base --is-ancestor`
  で pin がタグ以降にあることを確かめ、違えば WARNING (strict なら FATAL)。Revisor の版を Ergo が「持つ」のでは
  なく「照合する」だけ。
- `default`: `off` / `on` / `auto`。`auto` は `required_by` に挙がったモジュールが ON のとき ON。
  初版は Pictor / Tela とも `off` (素の Ergo ビルドを今まで通りネットワーク非依存・軽量に保つ。
  §7 の人間判断)。
- `required_by` の値: `build` (そのターゲット自体が組めない)、`real-path` (組めるが実経路が無効化される。
  今の `ergo_render` の挙動)、`feature:<OPTION>` (そのオプションが ON のときだけ必要)。
- `features.<f>`: `option` (Pictor 側 CMake オプション名)、`provides` (有効時に存在すべきターゲット)、
  `contract` (ターゲットが公開する compile definition。`ErgoRenderBackend.cmake` が `PICTOR_HAS_VULKAN`
  を正本にしている流儀を一般化)、`platforms`、`requires` / `conflicts` (他 feature との関係)。
  `reserved: true` は「名前だけ予約、選ぶと構成エラー」。
- T3 の `observe.package_json` は、可視化 CLI が `ERGO_PKG_SIBLING_ROOT/<Repo>/package.json` を読んで
  `dependencies` を使用関係として採る指示。読めなければ「未観測」として図に残す (黙って消さない)。

#### ホストごとの選択と上書き (CMake キャッシュ変数のみ)

| 変数 | 意味 |
|---|---|
| `ERGO_PKG_<NAME>` = ON / OFF | そのパッケージを Ergo が供給するか (既定はマニフェストの `default`) |
| `ERGO_PKG_<NAME>_SOURCE_DIR` = path | pin を使わずローカルソースを使う (オフライン / 並行開発)。HEAD と pin の差を drift として記録 |
| `ERGO_PKG_<NAME>_COMMIT` = SHA | pin を一時的に別 SHA へ (検証目的)。drift 記録。strict で FATAL |
| `ERGO_PKG_<NAME>_PROVIDED` = ON | **ホストが自分でターゲットを作る**宣言 (移行期の互換)。Ergo は取得せず「版不明」で記録 |
| `ERGO_PKG_FEATURE_<NAME>_<FEATURE>` = ON / OFF / AUTO | 方式・任意機能の選択 |
| `ERGO_PKG_SIBLING_ROOT` = dir | 兄弟チェックアウト探索の **明示 opt-in** (`E:/Document/Ars` など)。設定時のみ `<dir>/<Repo>` を候補にする |
| `ERGO_PKG_STRICT` = ON | drift / expect_tag 不一致 / 未検証供給を WARNING から FATAL に格上げ (CI 用) |
| 既存 `ERGO_FETCH_DEPENDENCIES` / `ERGO_DEPENDENCY_CACHE_DIR` | そのまま LUDIARS パッケージにも効く |

ホスト別の設定ファイル (例: `<host>/ergo-packages.host.json`) は **作らない**。CMake キャッシュ変数と
`CMakePresets.json` で表現できる範囲に留め、正本を増やさない。

#### ロック (再現性)

- マニフェスト自体が SHA 固定なので、別ロックファイルは不要。
- 再現性の証拠は `<build>/ergo-packages.resolved.json`:

```json
{
  "schema_version": 1,
  "ergo":  { "commit": "<SHA>", "dirty": false },
  "build": { "platform": "desktop", "emscripten": false, "generator": "Ninja", "strict": false },
  "packages": [
    { "name": "pictor", "enabled": true, "provenance": "fetch",
      "source_dir": ".../_deps/pictor-src", "commit_pinned": "<SHA>", "commit_resolved": "<SHA>",
      "drift": "none", "describe": "v2.2.0-37-g<SHA7>",
      "features": { "vulkan": "on", "web": "off" }, "provides_verified": ["pictor"],
      "contracts": ["PICTOR_HAS_VULKAN"] },
    { "name": "tela", "enabled": false, "reason": "ERGO_PKG_TELA=OFF" },
    { "name": "lapilli", "provenance": "submodule", "commit_resolved": "<gitlink SHA>", "initialized": true },
    { "name": "clio", "provenance": "observed", "commit_resolved": null, "note": "sibling root not set" }
  ],
  "diagnostics": [ { "code": "EPK-010", "level": "warning", "text": "..." } ]
}
```

- `drift` の値: `none` / `ahead` (解決 SHA が pin の子孫) / `behind` / `diverged` / `unknown` (git が無い、
  export されたツリー)。CI は `strict` で `none` 以外を拒否する。
- Revisor との関係: Ergo は `.revisor-version` を読まず書かない。表示用の `describe` は
  `git describe --tags --match 'v*'` で導出。開発機では可視化 CLI が任意で `revisor version show --repo`
  を叩いて 4 情報源 (版ファイル / タグ / health / package.json) を注釈として添えられるようにする
  (段階 S7、任意)。

### 4.3 取り込みの仕組み (依頼 4-3)

#### 構成の流れ (Ergo トップレベル `CMakeLists.txt`)

```
cmake_minimum_required(VERSION 3.20)        # 3.16 → 3.20 (string(JSON) と Pictor/Tela の要求に合わせる)
...
include(ErgoPackages)                        # cmake/ErgoPackages.cmake (エンジン)
ergo_packages_load("${CMAKE_CURRENT_LIST_DIR}/ergo-packages.json")
ergo_packages_resolve()                      # 1) 有効判定 2) 出所決定 3) 取得 4) add_subdirectory 5) 検証
include(third_party/dependencies.cmake)      # 既存 (kazmath / curl)
... 各 ergo_<mod> (今まで通り if(TARGET pictor) で分岐) ...
ergo_packages_finalize()                     # 検査の総括 + resolved.json 書き出し + FATAL/WARNING 集約
```

`ergo_packages_resolve()` の 1 パッケージあたりの出所決定順 (上から最初に当たったもの。**どれにも当たらなければ FATAL**):

1. `ERGO_PKG_<NAME>_PROVIDED=ON` → 取得しない。ターゲットの存在を検証して「ホスト供給・版不明」で記録
2. `ERGO_PKG_<NAME>_SOURCE_DIR` → そのディレクトリ。HEAD を `git rev-parse` して drift 判定
3. `ERGO_PKG_SIBLING_ROOT` が設定されていて `<root>/<Repo>/CMakeLists.txt` がある → 同上 (出所 `sibling`)
4. `ERGO_FETCH_DEPENDENCIES=ON` → 既存 `_ergo_fc_declare` 相当で `repo@commit` を `GIT_SHALLOW` 取得
   (`ERGO_DEPENDENCY_CACHE_DIR` で複数 build tree が共有)。取得後 HEAD == pin を検証 (別ホストが自前で持つ
   exact-source 検証と同じ役目を Ergo が引き受ける)
5. どれも無い → FATAL (「`ERGO_PKG_<NAME>_SOURCE_DIR` を指すか fetch を許可せよ」の指示付き)

出所決定後、`cmake/packages/<name>.cmake` が **そのパッケージ固有の結線**を行う (1 ファイル 1 パッケージ):

- マニフェストの `cmake.defaults` を「ホストが未設定なら既定値」として `set(... CACHE ...)` する
  (`FORCE` しない。ホストが先に別値を set していればそちらが勝つ)
- feature → オプションの写像 (`web` → `PICTOR_BUILD_WEBGL=ON`)
- `add_subdirectory(<src> <bin> EXCLUDE_FROM_ALL)` (Ergo が使うターゲットだけ組まれる)
- `ergo_verify_targets` で `provides` の存在、`contract` の公開を検証
- `exports` を CMake 変数として親スコープへ公開 (`PICTOR_SHADER_DIR`)

Pictor → Tela → Ergo モジュールの順。Tela を Pictor 不要の構成 (ヘッドレス) にするのは §2 の通り。

#### 「依存を暗黙に取りに行かない」との両立

- Ergo は **自分のマニフェストに書いたものだけ**を、**書いた commit で**取得する。Tela / Pictor の
  「兄弟ソースを暗黙に取りに行かない」は「自分の依存を自分で勝手に探さない」規則で、上位の集約点が
  明示宣言で取得するのは矛盾しない。
- Ergo から下層へは **ターゲットとパス**だけを渡す。下層同士に互いを探させない (Tela には
  Pictor を渡せないので Tela のヘッドレス部分だけを組む)。
- 兄弟探索は `ERGO_PKG_SIBLING_ROOT` 明示時のみ。既定では探さない。

#### オフライン / CI / worktree

| 状況 | 動き |
|---|---|
| オフライン開発機 | `ERGO_PKG_SIBLING_ROOT=E:/Document/Ars` または `_SOURCE_DIR`。drift は WARNING で開発を止めない |
| CI | `ERGO_PKG_STRICT=ON`。`ERGO_DEPENDENCY_CACHE_DIR` をジョブ間キャッシュ。取得失敗は FATAL (無言で Pictor 無しに落とさない。今の `ergo_render` の desktop WARNING は strict で FATAL に揃える) |
| worktree (`.worktrees/<name>`) | FetchContent は build dir 単位なので影響なし。`SIBLING_ROOT` は `.worktrees` を見ない (別 worktree の HEAD を拾うと版の由来が分からなくなる)。並行開発では `_SOURCE_DIR` で特定 worktree を明示する |
| ホストが `EXCLUDE_FROM_ALL` で Ergo を取り込む | Ergo 内の `add_subdirectory` も `EXCLUDE_FROM_ALL` なので、使うターゲットだけ組まれる |

#### Emscripten (Ludellus-Native の WebAssembly 化)

- 同じ `ergo_packages_resolve()` を使う。違いは `platforms` と feature の既定だけ。
  `EMSCRIPTEN` では Pictor の `vulkan` feature は選べず (`platforms` 不一致 → FATAL)、`web` が `auto` で ON。
  Pictor は Emscripten では `pictor_webgl` しか作らない (`cmake/PictorWebGL.cmake`、本体 `pictor` は Vulkan 直読み) ので、
  `provides` は feature ごとに別名にしてある。
- Ergo モジュール側の Emscripten 対応はこの設計の範囲外 (各モジュールの `platforms` を `module_list.yaml`
  に足すのは別 PR)。`ergo_render` の実経路は Pictor 側の描画方式の切り替え口 (multi-RHI) ができてから。
  今できるのは「構成が正しく拒否される / 正しく web ターゲットが選ばれる」ことまで。**実機 (wasm) 未確認**。
- Pictor の `web-mesh/` ES module + wasm の配布 (`<build>/web-mesh/`) は `exports` で名前付きパスとして公開する。

### 4.4 検査 (依頼 4-4)

すべて構成時 (`ergo_packages_resolve` / `_finalize`)。コードを付け、`resolved.json` の `diagnostics` にも残す。

| コード | 条件 | 既定 | strict |
|---|---|---|---|
| EPK-001 | `pin.commit` が 40 桁 SHA でない / ブランチ名 | FATAL | FATAL |
| EPK-002 | 解決した HEAD ≠ pin (fetch 出所) | FATAL (取得自体の失敗) | FATAL |
| EPK-003 | 解決した HEAD ≠ pin (`_SOURCE_DIR` / sibling 出所) = drift | WARNING + 記録 | FATAL |
| EPK-004 | `expect_tag` が checkout にあり、pin がその子孫でない | WARNING | FATAL |
| EPK-005 | ホスト供給 (`_PROVIDED`) だが `provides` ターゲットが無い | FATAL | FATAL |
| EPK-006 | **二重供給**: `_PROVIDED` 無しで `TARGET pictor` が既に存在する (ホストが先に `add_subdirectory` した) | 互換期間中 WARNING (「版不明」で記録)、期間後 FATAL | FATAL |
| EPK-007 | 足りない依存: `required_by` に挙がるモジュールが ON だがパッケージが OFF | `build` → FATAL、`real-path` → 今の `ergo_render_enforce_backend` の規則 (モバイルは FATAL、desktop は WARNING、`ERGO_RENDER_REQUIRE_REAL` で FATAL) | FATAL |
| EPK-008 | 取得後に `provides` / `contract` が無い (upstream のレイアウト変更) | FATAL | FATAL |
| EPK-009 | 宣言グラフ (`relations` / `required_by` / feature `requires`) に循環 | FATAL | FATAL |
| EPK-010 | feature 組合せ不正: `platforms` 不一致 / `conflicts` 同時 ON / `reserved` を選択 / 描画が必要なのに描画 feature が 1 つも ON でない | FATAL | FATAL |
| EPK-011 | パッケージの `cmake.minimum` が実行中 CMake より新しい | FATAL | FATAL |
| EPK-012 | T2 submodule が未初期化 (`lib/lapilli` が空) で tools/ergo を要求した | WARNING (C++ ビルドは止めない) | FATAL |
| EPK-013 | git が無く SHA を取れない (export ツリー) | WARNING + `drift: unknown` | FATAL |

原則: **黙って通さない・部分的に通さない**。`finalize` は FATAL を 1 つでも含めば全件を列挙してから止める
(1 つ直すごとに次が出る往復を避ける)。

循環検査について: CMake ターゲットのリンク循環は CMake 自身が検出する。ここで見るのは **マニフェストの
宣言レベル** (A が B を `requires`、B が A を `requires`)。パッケージの実リンク図の循環は File API から
可視化側 (§4.6) で色を付けて示す (構成は止めない。Pictor 内部の循環は Pictor の問題)。

### 4.5 リンク関係の収集 (依頼 4-5)

収集は **構成・生成後に別プロセス (CLI) が読む**。CMake の configure 中には File API の reply が無い。

| 層 | 情報源 | 読み方 |
|---|---|---|
| CMake ターゲット (Ergo モジュール / Pictor / Tela / ホスト) | **CMake File API codemodel v2** | `<build>/.cmake/api/v1/query/client-ergo/query.json` に `{"requests":[{"kind":"codemodel","version":2}]}` を置く (`ergo_packages_resolve` が置く)。generate 後 `reply/` の `codemodel-v2-*.json` → 各 `target-*.json` を読み、`name` / `type` / `sourceDir` / `dependencies[]` (直接依存) / `link.commandFragments` を採る |
| ターゲット → パッケージの帰属 | `resolved.json` の `source_dir` | ターゲットの `sourceDir` が最長一致するパッケージへ帰属。どれにも当たらなければ「host」 |
| パッケージ間 (T1) | 上の帰属で畳む | ターゲット辺を帰属パッケージ単位に集約 (重複は本数として保持) |
| T2 / T3 の使用関係 | `package.json` (`dependencies` / `devDependencies`)、`.gitmodules` + `git ls-tree` (gitlink SHA)、マニフェスト `relations` | 各リーダは 1 ファイル 1 責務。`SIBLING_ROOT` 未設定で読めないものは「未観測」ノードとして残す |
| 既知の外部 (kazmath / curl / glfw / Vulkan SDK / FMOD) | File API 上の import ターゲット / FetchContent 名 | グループ「third-party」に畳む |

除外既定: `type == EXECUTABLE` のうちテスト (`add_test` 対象) とベンチ、`UTILITY`、`INTERFACE` の空ターゲット。
`--include-tests` で戻せる。

Ergo だけで ~35 ライブラリ + Pictor ~10 + Tela ~8 + ホスト数本 = 128 を超えうる。超過時の畳み方は §4.6。

### 4.6 可視化 (依頼 4-6)

#### 図の種類 (1 図 = 1 `.tela`)

| 図 | ノード | グループ (≤ 8) | 辺 | 想定規模 |
|---|---|---|---|---|
| **A. パッケージ版一覧** `inventory` | パッケージ 1 つ = 1 ノード。ラベル: `名 (コード)` / `SHA7` / `describe` / `drift` / 出所 | `host` / `ergo` / `cmake-library` / `ts` / `service` / `third-party` | パッケージ間依存 (T1 は File API 集約、T2/T3 は観測・宣言)。drift のあるノードは `★drift` を付ける | ≤ 30 ノード |
| **B. リンク関係** `links` | ライブラリターゲット 1 つ = 1 ノード | `host` / `ergo:<domain>` (Anatomia の `spec/domains` の名で最大 4 つに畳む) / `pictor` / `tela` / `third-party` | `target_link_libraries` の直接辺 (実線)。`required_by: real-path` の有効/無効は破線 | 60〜150。**超過時は書かない**。`--focus <target>` (近傍 2 段) か `--collapse <package>` (そのパッケージを 1 ノードに畳む) を提案して終了 |
| **C. 使用関係 (MUSA / 関連)** `usage` | MUSA 系・Lapilli・Curare・Ergo モジュール (`ergo_resource` / tools/ergo) | `musa` / `lapilli` / `ergo` / `tools` / `host` / `unobserved` | `uses` (実線、package.json / submodule)、`port-of` / `tool-dependency` / `shares-vocabulary` (破線、宣言) | ≤ 30 |

方式 (feature) の選択状態は図 A のラベル行に `vulkan:on web:off` として載せる (別図にしない)。

#### 配置の規則 (決定的。同じ入力 → 同じ図)

- 論理サイズ 1600 × 900、ノード 200 × 48、列間 80、行間 20、余白 40。
- **左 → 右を依存の向き**にする: 層 = 「依存されない側からの最長路」(ホストが最左、third-party が最右)。
  同じ層内は グループ順 → 名前順で上から。
- 辺は縦横の折れ線 (render_viz と同じ)。層をまたぐ辺は列間の通路を通し、長さ 0 の区間を作らない。
  同一 (from,to) の重複はまとめて 1 本にし、本数をラベルに含めない (Tela の辺にラベルは無い)。
- 循環 (File API 上で検出) は赤系グループ `cycle` に当該ノードを移して見せる (グループ上限を 1 つ使う)。
- ID は `pkg:<name>` / `tgt:<name>` / `edge:<from>-><to>`。同名ターゲットが別パッケージにあれば `#2` を付ける
  (render_viz と同じ)。
- 配置も検査も `ergo_tela_graph::validate_tela_graph` を最後に通し、通らなければ **何も書かない**。

#### 手順

```sh
# 1) 構成 (query を置く + resolved.json を書く)
cmake -S <host> -B build -DERGO_PKG_PICTOR=ON -DERGO_PKG_TELA=ON
# 2) 図を書く (File API の reply は generate 後に存在する)
ergo_package_viz --resolved build/ergo-packages.resolved.json --file-api build/.cmake/api/v1/reply \
                 --figure inventory --out packages-inventory.tela
ergo_package_viz ... --figure links --collapse pictor --collapse tela --out packages-links.tela
ergo_package_viz ... --figure usage --sibling-root E:/Document/Ars --out packages-usage.tela
# 3) 見る (Tela は別インストール。Excubitor 経由で起動)
tela_view --font <file.ttf> --graph packages-inventory.tela
```

便宜のため CMake のカスタムターゲット `ergo_package_graphs` を用意し、`cmake --build build --target
ergo_package_graphs` で 3 図を `<build>/graphs/` に書く (CLI がビルドされている構成のみ)。

`render_viz` との並び: どちらも「スナップショット → 配置 → `ergo_tela_graph`」で、CLI 名も
`ergo_render_viz` / `ergo_package_viz` と対にする。

### 4.7 Ergo の中での配置 (依頼 4-7)

#### ビルド層 (C++ モジュールではない)

| 置き場 | 役割 | 既存との対応 |
|---|---|---|
| `ergo-packages.json` | マニフェスト正本 | `module_list.yaml` と並ぶ機械処理用一覧 |
| `cmake/ErgoPackages.cmake` | エンジン: JSON 読み込み、出所決定、取得 (既存 `_ergo_fc_declare` を再利用)、検証、resolved.json、diagnostics 集約。**具体パッケージ名を持たない** | `cmake/ErgoDependencies.cmake` |
| `cmake/packages/<name>.cmake` | 1 パッケージ 1 ファイルの結線 (defaults / feature 写像 / add_subdirectory / verify / exports) | `cmake/deps/<name>.cmake` |
| `cmake/ErgoRenderBackend.cmake` | **変更しない**。`pictor` ターゲットが誰に作られたかに関わらず `PICTOR_HAS_VULKAN` を正本に判定する。将来 `PICTOR_HAS_<RHI>` を読むのは multi-RHI 側の設計に従う | — |
| `tests/cmake/test_packages_*.cmake` | `cmake -P` で動く純関数テスト (SHA 形式、出所決定順、drift 判定、feature 組合せ、循環) | 別ホストが持つ「ターゲットに触らない純関数 + script-mode テスト」の render 契約検査が前例 |

#### C++ モジュール `ergo_package_viz` (observability ドメイン)

| ターゲット | 役割 | 依存 | ビルド条件 |
|---|---|---|---|
| `ergo_package_viz` | スナップショット値型 (`PackageSnapshot`: packages / targets / edges / diagnostics) と 3 つの配置 (`layout_inventory` / `layout_links` / `layout_usage`)、畳み (`collapse` / `focus`) | `ergo_tela_graph`、`ergo_common` | 常時 |
| `ergo_package_viz_readers` | `resolved.json` リーダ、File API codemodel リーダ、`package.json` リーダ、`.gitmodules` リーダ (各 1 ファイル) | `ergo_package_viz`、`ergo_common` (jsonm)、`ergo_io` | 常時 |
| `ergo_package_viz` (CLI、ターゲット名 `ergo_package_viz_cli`) | 引数 → リーダ → 配置 → `save_tela_graph` | 上 2 つ | `ERGO_BUILD_PACKAGE_VIZ=ON` (既定 ON) |

- `include/ergo/package_viz/`、`src/package_viz/`、`tests/package_viz/`、`spec/module/package_viz.md`。
- 依存の向き: `package_viz → tela_graph` は `render_viz → tela_graph` と同じ (tela_graph は「共通出口」として
  下層に置かれている)。`render_viz` や `render` は import しない。Pictor / Tela のヘッダも読まない
  (JSON と git の出力だけを読む) ので **Pictor 無しで常時ビルド・テスト可**。
- `ergo_common::jsonm` が File API の reply (配列・ネスト・数値) を読めるかは実装時に確認する。足りなければ
  `ergo_common` に読み取り専用のパーサ拡張を足す (新規 third-party JSON ライブラリは入れない)。
- `spec/domains/observability.domain.json` の membership に `package_viz` を追加。`cmake/packages/` は
  `foundation` (ビルド定義) に自然に入る。`module_list.md` / `module_list.yaml` / `README.md` に行を追加。

#### `ergo_render` との整合

`ergo_render` は今まで通り `if(TARGET pictor)` と `PICTOR_HAS_VULKAN` だけを見る。変わるのは
**`pictor` ターゲットを作る主体がホストから Ergo に移る**ことだけで、`RenderContext` / `IRenderLayer`
などの公開 API は触らない。multi-RHI で Pictor の公開面が変わるときは、その設計に従って `ergo_render`
が別途追随する (本設計の範囲外)。

#### tools/ergo との関係

- `tools/ergo` の `render_pipeline` プラグインが `Pictor/profiles/*.profile.json` を読む経路は、
  `resolved.json` の `exports.PICTOR_PROFILE_DIR` を参照できるようにする (段階 S7、任意)。
- 同じ `resolved.json` を読む `packages` プラグイン (Web で版一覧・リンク図を見る) は **任意の後段**。
  Tela が出口であることを変えない。

### 4.8 既存ホストの移行 (依頼 4-8)

ホストの取り込み方は今 2 系統ある。

- **系統 1: 兄弟チェックアウト + `add_subdirectory`** (AdventureCube: `../Pictor` と `external/ergo`、
  Ludellus-Native: `external/ergo` のみ、Pictor は任意)
- **系統 2: SHA 固定の FetchContent** (Pictor と Ergo を別々の SHA で取得し、自前の exact-source 検証を持つホスト)

| 段 | 系統 1 の手順 | 系統 2 の手順 | Ergo 側の挙動 |
|---|---|---|---|
| 0 (Ergo 更新直後、ホスト無変更) | 何もしない | 何もしない | ホストが作った `pictor` を検出 → EPK-006 WARNING「ホスト供給・版不明」。ビルドは今まで通り通る |
| 1 (明示) | `set(ERGO_PKG_PICTOR_PROVIDED ON)` を `add_subdirectory(ergo)` の前に 1 行 | 同じ | WARNING 消える。resolved.json に `provenance: host-provided` |
| 2 (委譲) | ホストの `add_subdirectory(../Pictor)` を削除し、`-DERGO_PKG_PICTOR=ON -DERGO_PKG_PICTOR_SOURCE_DIR=../Pictor` (開発機) または pin fetch (CI) | ホストの Pictor FetchContent と exact-source 検証関数を削除し `-DERGO_PKG_PICTOR=ON`。Ergo の pin が Pictor の版の正本になる | Ergo が Pictor を供給。drift は WARNING |
| 3 (整理) | `PICTOR_BUILD_*` の `FORCE` 設定をホストから外す (Ergo の defaults に任せる。必要な上書きだけ残す) | 同じ | — |
| 互換終了 | — | — | EPK-006 を FATAL に切り替える (別 PR)。`_PROVIDED=ON` は **恒久に残す** (明示なので無言ではない。Tela を独自に組むホストなどの逃げ道) |

互換期間: **既知の 3 ホストすべてが段 2 まで終わるか、Ergo の次の minor リリースのどちらか遅い方**
(§7 の人間判断)。

Ludellus-Native の Emscripten 化は、Pictor の描画方式の切り替え口が main に入ってから段 2 を適用する。
それまでは `platforms` フィルタで「Emscripten では `vulkan` を選べない」ことが構成エラーとして出るだけ。

### 4.9 段階計画と受入条件 → §5
### 4.10 リスクと未決事項 → §6 / §7

---

## 5. 段階計画と受入条件 (依頼 4-9)

各段階は単独で main に入れられ、前段無しで後段を入れない。「実機確認」は人の許可が要るものとして
計画に書き、未確認なら未確認と記す。

| 段 | 内容 | 受入条件 (自動テスト) | 実機で確かめること (未確認前提) |
|---|---|---|---|
| **S1 マニフェスト + エンジン** | `ergo-packages.json` (Pictor / Tela / Lapilli / MUSA 系の初期行)、`cmake/ErgoPackages.cmake`、`cmake/packages/pictor.cmake` / `tela.cmake`、CMake 3.20 への引き上げ、`spec/data/ergo-packages.md`、`spec/setup/packages.md`、domains membership | `cmake -P` テスト: SHA 形式 (EPK-001)、出所決定順、drift 判定 (none/ahead/behind/diverged/unknown)、`expect_tag` 照合、resolved.json のスキーマ。素の Ergo configure (`ERGO_PKG_*=OFF`) が今と同じ結果になる (回帰無し) | `-DERGO_PKG_PICTOR=ON` で Pictor が pin で取得され、Vulkan SDK がある desktop で `ergo_render` の実経路が有効になること。`_SOURCE_DIR` オフライン経路。MSVC と GCC 両方 (GCC で落ちる前例) |
| **S2 検査の完成** | EPK-005〜013、feature 組合せ (`platforms` / `conflicts` / `reserved`)、循環、二重供給、`ERGO_PKG_STRICT`、finalize の全件列挙 | 各 EPK を 1 ケースずつ `cmake -P` で再現。Emscripten toolchain を模した変数で `vulkan` 拒否 / `web` 自動 ON を確認 | 実 Emscripten での configure (Pictor が `pictor_webgl` だけ作ること)。**wasm 実行は対象外** |
| **S3 `ergo_package_viz` 本体 + 図 A** | スナップショット値型、`layout_inventory`、resolved.json リーダ、CLI、`ergo_package_graphs` カスタムターゲット、`spec/module/package_viz.md`、module_list 更新 | gtest: 配置の決定性、グループ上限、drift 印、`validate_tela_graph` を通ること、合成 resolved.json からの図。Tela がある構成では `test_tela_graph_adapter` と同様に Tela 本体の `load_graph` が受理 | `tela_view --graph` での見え方 (文字の収まり) |
| **S4 図 B (リンク関係)** | File API query 設置、codemodel リーダ、帰属 (sourceDir 最長一致)、`layout_links`、`--collapse` / `--focus`、超過時の拒否と提案 | gtest: 合成 codemodel reply (小さな fixture) からの辺、直接依存のみ、テスト除外、循環の `cycle` グループ、128/256 超過で書かないこと、collapse 後の同値性 | 実ホスト (AdventureCube) の build dir から図が出ること。ノード数の実測 |
| **S5 図 C (使用関係)** | `package.json` / `.gitmodules` リーダ、`relations` 取り込み、`layout_usage`、未観測ノード | gtest: fixture からの `uses` 辺、`port-of` 破線、未観測の残し方 | `SIBLING_ROOT=E:/Document/Ars` で Thaleia / Terpsichore / Lapilli の実ファイルが読めること |
| **S6 ホスト移行** (ホストごとに 1 PR、Ergo 外) | 系統 1 / 2 の段 1〜3 | 各ホストの既存 CI | 各ホストの実ビルド・起動 (ホスト側の許可に従う) |
| **S6' 互換終了** (Ergo、S6 完了後) | EPK-006 を FATAL に | `cmake -P` テスト更新 | — |
| **S7 任意** | `revisor version show` 注釈、tools/ergo `packages` プラグイン、`render_pipeline` の `PICTOR_PROFILE_DIR` 参照、`acquire: find_package` 経路 | 各々 | — |

テスト方針の補足:

- CMake 層は「ターゲットに触らない純関数 + `cmake -P`」で書き、configure 全体を回すテストは CI の 1 ジョブ
  (`ERGO_PKG_PICTOR=ON`、キャッシュ有り) に限る。
- C++ 層は Pictor / Tela 無しで常時テスト可。File API の fixture は実 reply から最小化した合成 JSON を
  `tests/package_viz/fixtures/` に置く。
- テストの実行は人の指示があるときだけ (依頼の前提)。

---

## 6. リスク (依頼 4-10 前半、推奨つき)

| リスク | 影響 | 推奨 |
|---|---|---|
| Pictor が desktop で glfw を暗黙 FetchContent する | Ergo 側で「オフラインで完結」を保証できない。CI で網が無いと落ちる | Pictor に `PICTOR_FETCH_GLFW` 相当の opt-out か `glfw3_DIR` 指定経路を依頼 (Pictor 側 PR)。それまでは `spec/setup/packages.md` に「desktop では glfw が系統 glfw3 か網経由」と明記 |
| CMake 最小版 3.16 → 3.20 | 古い環境の切り捨て | Pictor / Tela が既に 3.20 要求なので実質的な影響は無い。README に記す |
| Ergo の configure 時間が Pictor の分だけ延びる | 開発体験 | ホストが今払っているコストの移動で総量不変。`EXCLUDE_FROM_ALL` と共有キャッシュ。素の Ergo は既定 OFF |
| Windows のパス長 (FetchContent の `_deps/pictor-src/...` が深い) | MSVC で 260 文字超 | `ERGO_DEPENDENCY_CACHE_DIR` を短いパスに置く手順を setup に書く。既存機構 (`FETCHCONTENT_BASE_DIR`) で可能 |
| Pictor の pin 更新を誰がいつやるか | pin が古いまま放置 → ホストが Ergo 経由で古い Pictor を掴む | 「Pictor の変更を使いたいホストが Ergo の pin を上げる PR を出す」を規則にする。Ergo CI は pin で組むので壊れた pin はすぐ分かる。自動化 (Revisor のリリース hook で Ergo に PR) は S7 以降の検討 |
| 並行開発 (Pictor と Ergo を同時に直す) で drift が常態化 | WARNING が無視される | 開発機は WARNING、CI は strict。drift は図 A に `★drift` で必ず見える |
| リンク図のノード数が 128 を超える | 図 B が書けない | `--collapse` 既定を「ホストと Ergo 以外のパッケージは畳む」にし、展開は `--expand <pkg>` で明示。超過時は提案を出して **部分的に書かない** |
| File API の reply 形式変更 | リーダが壊れる | codemodel v2 は安定版。`version` を検査し、違えば明示エラー |
| multi-RHI の結論で feature 名・Pictor オプション名が変わる | `cmake/packages/pictor.cmake` の写像修正 | 写像は 1 ファイルに閉じている。マニフェストの feature 名は `reserved` にしてあり、選ぶと構成エラーなので誤用はない |
| `ergo_common::jsonm` が File API の JSON を読めない | リーダ実装が止まる | S4 着手前に確認。足りなければ読み取り専用拡張 (新規 third-party は入れない) |
| MUSA 系の観測が `SIBLING_ROOT` 依存 | CI では常に「未観測」 | 未観測を図に残す設計で誤解を防ぐ。T3 の版は Concordia / Revisor に聞くのが本筋 (S7) |
| Tela 表示の実画面未確認 | 文字の収まり・線の重なり | `render_viz` と同じ未確認事項として spec に書き、人の確認を待つ |

---

## 7. 未決事項 (依頼 4-10 後半、推奨つき)

### 7.1 設計側で決めてよいが確認したい点 (推奨で進める)

| 事項 | 推奨 |
|---|---|
| 素の Ergo ビルドで Pictor / Tela を既定 ON にするか | **OFF**。今の「ネットワーク非依存・軽量」を保ち、ホストと CI の 1 ジョブが ON にする |
| Tela を T1 に入れるか (ヘッドレスのみ) | **入れる**。`ergo_tela_graph_tela` のビルド可否が pin で再現できる価値がある。表示側は別インストール |
| T3 (MUSA 系) をマニフェストに載せる範囲 | **Ergo と関係があるものだけ** (Clio / Thaleia / Terpsichore / Ars-Musa / Curare)。LUDIARS 全体は Concordia / Anatomia |
| ホスト設定ファイルの新設 | **しない**。CMake キャッシュ変数 + presets のみ |
| 図 B の畳み既定 | 「ホストと Ergo を展開、他は畳む」。`--expand` で開く |
| CLI のモジュール名 | `ergo_package_viz` (`ergo_render_viz` と対) |

### 7.2 人間 (neco) にしか決められない点

| 事項 | 選択肢 | 推奨 |
|---|---|---|
| **互換期間の長さ** (ホスト供給 `pictor` を FATAL にする時期) | (a) 既知 3 ホストの移行完了時 (b) Ergo 次 minor (c) 期限なし | (a) と (b) の遅い方。`_PROVIDED=ON` は恒久に残す |
| **Pictor の pin 更新の責務** | (a) Pictor を変えた人 (b) 使いたいホストの人 (c) Revisor の自動 PR | (b)。自動化は後 |
| **Tier 3 の版の情報源** | (a) 兄弟チェックアウトを読む (b) Concordia `/v1/project-codes` + Revisor `service-version` を叩く | 初版 (a)、S7 で (b) を加える。(b) はネットワークと認可が絡むので人の判断 |
| **Emscripten を本設計の受入に含めるか** | (a) 構成拒否 / 選択だけ (b) wasm 実行まで | (a)。(b) は multi-RHI の結論待ち |
| **Pictor の glfw 暗黙取得の扱い** | (a) Pictor に opt-out を依頼 (b) 容認 | (a)。Pictor 側の PR が要る |
| **tools/ergo `packages` プラグイン** を作るか | (a) 作る (b) Tela だけ | (b) で開始、需要があれば (a) |
| **`ERGO_PKG_SIBLING_ROOT` を既定で `E:/Document/Ars` にするか** | (a) 既定なし (b) 開発機では presets で与える | (a)。presets の `local` 構成に (b) を書く。Ergo のソースに開発機のパスを書かない (公開リポ) |

---

## 付記: 今回触らないもの

- `ergo_render` の公開 API、`cmake/ErgoRenderBackend.cmake`、`ergo_tela_graph` / `ergo_render_viz` の実装。
- Pictor / Tela の CMake (依頼は Ergo 側の設計。Pictor への glfw opt-out は依頼事項としてリスクに記載)。
- `lib/lapilli` の取得方式 (submodule のまま)。
- 本書は設計のみ。実装・テスト実行・PR は行っていない。
