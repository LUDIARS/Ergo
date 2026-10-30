# パッケージ・ハブ — LUDIARS パッケージの統合リンク・管理と Tela 可視化 (統合設計)

- 日付: 2026-10-14
- 対象: Ergo (Eg)。可視化の表示は Tela (Tl)
- 依頼: `spec/tasks/2026-10-14-package-hub-design-request.md`
- 素案: `spec/plan/2026-10-14-package-hub-fable.md` (Fable)、`spec/plan/2026-10-14-package-hub-opus.md` (Opus)
- 種別: 設計 (正本)。実装・ビルド・実ホストでの確認は含まない。
- 実装状況: S1 (マニフェストと規則、記録のみ) — `spec/tasks/2026-10-14-package-hub-s1.md`、S2 (取り込み) — `spec/tasks/2026-10-14-package-hub-s2.md`、S3 (可視化 P1 / P2) — `spec/tasks/2026-10-14-package-hub-s3.md`、S4 (観測 P3) — `spec/tasks/2026-10-14-package-hub-s4.md` を実装済み。S5 以降は未着手

人間の指示 (neco 2026-10-14): 「Ergoがコンテンツの各LUDIARSのパッケージをまとめてリンク/管理するようにします。
各パッケージのバージョン、何をリンクしているか、MUSAや関連パッケージは何を使用しているかなどを可視化します。
可視化はTelaで行い、今回のパイプライン可視化もErgoでやります。」

---

## 0. 比較と採否

両案は骨格で一致した: (1) 管理するのは **ホストにリンクされる C++ / CMake パッケージ** だけで、TS・サービス・MUSA 系は
**観測して図にするだけ**、(2) 版の正本は **40 桁 commit SHA** でマニフェストがロックを兼ねる、(3) Revisor の版は固定子に
ならない (`.revisor-version` は skip-worktree、タグは major/minor のみ) ので **図の札として導出**する、(4) 取得は既存
`cmake/ErgoDependencies.cmake` を再利用、(5) リンク関係は **CMake File API codemodel v2** から集める、(6) 図は 3 種で出口は
`ergo_tela_graph`、(7) 親の解釈の「Lapilli は C++」は誤り (TS モノレポ)、MUSA 系は C++ を持たず接点は `ergo_resource` ← Clio の移植だけ。

| 論点 | Fable 案 | Opus 案 | 採用 | 理由 |
|---|---|---|---|---|
| 有効化の仕方 | パッケージごとに `ERGO_PKG_<NAME>=ON/OFF` | `ERGO_PACKAGES` (使う名前の列)。**未設定なら互換モード** | **Opus** | 既存ホストは何もしなければ今のまま動く。新方式に入ったことが 1 変数で分かる |
| 取得の既定 | 既存 `ERGO_FETCH_DEPENDENCIES` (既定 ON) に乗る | 別変数 `ERGO_PACKAGES_FETCH`、**既定 OFF** | **Opus** | LUDIARS パッケージと第三者ライブラリでは「勝手に取ってよいか」の答えが違う。暗黙に取りに行かない方針と一致 |
| ホストが先に作った `pictor` | 互換期間 WARNING「版不明」、明示フラグ `_PROVIDED` | 「ホスト供給」経路で受け、`SOURCE_DIR` の git HEAD を pin と照合 | **Opus** (照合) + Fable (`_PROVIDED` の明示は恒久に残す) | 版不明にせず照合できる |
| 兄弟チェックアウト | `ERGO_PKG_SIBLING_ROOT` の明示 opt-in (`.worktrees` は見ない) | 自動探索しない、`CMakeUserPresets.json` に書く | **Fable** (明示 opt-in) | 開発機での手数が少なく、明示なので暗黙探索にならない。公開リポに開発機のパスは書かない |
| マニフェストの場所 | リポ直下 `ergo-packages.json` | `packages/ludiars.packages.json` | **Fable** | `module_list.yaml` と並ぶ機械処理用一覧。新ディレクトリを作らない |
| マニフェストの中身 | tier / `expect_tag` / `features` (`provides` / `contract` / `reserved`) / `exports` / `required_by` | `modules` (`needs` / `optional` / `ported_from`) / `observe` | **両方の和** (§2) | Fable の feature 契約と exports、Opus の modules 宣言はどちらも機械検査と図に要る |
| 非公開パッケージ | ホスト側のマニフェストは作らない | `ERGO_PACKAGE_MANIFEST_EXTRA` (同名は構成エラー) | **Opus** | Ergo は公開リポ。非公開の名前を書けない逃げ場が要る |
| 検査 | EPK-001〜013 | PK-1〜12 | **統合して PKG-01〜16** (§4) | Fable の SHA 形式・expect_tag・CMake 版・submodule 未初期化・git 無しと、Opus の dirty・FORCE 奪い合い・同名ターゲット・宣言漏れ (File API) を合わせる |
| 警告の扱い | 開発機 WARNING、CI は STRICT で FATAL、finalize で全件列挙 | 非 STRICT でも findings として resolved.json と図に残す | **両方** | 全件列挙して止める + 通したものも図で見える |
| C++ の切り方 | `ergo_package_viz` + `_readers` | `ergo_graph_layout` (汎用配置、下層) + `ergo_package_model` + `ergo_package_viz` | **Opus** | 汎用の層別配置は下層に置く (Ergo 規約「共有は下層へ」)。読み取りは model に閉じる |
| P2 (リンク図) のグループ | Anatomia ドメインを 4 つに畳む | `.anatomia/layers.json` の層 5 つ | **Opus** | 宣言済みで畳み規則が要らない。層の依存は見たい性質そのもの |
| 上限超過 | 書かずに `--focus` / `--collapse` を提案 | 既定除外 → 推移簡約 → 焦点と畳み込み → **分割 + 索引図** | **Opus** (既定の畳みは Fable「ホストと Ergo を展開、他は畳む」) | 1 回で見られる形まで自動で落とす。どの段でも上限超過の 1 枚は書かない |
| 一括出力 | `ergo_package_graphs` カスタムターゲット | — | **Fable** | `cmake --build --target ergo_package_graphs` で 3 図 |
| Pictor の glfw 暗黙取得 | リスクとして記録、Pictor 側 opt-out を依頼 | — | **Fable** | オフライン完結を保証できない事実を隠さない |

---

## 1. 対象範囲

| 区分 | 対象 | 扱い |
|---|---|---|
| **link** (取得・版固定・検査・リンク) | Pictor (`pictor`、方式別ターゲットは multi-RHI に従う)、Tela (`Tela::Core` / `Tela::Graph`、ヘッドレスのみ)。将来の LUDIARS 製 C++ ライブラリ (Accommodatio など) | マニフェストで SHA 固定。ハブが取り込み、構成時に検査 |
| link の付属物 | パッケージ同梱のシェーダ・既定プロファイル・web-mesh 出力 | パッケージの版に従う (別に固定しない)。`exports` で名前付きパス (`PICTOR_SHADER_DIR` / `PICTOR_PROFILE_DIR`) として公開 |
| Ergo 自身 | Ergo のモジュール | ホストが Ergo の SHA を固定 (入口そのもの) |
| 第三者ライブラリ | kazmath / curl など | 既存 `third_party/dependencies.cmake` のまま。図には出す |
| **tool** (取得するがリンクしない) | Lapilli (`lib/lapilli`、`tools/ergo` が `@ludiars/one-shot` を使う) | 既存 submodule を維持。gitlink SHA を記録して図に出す |
| **observe** (読み取りのみ) | MUSA 系 (Clio / Thaleia / Terpsichore / Ars-Musa)、関連 (Curare: `ergo_resource` とタグ語彙を共有) | 版と使用関係を読んで図にする。固定も検査もしない |
| 対象外 | サービスの稼働中の版・サービス間呼び出し・LUDIARS 全リポの俯瞰 | Revisor / Anatomia / Concordia の領分 (N1) |

MUSA は「ゲームを支える外側の道具群」として観測図に 1 グループで出す。Ergo との接点は `ergo_resource` ← Clio の
**移植 (port-of)** で、図では破線にしてリンクと区別する。Tela の表示側 (tela_view / tela_overlay) は Ergo が組まない
(Tela の Windows アダプタは Pictor をファイルで要求し、`pictor` ターゲットを受け取れない)。

## 2. マニフェスト `ergo-packages.json`

置き場所: リポ直下。スキーマ説明は `spec/data/ergo-packages.md`、ホスト向け手順は `spec/setup/packages.md`。
CMake (`string(JSON)`)・C++ (`ergo_common` jsonm)・Node が同じファイルを読む。`cmake_minimum_required` を 3.16 → 3.20 に上げる
(Pictor / Tela が既に 3.20)。

```json
{
  "schema": 1,
  "packages": {
    "pictor": {
      "kind": "link", "code": "Pc",
      "repository": "https://github.com/LUDIARS/Pictor.git",
      "rev": "<40 桁 SHA>", "expect_tag": "v2.2.0",
      "platforms": ["desktop", "android", "ios", "emscripten"],
      "cmake": {
        "minimum": "3.20", "wiring": "cmake/packages/pictor.cmake",
        "targets": ["pictor"], "find_package": null,
        "cache": { "PICTOR_BUILD_DEMO": "OFF", "PICTOR_BUILD_TESTS": "OFF", "PICTOR_BUILD_TOOLS": "OFF" }
      },
      "features": {
        "vulkan": { "default": "auto", "platforms": ["desktop", "android", "ios"], "provides": ["pictor"], "contract": "PICTOR_HAS_VULKAN" },
        "web":    { "default": "auto", "platforms": ["emscripten"], "cache": { "PICTOR_BUILD_WEBGL": "ON" }, "provides": ["pictor_webgl"] },
        "webgpu": { "reserved": true }, "d3d12": { "reserved": true }, "metal": { "reserved": true }
      },
      "exports": { "PICTOR_SHADER_DIR": "shaders", "PICTOR_PROFILE_DIR": "profiles" },
      "requires": {}
    },
    "tela": {
      "kind": "link", "code": "Tl",
      "repository": "https://github.com/LUDIARS/Tela.git", "rev": "<40 桁 SHA>",
      "platforms": ["desktop"],
      "cmake": { "minimum": "3.20", "wiring": "cmake/packages/tela.cmake",
                 "targets": ["Tela::Core", "Tela::Graph"], "find_package": { "name": "Tela" },
                 "cache": { "TELA_BUILD_WINDOWS": "OFF", "TELA_BUILD_PICTOR": "OFF" } },
      "requires": {}
    },
    "lapilli": { "kind": "tool", "code": "La", "acquire": "submodule", "path": "lib/lapilli" }
  },
  "modules": {
    "ergo_render":          { "needs": ["pictor"], "need_kind": "real-path" },
    "ergo_render_viz":      { "optional": ["pictor"] },
    "ergo_tela_graph":      { "optional": ["tela"] },
    "ergo_particle":        { "optional": ["pictor"], "when": "ERGO_PARTICLE_HAS_RENDERER" },
    "ergo_resource":        { "ported_from": [{ "package": "clio", "rev": null }] }
  },
  "observe": {
    "clio":        { "code": "Cl", "path_hint": "Clio",        "group": "musa" },
    "thaleia":     { "code": "Th", "path_hint": "Thaleia",     "group": "musa" },
    "terpsichore": { "code": "Tp", "path_hint": "Terpsichore", "group": "musa" },
    "ars-musa":    { "code": "Au", "path_hint": "Ars-Musa",    "group": "musa" },
    "curare":      { "code": "Cu", "path_hint": "Curare",      "group": "related",
                     "relations": [{ "from": "ergo_resource", "kind": "shares-vocabulary" }] }
  }
}
```

規則:

- `rev` は **40 桁 SHA だけ** (ブランチ名・短縮 SHA・タグは構成エラー)。`expect_tag` は人向け注釈で、checkout にあれば
  pin がその子孫かを照合する (Revisor の版を「持つ」のではなく「照合する」)。
- `features.<f>`: `cache` (パッケージ側キャッシュ変数への写像)、`provides` (有効時に存在すべきターゲット)、`contract`
  (ターゲットが公開する compile definition)、`platforms` / `requires` / `conflicts`。`reserved` は名前だけ予約で、選ぶと構成エラー。
  Pictor の描画方式は multi-RHI (`Pictor/spec/architecture/multi-rhi.md`) の段階に合わせて `reserved` を外す。ハブは方式の可否を
  判定せず、選ばれた機能を CMake 変数に流すだけ。
- `modules`: Ergo のモジュールがどのパッケージを要るか。`needs` (無いと構成エラー。`need_kind: real-path` は今の
  `ergo_render_enforce_backend` の規則に従う)、`optional` (あれば使う)、`ported_from` (移植元。版のずれ判定は後回し、U5)。
  今 `CMakeLists.txt` に `if(TARGET pictor)` として散らばる暗黙の依存を図と検査に出す。
- `observe.<name>.path_hint` は `ERGO_PACKAGES_SIBLING_ROOT` (または CLI の `--root`) からの相対で、そこだけを読む。
- マニフェストがロックを兼ねる。別ロックファイルは作らない。構成結果は `<build>/ergo-packages.resolved.json` (追跡しない)。

### ホストの選択と上書き (すべて `add_subdirectory(<ergo>)` より前のキャッシュ変数)

| 変数 | 意味 |
|---|---|
| `ERGO_PACKAGES` | 使う link パッケージの列 (例 `pictor;tela`)。**未設定なら互換モード M0** (§6) |
| `ERGO_PACKAGE_FEATURES_<NAME>` | 機能の足し引き (`+web;-vulkan`) |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | ローカルソースを明示 (worktree・オフライン) |
| `ERGO_PACKAGES_SIBLING_ROOT` | 兄弟チェックアウト探索の明示 opt-in。`<root>/<Repo>` だけを候補にし、`.worktrees` は見ない |
| `ERGO_PACKAGE_REV_<NAME>` | SHA の一時上書き (検証用)。記録に残り、STRICT では構成エラー |
| `ERGO_PACKAGE_PROVIDED_<NAME>` | ホストが自分で作ると明示 (恒久の逃げ道。照合は行う) |
| `ERGO_PACKAGES_FETCH` | 取得を許すか。**既定 OFF** (CI プリセットだけ ON) |
| `ERGO_PACKAGES_STRICT` | 警告扱いを全部 FATAL に。CI と出荷構成で ON |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | ホスト側の追加マニフェスト (非公開パッケージ用)。同名は構成エラー |

### Revisor の版との関係

マニフェストには Revisor の版を書かない。図を作るとき、`revisor version show` が使えればその値、使えなければ
`git describe --tags --match 'v*'` の `<tag>+<距離>` を札にし、どちらから取ったかを札に書く (空欄を「同じ」と見なさない)。

## 3. 取り込みの仕組み

トップレベル `CMakeLists.txt` の第三者依存の include の直後、どのモジュールの節よりも前で
`include(cmake/packages/ErgoPackages.cmake)` を呼ぶ (モジュールは `if(TARGET pictor)` で分岐するため)。

1 パッケージの解決順 (最初に当たった経路を使い、どの経路でも §4 の同じ検査を通す):

1. **ホスト供給**: ターゲットが既にある。`SOURCE_DIR` プロパティの git HEAD を pin と照合する。
2. **ローカルソース**: `ERGO_PACKAGE_SOURCE_DIR_<NAME>`。
3. **兄弟チェックアウト**: `ERGO_PACKAGES_SIBLING_ROOT` 設定時の `<root>/<Repo>`。
4. **インストール済み**: マニフェストに `find_package` があるもの (今は Tela) だけ。SHA を照合できないので「版のみ照合」と記録し、
   STRICT では使わない (U3)。
5. **取得**: `ERGO_PACKAGES_FETCH=ON` のときだけ。既存 `_ergo_fc_declare` で `repo@rev` を取得
   (`ERGO_DEPENDENCY_CACHE_DIR` 共有。SHA pin は全履歴、タグ名は shallow: shallow clone はブランチ先端とタグしか持たず、
   先端より後ろの pin を取れない)。取得後 HEAD == pin を検証 (ホストが自前で持つ SHA 検証をハブが引き取る)。全履歴なので
   取得した checkout でも祖先関係 (drift) と PKG-08 を判定する。
6. どれにも当たらなければ **構成エラー** (ソースの指し方・取得の許し方をエラー文に書く)。

`cmake/packages/<name>.cmake` (1 パッケージ 1 ファイル) が結線する: `cache` を「ホスト未設定なら既定値」として設定
(`FORCE` しない)、feature の写像、`add_subdirectory(<src> <bin> EXCLUDE_FROM_ALL)`、`targets` / `provides` / `contract` の検証、
`exports` を親スコープへ。エンジン `ErgoPackages.cmake` は具体パッケージ名を持たない。

| 環境 | 動き |
|---|---|
| オフライン | ホスト供給・ローカルソース・兄弟だけで閉じる。既定で取得 OFF なのでネットワークに出ない |
| CI | プリセット `ci-packages` で `ERGO_PACKAGES_FETCH=ON` + `ERGO_PACKAGES_STRICT=ON`。キャッシュ共有可。pin が取得元の先端より後ろでも取れる (SHA pin は全履歴) |
| worktree | `ERGO_PACKAGE_SOURCE_DIR_PICTOR=<worktree>`。SHA 不一致は非 STRICT で警告・記録、STRICT で停止。未コミット変更は `dirty` |
| Emscripten | 同じ仕組み。`platforms` で `emscripten` を判定し、成り立たない機能を構成エラーにする。**実 wasm は未確認** (multi-RHI 待ち) |

「暗黙に取りに行かない」(Tela / Pictor の方針) との両立: ハブは自分のマニフェストに書いたものだけを、書いた SHA で、
明示的に許されたときだけ取得する。下層同士に互いを探させない。なお Pictor 自身が desktop で glfw を暗黙 FetchContent する事実は
ハブでは直せないので記録する (§7)。

## 4. 検査

構成時に判定し、`resolved.json` の `findings` に残す。`finalize` は FATAL を全件列挙してから止める。純粋な規則は
`cmake/packages/ErgoPackageRules.cmake` (リスト変数の入出力だけ) に置き、`cmake -P` で単体テストする。

| ID | 条件 | 非 STRICT | STRICT |
|---|---|---|---|
| PKG-01 | `ERGO_PACKAGES` に未知の名前 | エラー | エラー |
| PKG-02 | `rev` が 40 桁 SHA でない | エラー | エラー |
| PKG-03 | 選んだパッケージがどの経路でも解決しない | エラー | エラー |
| PKG-04 | 取得した HEAD ≠ pin | エラー | エラー |
| PKG-05 | ホスト供給・ローカル・兄弟の HEAD ≠ pin (drift: ahead / behind / diverged) | 警告 + 記録 | エラー |
| PKG-06 | `ERGO_PACKAGE_REV_<NAME>` で上書き | 警告 + 記録 | エラー |
| PKG-07 | 未コミットの変更 (`dirty`) | 記録 | エラー |
| PKG-08 | `expect_tag` が checkout にあり pin がその子孫でない | 警告 | エラー |
| PKG-09 | git が無く SHA を取れない (`drift: unknown`) | 警告 | エラー |
| PKG-10 | `targets` / `provides` / `contract` が無い (ホスト供給の宣言違反、取得後のレイアウト変更) | エラー | エラー |
| PKG-11 | `requires` 未充足 / パッケージの `cmake.minimum` が実行中 CMake より新しい | エラー | エラー |
| PKG-12 | 宣言グラフ (`requires` / feature `requires`) の循環 | エラー | エラー |
| PKG-13 | feature 組合せ不正 (`platforms` 不一致 / `conflicts` / `reserved` 選択 / 描画が要るのに描画 feature が 0) | エラー | エラー |
| PKG-14 | `modules.<m>.needs` のパッケージが無いのにモジュールが ON (`real-path` は既存 `ergo_render_enforce_backend` の規則) | エラー (M0 では今の挙動のまま) | エラー |
| PKG-15 | ホストと結線が同じキャッシュ変数を別値で FORCE / 同じターゲット名を 2 パッケージが提供 / 追加マニフェストと同名 | 警告 + 記録 (同名はエラー) | エラー |
| PKG-16 | tool の submodule が未初期化 | 警告 (C++ ビルドは止めない) | エラー |
| PKG-17 | File API の実リンクに宣言 (`needs` / `optional` / `requires`) に無いパッケージ間リンク (宣言漏れ) | 図に印 | `ergo_package_viz --check` が非 0 (CI の 1 段) |

PKG-15 のキャッシュ変数はホストと結線の値を比べ、値が同じなら記録しない。結線の値は選んだ機能の分だけを比べる (選ばれていない機能のキャッシュ既定は比べない)。

PKG-17 だけは構成時に判定しない (File API の返答は generate の後にしかできない)。

## 5. 収集と可視化

### 入力 (すべて機械的に作る。手書きの図は作らない)

1. `<build>/ergo-packages.resolved.json` (構成時にハブが `file(CONFIGURE)` で 1 回書く。同じ構成なら同じバイト列):
   パッケージごとの pin / 解決 SHA / 経路 / dirty / drift / 選んだ機能 / `project(VERSION)` / findings、モジュールごとの ON/OFF と理由
   (`ERGO_RENDER_BACKEND_REASON` などを写す)。
2. CMake File API codemodel v2: ハブが `<build>/.cmake/api/v1/query/client-ergo/query.json` を置く (CMake は問い合わせを
   構成の前に読むので、これに答えるのは次の構成)。CMake 3.27 以上では `cmake_file_api(QUERY API_VERSION 1 CODEMODEL 2)`
   も出し、同じ構成の generate が共有の問い合わせとして答える (読み手は client-ergo の返答が無ければ index の `objects` を使う)。ターゲットの `sourceDir` が
   最長一致するパッケージへ帰属させる (どれにも当たらなければ host)。既定でテスト・ダミー・ベンチ・`ergo_gtest_main` を除外
   (`--include-tests` で戻す)。`version` を検査し、違えば明示エラー。
3. 観測の使用関係 (`ergo_package_viz scan --root <dir>`): `package.json` の `dependencies` / `devDependencies`
   (`file:` / `workspace:` / `@ludiars/*`)、`.gitmodules` + gitlink SHA、マニフェストの `ported_from` / `relations`。
   **辺には根拠 (ファイルとキー) を必ず持たせ、根拠の無い辺は作らない。** `path_hint` の範囲外は読まない (総なめにしない)。
   読めないものは「未観測」ノードとして残す。

### 図 (1 図 = 1 `.tela`)

| 図 | ノード | グループ (≤ 8) | 辺 |
|---|---|---|---|
| **P1 パッケージ版一覧** `packages` | パッケージ (link・tool・第三者・observe・host・Ergo)。札: 名前 (コード) / 版の札 (`2.2.0+12`・出所付き) / SHA7 / 選んだ機能 / 印 (`★drift` `dirty` `上書き` `宣言漏れ` `未観測`) | Ergo / 描画 / 可視化 / 第三者 / 観測: MUSA / 観測: 関連 / host | `requires`・帰属で畳んだ実リンク (実線)、`ported_from` / `shares-vocabulary` (破線) |
| **P2 ターゲットのリンク図** `targets` | ターゲット | `.anatomia/layers.json` の 5 層 + 外部パッケージ + 第三者 | 直接の `dependencies` (実線)。循環は `cycle` グループへ移して見せる (構成は止めない) |
| **P3 使用関係図** `usage` | MUSA 系・関連・Lapilli・Ergo の該当モジュール・tools/ergo | MUSA / 関連 / 共有 TS / Ergo / ツール / 未観測 | submodule・`file:` 依存 (実線)、移植・語彙共有 (破線) |

上限 (8 グループ / 128 ノード / 256 辺) に収める手順 (決定的、収まった所で止める):
既定除外 → 推移簡約 (消した本数を題に添える) → 焦点と畳み込み (既定は「host と Ergo を展開、他パッケージは 1 ノードに畳む」、
`--focus <package|layer|target>` / `--expand <pkg>`) → 分割 (`out.1.tela` … と索引図 `out.index.tela`)。
上限超過の 1 枚は書かない (`ergo_tela_graph` の検査が拒む)。

配置 (`ergo_graph_layout`): 層別配置で依存の向きは左 (使う側) → 右 (使われる側)、列は最長経路、列内は重心法 2 往復の後に ID 順で固定、
辺は縦横の折れ線で列間の通路を通し長さ 0 の区間を作らない (`ergo_render_viz` と同じ性質)、ノード寸法は札の行数から、キャンバスは ±100000 内。

### 手順

```sh
cmake -S <host> -B build -DERGO_PACKAGES="pictor;tela" -DERGO_PACKAGE_SOURCE_DIR_PICTOR=../Pictor
cmake --build build --target ergo_package_graphs        # 3 図を build/graphs/ に書く
# または個別に
ergo_package_viz build --view packages --out packages.tela
ergo_package_viz build --view targets --focus ergo --out targets.tela
ergo_package_viz build --view usage --root <兄弟チェックアウトの親> --out usage.tela
tela_view --font <file.ttf> --graph packages.tela         # Tela は別インストール (Excubitor 経由で起動)
```

描画パイプライン可視化 (`ergo_render_viz --preset High --out high.tela`) と同じ「CLI で `.tela` を書いて `tela_view` で開く」1 本道。

## 6. Ergo の中での配置と既存ホストの移行

```
ergo-packages.json                        マニフェスト (正本)
cmake/packages/ErgoPackages.cmake         解決 (経路選択・add_subdirectory / find_package / 取得)。具体名を持たない
cmake/packages/ErgoPackageManifest.cmake  マニフェストの読み取り (JSON → リスト変数。S1 で分離)
cmake/packages/ErgoPackageRules.cmake     純粋な検査規則
cmake/packages/ErgoPackageReport.cmake    resolved.json の書き出しと File API query の設置
cmake/packages/ErgoPackageRulesAcquire.cmake  取り込み段の純粋な検査規則 (PKG-03/04/05/07/08/09/10/16。S2 で分離)
cmake/packages/ErgoPackageResolve.cmake   6 経路の選択と取得 (S2 で ErgoPackages.cmake から分離)
cmake/packages/ErgoPackageGit.cmake       checkout への読み取り専用の git 照会 (HEAD・祖先・dirty・タグ。S2)
cmake/packages/ErgoPackageWiring.cmake    結線ファイルが呼ぶ部品 (cache 既定値・feature 写像・add_subdirectory・検証・exports。S2)
cmake/packages/{pictor,tela}.cmake        1 パッケージ 1 ファイルの結線
include|src|tests/graph_layout/           ergo_graph_layout   汎用の層別配置 → tela_graph::Graph (下層、shared 層)
include|src|tests/package_model/          ergo_package_model  resolved.json / File API → PackageModel (読むだけ)
include|src|tests/package_scan/           ergo_package_scan   観測スキャン (兄弟チェックアウト) → UsageModel (読むだけ。S4 で package_model から分離)
include|src|tests/package_viz/            ergo_package_viz    PackageModel → P1/P2/P3 (グループ・簡約・畳み・分割)
tools/package_viz/main.cpp                CLI ergo_package_viz (ターゲット ergo_package_viz_cli)
tests/packages/                           cmake -P の規則テストと構成テスト用の偽パッケージ (git 初期化はテスト内)
spec/data/ergo-packages.md  spec/setup/packages.md  spec/module/{graph_layout,package_model,package_scan,package_viz}.md
```

- 依存の向きは `package_viz → package_model / package_scan / graph_layout → tela_graph` の一方向 (`package_scan` と
  `package_model` は互いに import しない)。`ergo_render_viz` とは互いに import しない
  (`ergo_render_viz` の配置を `ergo_graph_layout` に寄せるかは範囲外、U7)。C++ 側はどれも Pictor / Tela に依存せず常時ビルド・テスト可。
  `ergo_common::jsonm` で File API の JSON が読めるかは S3 着手前に確かめ、足りなければ読み取り専用拡張を足す (新規 third-party は入れない)。
- `ergo_render` / `cmake/ErgoRenderBackend.cmake` は変えない。変わるのは「`pictor` ターゲットを作る主体がホストからハブへ移り得る」ことだけ。
- ドメイン: `graph_layout` / `package_model` / `package_scan` / `package_viz` は `observability`、`cmake/packages/` と `ergo-packages.json` は `foundation`。
  同じ PR で `spec/domains/*.domain.json` と `.anatomia/layers.json` を更新。`module_list.md` / `.yaml` / `README.md` に行を足す。
- `tools/ergo` (Web) は触らない。版一覧の Web プラグインは resolved.json を読むだけの任意の後段 (U6)。

### 移行モード

| モード | ホストの状態 | ハブの動き |
|---|---|---|
| **M0 互換 (既定)** | `ERGO_PACKAGES` 未設定。今までどおりホストが取り込む | 取り込まない。既にあるターゲットを `host-unmanaged` として記録するだけ。ビルド結果は変わらない |
| **M1 照合** | `ERGO_PACKAGES` を設定、取り込みはまだホスト | ホスト供給経路で受け、SHA を pin と照合 (PKG-05) |
| **M2 委譲** | ホストが自前の取り込み・SHA 検証・`PICTOR_BUILD_*` の FORCE を消す | ハブがローカルソース / 兄弟 / 取得で取り込む |

- 兄弟ディレクトリを `add_subdirectory` するホスト (AdventureCube 型): M1 で `ERGO_PACKAGES=pictor` を足して照合 → M2 で
  `add_subdirectory(../Pictor)` を消し `ERGO_PACKAGE_SOURCE_DIR_PICTOR` を渡す。
- Pictor と Ergo を別々の SHA で FetchContent し自前で検証するホスト: M1 で照合し食い違いをどちらに寄せるか決める → M2 で
  Ergo の SHA だけを固定し、Pictor の取得と検証関数を消して `ERGO_PACKAGES_FETCH=ON` + `STRICT=ON`。Pictor より前に走る必要のある
  モバイルの Vulkan 結線はホストに残す。
- Ergo の一部モジュールだけを使い将来 wasm 化するホスト (Ludellus-Native 型): 今は M0 のまま。描画を Pictor 経由にする段階で最初から M2 で入る。
- Pictor の新しい SHA を使いたいときは、Ergo のマニフェストの `rev` を上げる PR を出す (STRICT 構成の CI と P1 の差分を確認材料にする)。

## 7. 段階計画と受入条件

> 2026-10-14 追記: S4 より後の段階は `spec/feature/package-manager.md` §7 (W1〜W7 を差し込んだ段階表) が正本。
> ホスト側の宣言 (`host` 節) を設けることにしたため、本書の「ホスト側の別マニフェストは作らない」は同書 §2.1 で改めた。

どの段階も単独で main に入り、M0 が既定なので既存ホストは壊れない。実機・実ホストの確認は人の許可に従い、未確認は未確認と書く。

| 段階 | 内容 | 受入 (自動テスト) | 実機・実ホスト |
|---|---|---|---|
| **S1 マニフェストと規則 (記録のみ)** | `ergo-packages.json`、`ErgoPackageRules.cmake`、`ErgoPackageReport.cmake`、CMake 3.20、M0 の記録、`spec/data` / `spec/setup` | `cmake -P`: PKG-01/02/06/11/12/13/14/15 の真偽両方、循環検出、機能の足し引き、resolved.json の決定性。`ERGO_PACKAGES` 未設定で既存構成のターゲット集合と compile definitions が変わらない (C-5) | 既存ホスト 1 つを M0 で構成して結果不変 (未確認として PR で確かめる) |
| **S2 取り込み** | `ErgoPackages.cmake` (6 経路)、`cmake/packages/{pictor,tela}.cmake`、M1 / M2、STRICT、追加マニフェスト、PKG-03/04/05/07/08/09/10/16、`ci-packages` プリセット | 偽パッケージ 2〜3 個 (テスト内で git 初期化) で各経路・drift 判定 (none/ahead/behind/diverged/unknown)・STRICT 切替・同名衝突。Emscripten を模した変数で `vulkan` 拒否 / `web` 自動 ON | Pictor 実ソースを `SOURCE_DIR` で取り込み、`ergo_render` の実経路判定が今と同じ (desktop。Android / iOS / Emscripten は未確認)。MSVC と GCC |
| **S3 可視化 P1 / P2** | `ergo_graph_layout`、`ergo_package_model` (resolved.json / File API リーダ)、`ergo_package_viz`、CLI、`ergo_package_graphs`、PKG-17 (`--check`) | 配置の決定性・縦横の折れ線・長さ 0 なし・推移簡約・焦点と畳み・分割と索引・上限ぎりぎりと超過・`validate_tela_graph` 通過、File API 見本 (同梱) からの読み取り、宣言漏れ検出。Tela がある構成では Tela 本体の `load_graph` が受理 | Ergo 単体とホスト 1 つで `.tela` を書き `tela_view` で開く (見え方は手元確認) |
| **S4 観測 P3** | `scan` (package.json / .gitmodules / `ported_from` / `relations`)、MUSA 図 | 見本ディレクトリからの辺抽出、根拠の付与、根拠の無い辺を作らない、範囲外を読まない、未観測の残し方 | 実際の Clio / Thaleia / Terpsichore / Lapilli のチェックアウトで図が出る (手元確認) |
| **S5 ホスト移行** (ホストごとに 1 PR、Ergo 外) | M1 → M2 | 各ホストの CI | 各ホストの実ビルド・起動 (ホスト側の許可に従う) |
| **S6 互換の既定変更** (S5 後) | M0 で警告を出す既定へ | 警告テスト | — |
| **S7 任意** | `revisor version show` 注釈、`tools/ergo` 版一覧プラグイン、`render_pipeline` の `PICTOR_PROFILE_DIR` 参照、Pictor 方式 feature の `reserved` 解除 (multi-RHI の段階に追随) | 各々 | — |

## 8. リスク

両案のリスク表 (Fable §6、Opus §6) を引き継ぐ。主なもの:

- キャッシュ変数の奪い合い (ホストとハブが同じ `PICTOR_*` を FORCE) → PKG-15 で検出、M2 でホスト側の FORCE を消す。
- Pictor が install / export を持たず `find_package` 経路が使えない → 当面ソースから組む運用のみ。
- Tela の Pictor アダプタが `find_library` 前提 → 最初の版ではハブで扱わない (U2)。
- Pictor の glfw 暗黙 FetchContent でオフライン完結を保証できない → Pictor に opt-out を依頼 (N6)。それまで `spec/setup/packages.md` に明記。
- multi-RHI で feature 名・オプション名が変わる → 写像は `cmake/packages/pictor.cmake` 1 ファイルに閉じ、未対応方式は `reserved`。
- リンク図のノード数が上限を超える → 既定の畳み + 分割。File API の形式変更は `version` 検査で明示エラー。
- 公開リポへの非公開名の漏れ → 非公開は `ERGO_PACKAGE_MANIFEST_EXTRA` だけに書く。`.tela` はビルドディレクトリに置きコミットしない。
- Windows のパス長 (FetchContent の深いパス) → `ERGO_DEPENDENCY_CACHE_DIR` を短いパスに置く手順を setup に書く。

## 9. 未決事項 (推奨つき)

人間 (neco) にしか決められないもの。いずれも S1〜S2 の着手は妨げない (推奨で進め、該当段階の前に確定する)。

| ID | 論点 | 推奨 |
|---|---|---|
| N1 | 観測の範囲に稼働中の版・サービス間呼び出しを入れるか | 入れない (ビルドとソースから取れる関係に限る)。必要なら Revisor / Anatomia の値を札として貼るだけ |
| N2 | Ergo を唯一の入口にすることを義務にするか | 新規ホストは義務、既存ホストは推奨 (M0 で移行を待つ)。義務化の時期は S5 の後 |
| N3 | 互換モード M0 を最終的に消すか | 消さずに警告 (一部モジュールだけ使う軽いホストに合う)。既定変更は既知ホストの M2 完了と Ergo 次 minor の遅い方 |
| N4 | STRICT で pin に「公開済みリリースのタグ」を要求するか | STRICT では要求、非 STRICT では要求しない |
| N5 | MUSA グループの範囲 | 女神名のリポ + Ars-Musa を 1 グループ。直接の辺が無いものも存在を示すために出す |
| N6 | Pictor の glfw 暗黙取得 | Pictor に opt-out を依頼する (Pictor 側 PR) |
| N7 | Pictor の pin 更新の責務 | 使いたいホストの人が Ergo の `rev` を上げる PR を出す。自動化は後 |
| N8 | observe の版の情報源 | 初版は兄弟チェックアウトを読む。Concordia / Revisor への問い合わせは S7 (ネットワークと認可が絡む) |

設計側で推奨どおり進めるもの: U1 マニフェストは JSON、U2 Tela の `TARGET pictor` 分岐は Tela の判断、U3 `find_package` 経路は STRICT で使わない
(LUDIARS で `<Name>_GIT_SHA` の慣習を決めるまで)、U4 ソースレベルの使用関係は Anatomia の出力を読む形で後から、U5 `ported_from` の版ずれ判定は後回し、
U6 `tools/ergo` プラグインは低優先、U7 `ergo_render_viz` の配置は寄せない。
