# 設計案 (Opus): Ergo による LUDIARS パッケージの統合リンク・管理と Tela 可視化

- 日付: 2026-10-14
- 対象: Ergo (Eg)。 可視化の表示は Tela (Tl)
- 依頼文: `spec/tasks/2026-10-14-package-hub-design-request.md` (コミット c5962a2)
- 立場: 二本立ての「Opus」側の独立案。 もう一方の案は見ていない。
- 状態: 設計のみ。 実装・ビルド・テストは一切していない。 本文中の「確認済み」は
  リポジトリのファイルを読んで確かめた事実だけを指す。

---

## 1. 結論

1. **Ergo に「パッケージ・ハブ」を足す。 ホストは `add_subdirectory(<ergo>)` の前に
   `ERGO_PACKAGES` (使うパッケージの名前の列) を置くだけで、 Pictor・Tela などが
   Ergo のマニフェストに書かれた版で揃う。** 下層パッケージは Ergo を知らないままにする
   (Pictor → Ergo の依存は作らない)。
2. **管理は 2 段に分ける。**
   - **リンク管理 (link)**: ホストのバイナリにリンクされる C++ / CMake パッケージ。
     版を固定し、 構成時に検査し、 食い違いがあれば止める。
   - **観測 (observe)**: TypeScript パッケージ・サービス・MUSA 系。
     ゲームにはリンクされないので固定も検査もしない。 使用関係と版を **読み取って図に出すだけ**。
   MUSA 系は観測の側に置く。 MUSA と C++ を結ぶのは「移植元」の関係だけで、
   今あるのは `ergo_resource` → Clio の 1 本。
3. **版の正本は Git のコミット SHA (完全形) とし、 マニフェスト 1 ファイルをロックも兼ねる
   正本にする。** Revisor の版 (`.revisor-version`) は固定に使えない。
   このファイルは `skip-worktree` で、 origin には `uninitialized` が載っている
   (Revisor `spec/feature/service-version.md`)。 そのため Revisor の版は
   **表示用の札** と割り切り、 図を作るときに導出する。 マニフェストには書かないので二重管理にならない。
4. **取り込みの順序は「ホストが既に用意したターゲット」 → 「明示したローカル・ソース」
   → 「インストール済み (`find_package` CONFIG)」 → 「明示的に許可された取得 (FetchContent)」。**
   兄弟ディレクトリを暗黙に探さず、 既定では取得しない。 これは Tela と Pictor の
   「暗黙に取りに行かない」方針と同じ。 取得の仕組みは既存の `cmake/ErgoDependencies.cmake`
   (固定・ローカル差し替え・取得) を **再利用** する。
5. **可視化は `ergo_render_viz` と並ぶ新しい可視化層 `ergo_package_viz` が担う。 出口は既存の
   `ergo_tela_graph` に揃える。** リンク関係は手で描かず機械的に集める。
   - ターゲットのリンク関係: CMake File API (codemodel-v2)
   - パッケージの解決結果: 構成時に Ergo が書き出す `ergo_packages.resolved.json`
   - 観測の使用関係: `package.json` / `.gitmodules` / マニフェストの宣言を、 根拠ファイル付きで読み取る
   図は 3 種類 (版一覧・ターゲットのリンク図・使用関係図) で、 Tela の上限は焦点と畳み込みで守る。
6. **既存ホストは何もしなければ今のまま動く (互換モード)。** `ERGO_PACKAGES` を置いた
   ホストだけが新方式に入る。 段階は 6 つで、 どれも単独で main に入れられる。

---

## 2. 解釈 (親の解釈への同意・異論)

| # | 親の解釈 | 判定 | 根拠と修正 |
|---|---|---|---|
| a | 「コンテンツ」= 利用側ホスト (ゲーム) | **同意** | AdventureCube は `add_subdirectory(../Pictor)` と `external/ergo` で個別に取り込む。 非公開の別ホストは FetchContent で Pictor と Ergo の SHA を別々に固定し、 自前の SHA 検証関数を持つ。 Ludellus-Native は `ergo_math` / `ergo_physics2d` だけを使う (各 `CMakeLists.txt` で確認済み)。 「各ホストがそれぞれ固定と検証を書いている」重複を Ergo に寄せる価値がある |
| b | ホストが Ergo だけを取り込めば、 Ergo・Pictor・Tela・**Lapilli** などの C++ ライブラリが揃う | **一部異論** | **Lapilli は C++ ライブラリではない。** pnpm の TypeScript モノレポ (`@ludiars/*`) で、 Ergo では `tools/ergo` (Node) がサブモジュール `lib/lapilli` の `@ludiars/one-shot` を `file:` 依存で使っているだけ。 ホストのバイナリにはリンクされない。 リンク管理の対象から外し、 観測の対象にする |
| c | 版は「コミット / タグ / **Revisor の版**」でマニフェストに持つ | **一部異論** | Revisor の版ファイルは `skip-worktree` で、 origin は `uninitialized` のまま。 Revisor 自身も「状態に持たず毎回導出する」と決めている (`service-version.md`)。 Ergo のマニフェストに Revisor の版を書くと、 必ずずれる 2 つ目の正本になる。 **固定は SHA だけ。 Revisor の版は図の札として導出する** |
| d | 版の食い違い・足りない依存を構成時に検出する | **同意** | 規則は §4.4 にまとめた。 Ergo の既存方針「無言フォールバック禁止」(`ErgoDependencies.cmake`) と同じく FATAL_ERROR で止める |
| e | MUSA = 女神名のプロジェクト群 + Ars-Musa | **同意。 ただし位置付けを修正** | 確認した範囲では次のとおり。 Clio は Python、 Thaleia はサービス (TS)、 Terpsichore は TS パッケージ (Actio がサブモジュールで取り込む)、 Ars-Musa はデスクトップアプリ (`melpomene/src-tauri`) と各女神のディレクトリ。 **どれもゲームのバイナリにリンクされない。** 「MUSA が何を使っているか」は観測の側で図にする。 C++ との接点は `ergo_resource` が Clio の C++ 移植であること (`spec/module/resource.md`) だけ |
| f | Pictor → Ergo の依存は作らない。 Ergo が集約点になる | **同意** | Pictor `AGENTS.md`「Ergo は主な consumer で、 Pictor の必須依存にしない」。 ハブの仕組みはすべて Ergo 側に置き、 下層パッケージには何も足さない (§4.3 の Tela の件だけ、 Tela 側の任意改善として別に挙げる) |
| g | 描画パイプライン可視化と同じ出口を使う | **同意** | `ergo_tela_graph` は自分の仕様に「今後のパッケージ構成の可視化も同じ出口を使う」と書いている |

追加の論点が 1 つある。 **Ergo は公開リポジトリなので、 マニフェストに非公開リポジトリを書けない。**
非公開のパッケージや非公開ホストの固有事情は、 ホスト側の追加マニフェスト (§4.2 の
`ERGO_PACKAGE_MANIFEST_EXTRA`) へ逃がす。

---

## 3. 比較した案

### 3.1 取り込みの仕組み

| 案 | 内容 | 利点 | 欠点 | 採否 |
|---|---|---|---|---|
| A. git サブモジュール集約 | Ergo が `external/pictor` などをサブモジュールで持つ | 版がコミットに固定され、 オフラインで再現できる | ホストが既に持つ Pictor と二重になる。 worktree ごとにサブモジュールの実体が要る。 公開リポに非公開サブモジュールを置けない。 Tela の「兄弟を暗黙に取らない」と相性は良いが、 Ergo 単体のクローンが重くなる | 不採用 |
| B. FetchContent 一本 | マニフェストの SHA から常に取得する | 実装が単純 | 既定でネットワークに出る (既存方針に反する)。 ローカルの worktree で編集中の Pictor を差し込みにくい | 単独では不採用 (D の一経路として使う) |
| C. `find_package` 一本 | 各パッケージを install して CONFIG で探す | CMake の正道。 Tela は対応済み | Pictor は install / export を持たない。 開発中のソースを直に使う今の運用に合わない | 単独では不採用 (D の一経路として使う) |
| **D. 解決順つきの宣言的ハブ** | マニフェストの SHA を正本にし、 「ホスト供給 → ローカル・ソース → インストール済み → 許可された取得」の順に解決して、 全経路で同じ検査をかける | 今のホストの形 (ホスト供給) を互換で受けられる。 オフラインと worktree はローカル・ソースで、 CI は取得か事前展開で対応できる。 既存エンジンを再利用できる | 経路が 4 本あるので検査を経路に依存させない作りが要る | **採用** |

### 3.2 リンク関係の収集

| 案 | 採否 | 理由 |
|---|---|---|
| CMake File API (codemodel-v2) | **採用 (ターゲット)** | 構成済みの実ターゲットと依存 (`dependencies`) と所属ディレクトリが JSON で取れる。 構成ごと・プラットフォームごとに正確 |
| `cmake --graphviz` | 不採用 | 別実行が要り、 dot をもう一度解析することになる。 パッケージ境界 (どのソースツリーのターゲットか) が取れない |
| 宣言 (マニフェストの `requires`) | **採用 (パッケージ間)** | パッケージ間の「要求」は意図なので宣言が正本になる。 File API で得た実リンクと突き合わせ、 宣言に無い実リンクは「宣言漏れ」として図と検査に出す |
| ソース解析 (`#include` 走査など) | 不採用 (将来 Anatomia 連携) | Ergo の責務を超える。 観測の側で深い解析が要るなら Anatomia の結果を読む (§7 の未決 U4) |

### 3.3 可視化の切り方

| 案 | 採否 | 理由 |
|---|---|---|
| `ergo_render_viz` を拡張する | 不採用 | 描画パイプラインとパッケージ構成は別の責務 (SRP)。 モジュール間の cross-import も禁止されている |
| `tools/ergo` (Web) のプラグインにする | 当面不採用 | 人間の指示は「可視化は Tela で」。 Web 側は後で JSON を読むだけのプラグインとして足せる (§7 U6) |
| **新しい可視化層 `ergo_package_viz` + CLI、 汎用の配置は下層 `ergo_graph_layout`** | **採用** | `ergo_render_viz` と並ぶ形 (ライブラリ + CLI + 出口は `ergo_tela_graph`) にそろう |

---

## 4. 推奨案の詳細

### 4.1 対象範囲 (依頼 1)

| 区分 | 対象 | 扱い | 理由 |
|---|---|---|---|
| **link** (固定・検査・取り込み) | Pictor、 Tela、 将来の LUDIARS 製 C++ ライブラリ (ゲーム文表示の C++ ライブラリなど、 ホストへリンクされるもの) | マニフェストで SHA を固定する。 ハブが取り込み、 構成時に検査する | ホストのバイナリの再現性に直接効く |
| link の付属物 | パッケージ同梱のシェーダ・既定プロファイル・フォントなど | **パッケージの版に従う** (別に固定しない) | 同じツリーから来るので SHA 1 つで決まる。 別に固定すると食い違いの元になる |
| Ergo 自身 | Ergo のモジュール群 | ホストが Ergo の SHA を固定する (ここだけはホスト側の責務) | 入口そのもの。 Ergo の SHA が決まればマニフェストも決まる |
| 第三者ライブラリ | kazmath / curl など | 既存の `third_party/dependencies.cmake` のまま。 **図には出す** | 既に固定と取得の仕組みがある。 二つ目の管理を作らない |
| **observe** (読み取りのみ) | Lapilli (TS)、 MUSA 系 (Clio / Thaleia / Terpsichore / Ars-Musa)、 `tools/ergo` の npm 依存 | 版と使用関係を読み取って図にする。 固定も検査もしない | ゲームにリンクされない。 固定は各リポ (サブモジュール・lockfile) の責務 |
| 対象外 | サービスの稼働中の版 (health) | 扱わない | Revisor の service-version の領分。 観測の図には Revisor が返す値を札として貼るだけ |

MUSA の位置付け: **「判断を移譲する外側の道具群」として観測の図に 1 グループで出す。**
C++ との接点は、 マニフェストの宣言 `ported_from` (例: `ergo_resource` ← Clio) で結ぶ。
移植元の版と移植先の版がずれたかどうかは今は判定しない。 「移植元のどの SHA に合わせたか」を
宣言に書けるようにしておき、 判定は将来の課題とする (§7 U5)。

### 4.2 マニフェスト (依頼 2)

**置き場所**: `packages/ludiars.packages.json` (Ergo のリポ直下に `packages/` を新設)。
JSON にする理由は 3 つ。 CMake 3.19 以降の `string(JSON)`、 C++ の CLI、
`tools/ergo` (Node) のどれからも同じファイルを読めること。
Ergo の `cmake_minimum_required` は今 3.16 なので **3.20 へ上げる** (Pictor と Tela は既に 3.20)。
これは段階 1 の PR に含める。

**形 (案)**:

```json
{
  "schema": 1,
  "packages": {
    "pictor": {
      "kind": "link",
      "code": "Pc",
      "repository": "https://github.com/LUDIARS/Pictor.git",
      "rev": "<40 桁の SHA>",
      "cmake": {
        "targets": ["pictor"],
        "find_package": null,
        "cache": { "PICTOR_BUILD_DEMO": "OFF", "PICTOR_BUILD_TESTS": "OFF", "PICTOR_BUILD_TOOLS": "OFF" }
      },
      "features": {
        "postprocess": { "default": true,  "cache": { "PICTOR_ENABLE_POSTPROCESS": "ON" } },
        "decals":      { "default": true,  "cache": { "PICTOR_ENABLE_DECALS": "ON" } },
        "webgl2":      { "default": false, "cache": { "PICTOR_BUILD_WEBGL": "ON" }, "platforms": ["emscripten"] }
      },
      "requires": {}
    },
    "tela": {
      "kind": "link",
      "code": "Tl",
      "repository": "https://github.com/LUDIARS/Tela.git",
      "rev": "<40 桁の SHA>",
      "cmake": { "targets": ["Tela::Graph", "Tela::Core"], "find_package": { "name": "Tela", "version": "0.1" } },
      "features": {},
      "requires": {}
    }
  },
  "modules": {
    "ergo_render":     { "needs": ["pictor"] },
    "ergo_tela_graph": { "optional": ["tela"] },
    "ergo_render_viz": { "optional": ["pictor"] },
    "ergo_resource":   { "ported_from": [{ "package": "clio", "rev": null }] }
  },
  "observe": {
    "clio":   { "code": "Cl", "path_hint": "Clio",   "group": "musa" },
    "lapilli": { "code": "La", "path_hint": "Lapilli", "group": "shared-ts" }
  }
}
```

- `rev` は **完全な SHA だけ** を受け付ける (タグやブランチ名は不可)。 タグは付け替えられるので再現性の正本にしない。
  タグ名は図の札として導出する (§4.6)。
- `features` は「パッケージの任意機能 → そのパッケージの CMake キャッシュ変数」の対応表。
  Pictor の描画方式の切り替え口 (並行の多方式設計) で方式ごとの任意モジュールが増えたら、
  ここに `vulkan` / `webgpu` / `dx12` などを足すだけで受けられる。 `platforms` で
  「その機能が成り立つプラットフォーム」を、 `conflicts` で「同時に選べない機能」を書く。
- `modules` は Ergo 自身のモジュールがどのパッケージを要るか (`needs` = 無いと構成エラー、
  `optional` = あれば使う) の宣言。 今は `CMakeLists.txt` の中に `if(TARGET pictor)` として
  散らばっている暗黙の依存を、 図と検査に出せるようにする。
- **ロック**: マニフェスト自体がロックを兼ねる (SHA を書くのでロックファイルを別に持たない)。
  ロックファイルを別に持つと「マニフェストとロックの食い違い」という検査対象が 1 つ増えるだけで、
  得るものが無い。

**ホストごとの選択と上書き** (すべて `add_subdirectory(<ergo>)` より前に置くキャッシュ変数):

| 変数 | 意味 |
|---|---|
| `ERGO_PACKAGES` | 使う link パッケージの列 (例 `pictor;tela`)。 **未設定なら互換モード** (§4.8) |
| `ERGO_PACKAGE_FEATURES_<NAME>` | 機能の選択。 `+webgl2;-decals` のように既定から足し引きする |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | ローカル・ソース (兄弟チェックアウトや worktree) を明示する |
| `ERGO_PACKAGE_REV_<NAME>` | SHA の上書き。 上書きした事実は報告と図に必ず残る。 `ERGO_PACKAGES_STRICT=ON` では構成エラー |
| `ERGO_PACKAGES_FETCH` | 取得を許すか。 **既定 OFF** (CI のプリセットだけ ON にする) |
| `ERGO_PACKAGES_STRICT` | 食い違いを警告で済ませず必ず止める。 CI と出荷構成で ON |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | ホスト側の追加マニフェスト (非公開パッケージ用)。 Ergo のマニフェストと同じ形で、 同名のパッケージがあれば構成エラー (上書きを黙って許さない) |

**Revisor の版との関係**: マニフェストには Revisor の版を書かない。 図を作るとき (§4.5) に
`revisor version show` が使えればその値を札にし、 使えなければ `git describe --tags` の
`<tag>+<距離>` を札にする。 Revisor と同じ「公開済みの版 + 距離」の書き方にそろえる。
SHA が正本で札は導出値なので、 二重管理にならない。

### 4.3 取り込みの仕組み (依頼 3)

**ホストが書くこと**:

```cmake
set(ERGO_PACKAGES pictor tela CACHE STRING "")
set(ERGO_PACKAGE_FEATURES_PICTOR "+postprocess" CACHE STRING "")
add_subdirectory(external/ergo)
target_link_libraries(game PRIVATE ergo_render pictor)
```

**Ergo の中の順序**: Ergo のモジュールは構成時に `if(TARGET pictor)` で分岐する。 そのため
ハブの解決は **トップレベル `CMakeLists.txt` の先頭** (第三者依存の include の直後で、 どの
モジュールの節よりも前) で行う。 `include(cmake/packages/ErgoPackages.cmake)` を 1 行足すだけにする。

**1 パッケージの解決順** (最初に当たった経路を使う。 どの経路でも §4.4 の同じ検査を通す):

1. **ホスト供給**: ターゲットが既にある (ホストが先に `add_subdirectory(Pictor)` した)。
   今のホストの形をそのまま受ける経路。 ソースディレクトリはターゲットの `SOURCE_DIR` プロパティから引き、
   その git HEAD を固定値と突き合わせる。
2. **ローカル・ソース**: `ERGO_PACKAGE_SOURCE_DIR_<NAME>` が指すツリーを `add_subdirectory` する。
   兄弟チェックアウトや worktree はここで扱う。 **兄弟を自動では探さない** (Tela の README と同じ方針)。
   手元の利便のためには `CMakeUserPresets.json` (git 管理外) の例を `spec/setup/packages.md` に載せる。
3. **インストール済み**: マニフェストに `find_package` が書かれたパッケージ (今は Tela) だけ。
   `find_package(<Name> <version> CONFIG)` で探す。 SHA を確かめられないので、 報告には「版のみ照合」と残す。
   STRICT のときは、 パッケージの config が SHA を公開していない限りこの経路を使わない (§7 U3)。
4. **取得**: `ERGO_PACKAGES_FETCH=ON` のときだけ。 既存の `cmake/ErgoDependencies.cmake`
   (`ergo_declare_dependency` / `_ergo_fc_declare`) を再利用して、 固定の SHA で FetchContent する。
   取得先の共有キャッシュも既存の `ERGO_DEPENDENCY_CACHE_DIR` を共用する。
5. どれにも当たらなければ **構成エラー**。 エラー文には、 ソースの指し方と取得の許し方を具体的に書く。

**「暗黙に取りに行かない」方針との両立**: 既定では取得 OFF で、 兄弟も探さない。
ハブが新たに足すネットワーク経路は「ホストが明示的に `ERGO_PACKAGES_FETCH=ON` にしたとき」だけ。
既存の第三者依存の `ERGO_FETCH_DEPENDENCIES` (既定 ON、 ただし個々の依存は既定 OFF) とは
変数を分ける。 LUDIARS のパッケージと第三者ライブラリでは「勝手に取ってよいか」の答えが違うため。

**環境ごとの動き**:

| 環境 | 動き |
|---|---|
| オフライン | ローカル・ソースかホスト供給だけで閉じる。 取得 OFF が既定なので、 何もしなければネットワークに出ない |
| CI | プリセット `ci-packages` で `ERGO_PACKAGES_FETCH=ON` と `ERGO_PACKAGES_STRICT=ON` を立てる。 取得キャッシュはジョブ間で共有してよい (SHA 固定なので内容は不変) |
| worktree | `ERGO_PACKAGE_SOURCE_DIR_PICTOR=<Pictor の worktree>`。 SHA が固定値と違えば、 非 STRICT なら警告と報告 (「上書き相当」)、 STRICT なら停止。 未コミットの変更があれば `dirty` として報告と図に出す |
| Emscripten | 仕組みは同じ (`emcmake cmake`)。 `features[].platforms` で Emscripten で成り立たない機能 (デスクトップ専用の描画方式など) を構成時に弾く。 ハブの判定は `CMAKE_SYSTEM_NAME STREQUAL "Emscripten"` を `emscripten` という名前のプラットフォームとして扱う (既存の `ErgoRenderBackend.cmake` の desktop / android / ios と同じ名前の体系に 1 つ足す)。 **Emscripten で実際に構成できるかは未確認** (Pictor の WebGL2 / WebGPU 側の対応に依存する) |

**Tela と Pictor の結合について**: Tela の Pictor 用アダプタ (`TELA_BUILD_PICTOR` など) は、
`find_path` / `find_library` でインストール済みの Pictor を探す作り
(Tela `CMakeLists.txt` で確認済み)。 ハブが `pictor` ターゲットとして供給した Pictor をそのまま使えない。
ハブの最初の版では **Tela の Pictor 用アダプタは機能として持たない** (`ergo_tela_graph` が使うのは
`Tela::Graph` だけで、 Pictor は不要)。 Tela 側で「既に `pictor` ターゲットがあればそれを使う」
分岐を入れるかは、 Tela の判断として §7 に分けた。

### 4.4 検査 (依頼 4)

すべて構成時に判定する。 純粋な規則は `cmake/packages/ErgoPackageRules.cmake` にまとめる
(入出力はリスト変数だけで、 ファイルもターゲットも触らない)。 規則は `cmake -P` で単体テストできる。

| ID | 規則 | 非 STRICT | STRICT |
|---|---|---|---|
| PK-1 | `ERGO_PACKAGES` に未知の名前がある | エラー | エラー |
| PK-2 | 選んだパッケージがどの経路でも解決しない | エラー | エラー |
| PK-3 | 解決したソースの HEAD が固定 SHA と違う (ホスト供給・ローカル・ソース) | 警告と記録 | エラー |
| PK-4 | `ERGO_PACKAGE_REV_<NAME>` で上書きしている | 警告と記録 | エラー |
| PK-5 | ソースに未コミットの変更がある (`dirty`) | 記録 | エラー |
| PK-6 | `requires` (パッケージ間の要求。 `>=` の版の下限も書ける) が満たされていない。 選ばれていない、 または `project(VERSION)` が下限未満 | エラー | エラー |
| PK-7 | `requires` に循環がある (トポロジカル順が作れない) | エラー | エラー |
| PK-8 | 機能がそのプラットフォームで成り立たない、 または `conflicts` の組合せを選んだ | エラー | エラー |
| PK-9 | 機能が宣言に無いキャッシュ変数を、 ホストが別の値で FORCE 済み (ホストと Ergo で同じ変数を奪い合っている) | 警告と記録 | エラー |
| PK-10 | `modules.<m>.needs` のパッケージが選ばれていないのに、 そのモジュールの `ERGO_BUILD_*` が ON | エラー (互換モードでは今の挙動のまま) | エラー |
| PK-11 | 同じターゲット名を 2 つのパッケージが提供している (ホスト追加マニフェストとの衝突も含む) | エラー | エラー |
| PK-12 | File API で得た実リンクに、 宣言 (`requires` / `needs` / `optional`) に無いパッケージ間リンクがある | 図に「宣言漏れ」として出す | `ergo_package_viz --check` が非 0 で終わる (CI で使う) |

「黙って通さない」の線引き: **非 STRICT でも、 警告で通したものは全部
`ergo_packages.resolved.json` の `findings` に残り、 図に印として出る。**
警告のログが流れて見落とされても、 図を見れば分かる。

PK-12 だけは構成時に判定しない。 File API の返答は generate の後にしかできないため、
可視化 CLI の検査モードで扱う。 構成時に止めたいときは、 CI でこの検査を 1 段挟む。

### 4.5 リンク関係の収集 (依頼 5)

3 つの入力をそれぞれ機械的に作る。 手書きの図は作らない。

1. **`<build>/ergo_packages.resolved.json`** (構成時にハブが書く)
   - パッケージごとの記録: 固定 SHA、 実際の SHA、 経路 (host / source / installed / fetched)、
     `dirty`、 選んだ機能、 上書きの有無、 `project(VERSION)`、 プラットフォーム、 `findings` (PK-* の記録)
   - Ergo のモジュールごとの記録: ON/OFF と、 OFF になった理由 (既存の
     `ERGO_RENDER_BACKEND_REASON` などをそのまま写す)
   - 書き出しは `file(CONFIGURE)` でまとめて 1 回。 構成が同じなら同じバイト列になる (決定的)。
2. **CMake File API の codemodel-v2 返答**
   - ハブが構成時に `<build>/.cmake/api/v1/query/client-ergo/codemodel-v2` を置く。
     クライアント用の問い合わせは generate の段で読まれるので、 同じ構成の実行で返答が `reply/` にできる。
   - ターゲットからパッケージへの対応付けは、 各ターゲットの `paths.source` が
     どのパッケージのソースディレクトリの下にあるか (resolved.json が持つ) で決める。
     Ergo のモジュールは `module_list.yaml` の `name` で引く。
   - 既定で除外するもの: テスト (`test_*`)、 ダミー (`*_dummy`)、 ベンチマーク、 `ergo_gtest_main`。
     リンク関係を見たいのは製品のターゲットだから。 `--include-tests` で含められる。
3. **観測の使用関係** (`ergo_package_viz scan` が読む)
   - `package.json` の `dependencies` / `devDependencies` (`file:` / `workspace:` / `@ludiars/*`)
   - `.gitmodules` (サブモジュールの取り込み。 例: Ergo → Lapilli、 Actio → Terpsichore)
   - マニフェストの `ported_from` (例: `ergo_resource` ← Clio)
   - 辺には必ず **根拠** (ファイルとキー) を持たせ、 図の辺の札に出す。 根拠の無い辺は作らない。
   - 読み取る範囲は、 マニフェストの `observe.<name>.path_hint` を `--root <dir>` から解決した
     ディレクトリだけ。 ディレクトリを総なめに走査しない (ハンドル枯渇を避け、 範囲を明示するため)。
   - サービス間の HTTP 呼び出しは今回は集めない (§7 U4)。

### 4.6 可視化 (依頼 6)

**図の種類**:

| 図 | ノード | 辺 | グループ (≤ 8) |
|---|---|---|---|
| **P1 パッケージ版一覧** | パッケージ (link・Ergo・第三者・observe)。 札は「名前 / 版の札 (`2.2.0+12`) / 短い SHA / 選んだ機能」と印 (`固定と不一致` `dirty` `上書き` `宣言漏れ`) | `requires`、 `ported_from` (破線) | Ergo / 描画 / 可視化 / 第三者 / 観測: MUSA / 観測: 共有 TS / ホスト (最大 7) |
| **P2 ターゲットのリンク図** | ターゲット | `dependencies` (直接の依存だけ) | Anatomia の層の宣言 (`.anatomia/layers.json`: presentation / domain-logic / infrastructure / application / shared の 5 つ) + 外部パッケージ + 第三者 = 7 |
| **P3 使用関係図 (MUSA と関連パッケージ)** | リポ / パッケージ | 観測の辺 (根拠つき)。 サブモジュールは実線、 `file:` 依存は実線、 `ported_from` は破線 | MUSA / 共有 TS / Ergo / エンジン / ツール (最大 5) |

P2 のグループに Ergo のドメイン (14 個) を使わず Anatomia の層 (5 個) を使う理由:
ドメインは Tela のグループ上限 8 を超える。 層は既にリポジトリで追跡されている宣言で、
「どの層がどの層に依存しているか」は図で見たい性質そのものでもある。

**上限 (8 グループ / 128 ノード / 256 辺) に収める手順** (決定的、 この順で上限に収まった所で止める):

1. **既定の除外**: テストやダミーなど (§4.5)。
2. **推移簡約**: A→B→C があるときの A→C を描かない (辺の数を減らす)。 消した辺の数は図の題に「(簡約 n 本)」と添える。
3. **焦点と畳み込み**: `--focus <package|layer|target>` で選んだ範囲だけをターゲット単位で展開する。
   ほかのパッケージは 1 ノードに畳む (「Pictor (12 ターゲット)」)。 焦点を指定しないときは、
   Ergo を展開して外部パッケージを畳む。
4. **分割**: それでも超えるときは、 層ごとに分けて `out.1.tela`, `out.2.tela`, … を書き、
   最後に「どのファイルに何があるか」の索引図 `out.index.tela` を書く。
   上限超過のまま 1 枚を書くことはしない (`ergo_tela_graph` の検査が拒むので、 そもそも書けない)。

**配置の規則** (`ergo_graph_layout`):
- 層別配置。 依存の向きは左 (使う側) → 右 (使われる側)。 列は最長経路で決める。
- 列の中の順は、 重心法で 2 往復した後に ID 名で順序を固定する (同じ入力なら同じ図)。
- 辺は縦横の折れ線にし、 列の間の通路を通す。 長さ 0 の区間は作らない。
  これは `ergo_render_viz` が満たしている性質と同じで、 Tela は経路を描き直さない
  (Tela `SPEC-TL-GRAPH`) ため。
- ノードの寸法は札の行数で決める。 キャンバスは ±100000 の範囲に収める。

**モジュール / CLI / ツールの切り方**:

| ターゲット | 役割 | 依存 | ビルド条件 |
|---|---|---|---|
| `ergo_graph_layout` | 汎用の層別配置。 抽象グラフ (ノード寸法と辺) → 配置済みの `tela_graph::Graph` | `ergo_tela_graph` | 常時 |
| `ergo_package_model` | resolved.json・File API 返答・観測スキャン → 値型 `PackageModel` (読むだけ) | `ergo_common` (JSON) | 常時 |
| `ergo_package_viz` | `PackageModel` → P1 / P2 / P3 の抽象グラフ (グループ分け・畳み込み・簡約・分割)。 配置は `ergo_graph_layout` に任せる | `ergo_package_model`, `ergo_graph_layout`, `ergo_tela_graph` | 常時 |
| `ergo_package_viz` (CLI、 ターゲット名 `ergo_package_viz_cli`) | `tools/package_viz/main.cpp`。 `ergo_package_viz <build-dir> --view packages\|targets\|usage [--focus X] --out f.tela`、 `scan --root <dir>`、 `--check` (PK-12) | 上記 | 常時 (Pictor にも Tela にも依存しない) |

どれも Pictor と Tela に依存しないので、 Ergo 単体の素のビルドでもテストできる。

**Tela で表示する手順**:

```sh
cmake -S host -B build -DERGO_PACKAGES="pictor;tela" -DERGO_PACKAGE_SOURCE_DIR_PICTOR=../Pictor ...
ergo_package_viz build --view packages --out packages.tela
ergo_package_viz build --view targets  --focus ergo --out targets.tela
tela_view --font <file.ttf> --graph packages.tela
```

描画パイプラインの可視化 (`ergo_render_viz --preset High --out high.tela`) と同じく、
「CLI で `.tela` を書いて `tela_view` で開く」の 1 本道にそろえる。

### 4.7 Ergo の中での配置 (依頼 7)

```
packages/ludiars.packages.json          マニフェスト (正本)
cmake/packages/ErgoPackages.cmake       解決 (経路の選択と add_subdirectory / find_package / 取得)
cmake/packages/ErgoPackageRules.cmake   純粋な検査規則 (PK-1〜11)
cmake/packages/ErgoPackageReport.cmake  resolved.json の書き出しと File API の問い合わせの設置
include/ergo/graph_layout/  src/graph_layout/  tests/graph_layout/
include/ergo/package_model/ src/package_model/ tests/package_model/
include/ergo/package_viz/   src/package_viz/   tests/package_viz/
tools/package_viz/main.cpp
tests/packages/                          cmake -P の規則テストと構成テスト用の小さな偽パッケージ
spec/setup/packages.md                   ホスト向けの使い方
spec/module/{graph_layout,package_model,package_viz}.md
```

- **SRP**: 「解決」「規則」「報告」を CMake の 3 ファイルに分ける。 既存の
  `ErgoDependencies.cmake` (仕組み) / `deps/<name>.cmake` (結線) / `dependencies.cmake` (登録)
  の 3 層と同じ考え方。
- **cross-import の禁止**: 依存の向きは `package_viz → package_model / graph_layout → tela_graph` の
  一方向だけ。 `ergo_render_viz` とは互いに import しない。 汎用の配置を `ergo_graph_layout` という
  下層に出すのは、 規約の「共有は下層へ」に従うため。 `ergo_render_viz` が自前の配置を
  `ergo_graph_layout` へ寄せるかは今回の範囲外で、 寄せるとしても別 PR にする。
- **`ergo_render` との整合**: `ergo_render` は今と同じく「`pictor` ターゲットがある」ことを前提にし、
  コードは変えない。 `ErgoRenderBackend.cmake` も変えない。 ハブは `pictor` ターゲットを
  モジュールの節より前に用意する役に徹する。 実描画経路の可否 (`PICTOR_HAS_VULKAN`) の判定は、
  これまでどおり `ergo_render_resolve_backend` が正本になる。 Pictor が方式ごとの任意モジュールに
  分かれたら、 ハブ側はマニフェストの `features` に方式を足し、 `ErgoRenderBackend` 側は
  Pictor が公開する契約を読む。 ハブは方式の可否を判定せず、 選ばれた機能を CMake 変数に流すだけにする。
- **ドメイン**: `graph_layout` / `package_model` / `package_viz` は `observability`。
  `cmake/packages/` と `packages/` は `foundation` (ビルド定義)。 どちらも同じ PR で
  `spec/domains/*.domain.json` と `.anatomia/layers.json` を更新する
  (`graph_layout` は shared 層、 `package_model` は infrastructure 層、 `package_viz` は infrastructure 層。
  可視化は既存の `render_viz` と同じ層にそろえる)。
- **`tools/ergo` (Web) との関係**: 段階 1〜5 では触らない。 `tools/ergo` の npm 依存 (Lapilli) は
  観測の図 P3 に出す対象になるだけ。 Web 側で版一覧を出したくなったら、 resolved.json を読むだけの
  組み込みプラグインを後で足す (§7 U6)。
- `module_list.md` / `module_list.yaml` に 3 モジュールを足す (`tool_plugin: null`)。

### 4.8 既存ホストの移行 (依頼 8)

**3 つのモード**:

| モード | ホストの状態 | ハブの動き |
|---|---|---|
| M0 互換 (既定) | `ERGO_PACKAGES` 未設定。 今までどおりホストが Pictor などを自分で取り込む | **パッケージの取り込みはしない。** 既にあるターゲットを観測して resolved.json に `host-unmanaged` として記録するだけ。 検査は PK-12 相当の「記録」だけで、 構成は止めない。 今のホストのビルド結果は変わらない |
| M1 照合 | `ERGO_PACKAGES` を設定し、 取り込みはまだホストがしている | ホスト供給の経路で受け、 SHA を固定値と照合する (PK-3) |
| M2 委譲 | ホストが自前の `add_subdirectory(Pictor)` や FetchContent を消す | ハブがローカル・ソースまたは取得で取り込む |

**ホストの型ごとの手順**:

- **兄弟ディレクトリを `add_subdirectory` するホスト (AdventureCube 型)**
  1. M1: `set(ERGO_PACKAGES pictor)` を足す。 `ADVC_PICTOR_DIR` は残す。 照合の警告を見て、
     Ergo のマニフェストの Pictor の SHA と手元の Pictor をそろえる。
  2. M2: `add_subdirectory(${ADVC_PICTOR_DIR} ...)` と Pictor のキャッシュ変数の FORCE を消す。
     代わりに `ERGO_PACKAGE_SOURCE_DIR_PICTOR=${ADVC_PICTOR_DIR}` を渡す。 Pictor のデモや
     テストの OFF は、 マニフェストの `cmake.cache` に移る。
- **FetchContent で Pictor と Ergo の SHA を別々に固定し、 自前で SHA を検証するホスト (非公開ホスト)**
  1. M1: ホストの Pictor の SHA が Ergo のマニフェストと一致することをハブが照合する。
     食い違いがあれば、 どちらに寄せるかを先に決める (Ergo のマニフェストを更新する PR、 またはホストの更新)。
  2. M2: ホストは **Ergo の SHA だけ** を固定する。 Pictor の FetchContent と自前の SHA 検証関数を消し、
     `ERGO_PACKAGES_FETCH=ON` と `ERGO_PACKAGES_STRICT=ON` にする。 SHA の検証はハブの PK-3 が引き継ぐ。
     モバイルの Vulkan の結線 (ホスト側の cmake) は Pictor より前に走る必要があるので、 ハブに入れずホストに残す。
- **Ergo の一部モジュールだけを使い、 将来 WebAssembly 化するホスト (Ludellus-Native 型)**
  1. 今は Pictor を取り込んでいないので M0 のままでよい (何も変わらない)。
  2. 描画を Pictor 経由にする段階で、 最初から M2 (`ERGO_PACKAGES=pictor` +
     `ERGO_PACKAGE_FEATURES_PICTOR=+webgpu` など) で入る。 Emscripten での構成確認はその段階で行う。

**互換期間**: M0 は少なくとも、 既知のホストが M2 に移り終わるまで残す。 終わった後に既定を
「M0 で警告を出す」へ変える。 M0 自体を消すかどうかは人間の判断 (§7 N3)。

**固定を上げる流れ**: Pictor の新しい SHA を使いたいときは、 Ergo のマニフェストの `rev` を上げる
PR を 1 本出す。 その PR では、 CI の STRICT 構成と P1 の図の差分 (版の札) を確認材料にする。

---

## 5. 段階計画と受入条件 (依頼 9)

どの段階も単独で main に入れられる。 前の段階が入っていなくても、 既存のホストは壊れない
(互換モードが既定のため)。

| 段階 | 内容 | 自動テストで確かめること | 実機・実ホストで確かめること |
|---|---|---|---|
| **S0** 仕様 | `spec/setup/packages.md` (ホスト向け)、 マニフェストの schema、 本設計の統合版 | — | — |
| **S1** マニフェストと規則 (報告のみ) | `packages/ludiars.packages.json`、 `ErgoPackageRules.cmake`、 `ErgoPackageReport.cmake`、 `cmake_minimum_required` を 3.20 へ。 M0 の記録だけをする | `cmake -P` の規則テスト: PK-1/4/6/7/8/9/10/11 の真と偽の両方。 循環の検出。 機能の足し引き。 resolved.json が決定的であること | Ergo 単体の素の構成で今と同じターゲットができること (CI)。 既存ホスト 1 つを M0 で構成して結果が変わらないこと (**未確認として残す。 S1 の PR で確かめる**) |
| **S2** 取り込み | `ErgoPackages.cmake` (4 経路) と M1 / M2。 取得は `ErgoDependencies.cmake` を再利用 | `tests/packages/fixtures/` の偽パッケージ 2〜3 個 (git の初期化をテストの中で行う) を使った構成テスト。 4 経路の各々、 PK-2/3/5、 STRICT の切り替え、 ホスト追加マニフェストの衝突 | Pictor の実ソースを `ERGO_PACKAGE_SOURCE_DIR_PICTOR` で取り込み、 `ergo_render` の実描画経路が今と同じ判定になること (デスクトップのみ。 Android / iOS / Emscripten は **未確認**) |
| **S3** 可視化 (P1 / P2) | `ergo_graph_layout`、 `ergo_package_model`、 `ergo_package_viz`、 CLI、 File API の問い合わせ | 配置の決定性、 縦横の折れ線、 長さ 0 の区間が無いこと、 推移簡約、 焦点と畳み込み、 分割と索引、 上限のぎりぎりと超過、 `validate_tela_graph` を通ること。 File API 返答の小さな見本 (テストデータとして同梱) からの読み取り。 PK-12 の検出 | Ergo 単体とホスト 1 つで `.tela` を書き、 `tela_view` で開けること (**画面での見え方は手元確認。 自動テストではない**) |
| **S4** 観測 (P3) | `scan` (package.json / .gitmodules / `ported_from`)、 MUSA の図 | 見本ディレクトリからの辺の抽出、 根拠の付与、 根拠の無い辺を作らないこと、 範囲外を読まないこと | 実際の Clio / Thaleia / Terpsichore / Lapilli のチェックアウトで図が出ること (手元確認) |
| **S5** ホスト移行 | 各ホストのリポで M1 → M2 の PR (ホストごとに 1 PR、 Ergo 側の変更は無し) | ホスト側の CI | 各ホストの実ビルドと起動。 モバイルと Emscripten はそのホストの段階で確認する。 **確認できるまでは未確認と書く** |
| **S6** 互換終了 | 既定を「M0 で警告」に変える | 互換モードの警告テスト | — |

受入条件 (統合設計で契約の書式に落とす前の素案):

- C-1 `ergo_packages_resolve()`: 選んだパッケージが 4 経路のどれでも解決しないとき、 構成は FATAL_ERROR で止まり、 エラー文に取り込み方が書かれている。
- C-2 `ergo_packages_check_rev()`: STRICT のとき、 ソースの HEAD が固定 SHA と違う構成は止まる。 非 STRICT のときは `findings` に記録される。
- C-3 `ergo_packages_check_requires()`: 循環と未充足の要求を検出する。
- C-4 `ergo_packages_check_features()`: プラットフォームで成り立たない機能と、 衝突する機能の組合せを拒む。
- C-5 `ERGO_PACKAGES` 未設定のとき: 既存の構成で作られるターゲットの集合と、 その compile definitions が変わらない。
- C-6 `ergo::package_viz::layout_*()`: 同じ入力から同じバイト列の `.tela` ができ、 どの出力も `validate_tela_graph` を通る。
- C-7 `ergo::package_viz` の上限処理: 上限を超える入力では、 分割したファイルと索引図を書き、 1 枚で上限を超えるファイルを書かない。
- C-8 `ergo::package_viz::scan()`: 根拠の無い辺を作らず、 指定した範囲の外を読まない。

---

## 6. リスク

| リスク | 影響 | 対策 (推奨) |
|---|---|---|
| R1 キャッシュ変数の奪い合い。 ホストとハブが同じ `PICTOR_*` を FORCE する | 構成ごとに挙動が変わる | PK-9 で検出する。 M2 ではホスト側の FORCE を消す手順にする |
| R2 Pictor が install / export を持たないため、 `find_package` の経路が Pictor では使えない | インストール済みの Pictor を使う配布形態が取れない | 当面は不要と判断する (ソースから組む運用のみ)。 必要になったら Pictor 側の判断で export を足す |
| R3 Tela の Pictor 用アダプタが `find_library` 前提 | ハブ経由の Pictor を Tela のアダプタが使えない | 最初の版ではその機能を扱わない (§4.3)。 Tela 側の改善は U2 |
| R4 File API の返答は構成と generate が終わった後にしかできない | 構成時に PK-12 (宣言漏れ) で止められない | 可視化 CLI の `--check` を CI の 1 段にする |
| R5 Pictor の多方式化で `features` の形が変わる | マニフェストの schema が変わる | `features` を「機能名 → キャッシュ変数」の薄い対応に留め、 方式の意味をハブに持たせない。 schema に版 (`schema: 1`) を持たせる |
| R6 公開リポに非公開の名前が漏れる | 情報漏洩 | 非公開のものは `ERGO_PACKAGE_MANIFEST_EXTRA` (ホスト側) にだけ書く。 図の出力 (`.tela`) はビルドディレクトリに置き、 コミットしない |
| R7 ハブの導入で Ergo 単体のビルドが重くなる | 開発体験が悪くなる | `ERGO_PACKAGES` 未設定では何も取り込まない。 可視化のモジュールは軽い C++17 だけで作る |
| R8 Emscripten で FetchContent とサブディレクトリの組合せが想定どおり動かない | WebAssembly 化の段階で詰まる | S5 で実際に確かめるまで「未確認」と明記する。 ハブ自体はプラットフォーム固有の処理を持たない |
| R9 Revisor の版の札が取れない環境 (Revisor が無い CI) | 図の札が欠ける | `git describe` に落とし、 落としたことを札に書く (「タグ由来」)。 空欄を「同じ」と見なさない (Revisor の service-version と同じ考え方) |

---

## 7. 未決事項 (推奨つき)

### 7.1 人間 (neco) にしか決められないこと

| ID | 論点 | 推奨 |
|---|---|---|
| N1 | 観測の範囲。 サービス (Thaleia など) の **稼働中の版** や **サービス間の呼び出し** まで Ergo の図に入れるか | **入れない。** Ergo の図は「ビルドとソースから機械的に取れる関係」に限る。 稼働中の版は Revisor、 呼び出し関係は Anatomia の領分。 必要なら P3 に Revisor / Anatomia の値を札として貼るに留める |
| N2 | 「Ergo を唯一の入口にする」を全ホストの **義務** にするか、 推奨に留めるか | **新規ホストは義務、 既存ホストは推奨** (互換モードで移行を待つ)。 義務にする時期は S5 の後に決める |
| N3 | 互換モード (M0) を最終的に消すか | **消さずに警告を出す。** Ergo の一部モジュールだけを使う軽いホストには M0 がちょうどよい |
| N4 | 固定する SHA に「公開済みのリリース」を要求するか (Revisor のリリースタグがある SHA だけ許す) | **STRICT では要求する、 非 STRICT では要求しない。** 出荷構成の再現性と、 開発中の速さを両立させる |
| N5 | 「MUSA」の範囲。 Ars-Musa のデスクトップアプリと、 女神名の各リポのどこまでを MUSA グループに入れるか | 女神名のリポ + Ars-Musa を 1 グループにする。 ゲームや Ergo と直接の辺が無いものも、 存在を示すために出す |

### 7.2 設計の側で決めてよいが、 統合設計で 1 本に寄せたいこと

| ID | 論点 | 推奨 |
|---|---|---|
| U1 | マニフェストの形式 (JSON / YAML / CMake) | **JSON** (CMake・C++・Node の全部が追加の依存なしで読める) |
| U2 | Tela 側で `TARGET pictor` を受ける分岐を足すか | Tela の判断。 ハブの最初の版では不要 |
| U3 | `find_package` の経路で SHA を照合できないこと | パッケージの config が SHA を公開する慣習 (`<Name>_GIT_SHA`) を LUDIARS で決めるまでは、 STRICT ではこの経路を使わない |
| U4 | 観測の深い解析 (ソースレベルの使用関係) | Anatomia の出力を読む形で後から足す。 Ergo に解析器を持たない |
| U5 | `ported_from` の版のずれ (移植元の Clio が先に進んだ) の検出 | 移植元の SHA を宣言できるようにだけしておき、 判定は後回し |
| U6 | `tools/ergo` に版一覧のプラグインを足すか | 足すなら resolved.json を読むだけの組み込みプラグインにする。 優先度は低い |
| U7 | `ergo_render_viz` の配置を `ergo_graph_layout` に寄せるか | 寄せない (描画パイプラインには固有の行構成がある)。 共通化が必要になったら別 PR |

---

## 付録: 読んだもの

- Ergo: `CLAUDE.md`、 `CMakeLists.txt`、 `CMakePresets.json`、 `cmake/ErgoRenderBackend.cmake`、
  `cmake/ErgoDependencies.cmake`、 `third_party/dependencies.cmake`、 `spec/setup/third-party-deps.md`、
  `spec/setup/build-cpp.md`、 `spec/domains/README.md`、 `.anatomia/layers.json`、 `module_list.yaml`、
  `spec/module/tela_graph.md`、 `spec/module/render_viz.md`、 `spec/module/resource.md`、
  `include/ergo/tela_graph/{graph,writer,tela_adapter}.h`、 `spec/feature/tool/ergo.md`、 `.gitmodules`、
  `tools/ergo/package.json`
- Tela: `CMakeLists.txt`、 `README.md`、 `spec/contracts.md` (SPEC-TL-GRAPH)
- Pictor: `CMakeLists.txt`、 `AGENTS.md`、 `cmake/PictorOptionalEffects.cmake`
- Revisor: `spec/feature/service-version.md`
- Lapilli (`package.json`、 `CLAUDE.md`)、 Clio / Thaleia / Terpsichore / Ars-Musa (README とディレクトリ構成)
- ホスト: 3 つのホストの `CMakeLists.txt` の取り込み部分 (固有名は本文で型として抽象化した)
- 並行する Pictor の多方式設計の依頼文 (§1〜4 の冒頭)
