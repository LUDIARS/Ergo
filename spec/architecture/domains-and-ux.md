# Ergo のドメインと UX (Praeforma 登録・Anatomia ドメインの切り直し)

> **AI 案・人の承認前。** 本書の UX・コアドメイン・ビジネスドメイン・Anatomia ドメインの分け方と
> 古い処理の判定は、すべて AI がコードと仕様から起こした案である。人が承認するまで正本扱いしない。
> 削除・移動は本書では行わない (どれを消すかは人が決める)。
>
> - 起点: Actio `actio:32bd1e63-95b1-42d1-a91b-e8f8d84181cc` (2026-10-14)
> - 関連: [spec/domains/README.md](../domains/README.md)、`.anatomia/layers.json`

## 1. 結論

- Praeforma (Pf) には、Ergo を **4 つのコアドメイン (体験の責務)** と **18 のビジネスドメイン** で登録する。
  コアは「部品を組み込む」「描画と演出を整える」「動かしながら確かめて直す」「構成を把握して揃える」。
  ビジネスは複数のコアに所属してよい (開発者ツールのホストと、描画パイプラインの可視化が複数所属)。
- Anatomia のドメイン (コード上の分類) は旧 14 本を **16 本** に切り直す。
  新設は `build-packages` / `structure-visualization` / `dev-tools` / `scene-composition` / `gameplay-rules` / `legacy`。
  廃止は `combat-skill` / `ai-behavior` / `world-time` (`gameplay-rules` へ統合) と `scene-frame` (`scene-composition` へ改名、frame は `observability` へ)。
  各モジュールのテスト (`tests/<名>/`) は、モジュール本体と同じドメインに置く。
- **古い処理**は 9 件を挙げ、どれも削除はしていない。ディレクトリ単位で切り出せるもの
  (Unity Shuriken 変換・旧日付のレビュー記録・旧ドメイン定義置き場) は Anatomia の `legacy` ドメインに分けた。
  それ以外はファイル単位の名残なので、元のドメインに置いたまま本書の一覧で示す。
- Pf のコア/ビジネスは**体験の責務**、Anatomia のドメインは**コードの置き場の分類**で、別物として扱う。
  両者をつなぐのはビジネスドメインの `anatomiaDomain` だけ (§4 の表)。この対応付けは本 PR が main に入り、
  Anatomia 側に新しいドメイン名が出てから登録する。

## 2. UX

### 2.1 キャッチコピー (目指す価値 / コンセプト、80 字以内・改行なし)

> ゲームの土台を作り直さず、要る部品だけ選んで載せ、動かしながら直せる C++ フレームワーク

### 2.2 ターゲットユーザー

- **ゲーム / アプリ開発者 (主)**: C++ で自作ゲームやアプリを作る人。入力・キャラクター制御・音・描画の土台を毎回は書き直したくない。
  Ergo をリポジトリごと `add_subdirectory` で取り込み、必要なモジュールだけリンクする。
- **ツール / エンジン開発者**: Pictor (描画) や周辺パッケージと組み合わせる人。版の揃い方・リンクの形・描画パイプラインの構成を図で確かめたい。
- **テクニカルアーティスト / 調整担当**: パーティクル・UI レイアウト・変数を、ビルドし直さずに Web ツール (`tools/ergo`) から触りたい。

### 2.3 カスタマージャーニー (使われ方)

1. **出会う**: README とモジュール一覧で、用意されている部品とその範囲を知る。
2. **取り込む**: `add_subdirectory` で全体を取り込み、`ERGO_BUILD_<名>` で要らないモジュールを外す。
   Pictor・Tela などの外部パッケージは `ergo-packages.json` の宣言どおりに揃う。
3. **組み込む**: 入力・キャラクター制御・ゲームルール部品・音・描画を、自分のゲームのループへつなぐ。ゲーム固有の処理は自分のリポに残す。
4. **動かしながら直す**: `BIND_VAR` で変数を公開し、`tools/ergo` から値を変える。性能のタイムラインやログで詰まりを見つける。
5. **把握する**: パッケージの版とリンク、描画パイプラインを Tela の関係図で読み、壊れ方を先に知る。
6. **使い続ける**: 新しいモジュールやパッケージの版を取り込み、同じ手順で更新する。

### 2.4 この UX を縛る制約

| 種類 | 制約 |
| --- | --- |
| 企画 | ゲーム固有のエディタ・処理は Ergo に入れない。ホスト側のプラグインパック (`ERGO_PLUGIN_DIR`) で持つ。 |
| 企画 | 公開リポジトリなので、非公開タイトルの固有名・素材・社内向けの情報を持ち込まない。 |
| 技術 | 各モジュールは自己完結させ、上位 (ツール・利用側) を import しない。モジュール同士の横の import もしない (共有は `ergo_common` などの下層へ)。 |
| 技術 | 描画は Pictor に任せる。Ergo は Pictor を変更せず、その上で組み立てる。 |
| 技術 | 外部パッケージの版と取り込み経路は `ergo-packages.json` を唯一の宣言にする。 |

### 2.5 シナリオとユースケース

| シナリオ | 利用者 | ユースケース |
| --- | --- | --- |
| S1 ゲームに部品を組み込む | ゲーム / アプリ開発者 | UC1-1 要るモジュールだけ選んでビルドする / UC1-2 入力とキャラクター制御を自分のループにつなぐ / UC1-3 Pictor の上に描画を載せる |
| S2 動かしながら確かめて直す | 調整担当・開発者 | UC2-1 変数を公開して Web ツールから直す / UC2-2 性能タイムラインで詰まりを探す / UC2-3 遠隔テストからアプリの画面とキー入力を扱う |
| S3 構成を図で把握する | ツール / エンジン開発者 | UC3-1 パッケージの版とリンクを図で確かめる / UC3-2 描画パイプラインを図で読む |

各ユースケースの起点・成功・失敗の詳細は Pf に登録した内容を正とする。

## 3. コアドメインとビジネスドメイン (Pf)

### 3.1 コアドメイン (体験の責務)

| コア | 責務 |
| --- | --- |
| 部品を組み込む | ゲームの土台を再実装させない。要るモジュールだけを選んで載せ、ゲーム側のループへつなげる状態を保つ。 |
| 描画と演出を整える | Pictor の上で、描画の順序・パーティクル・UI を組み立て、見た目を詰められる状態を保つ。 |
| 動かしながら確かめて直す | ビルドし直さずに、実行中のアプリの値・性能・画面を観測して直せる状態を保つ。 |
| 構成を把握して揃える | 外部パッケージの版・取り込み・リンクの形と描画パイプラインを、同じ形でそろえて図で読める状態を保つ。 |

### 3.2 ビジネスドメインと所属

| ビジネス | 所属するコア | 対応する Anatomia ドメイン |
| --- | --- | --- |
| 入力 | 部品を組み込む | `input` |
| キャラクター制御 | 部品を組み込む | `actor-character` |
| ゲームルール部品 | 部品を組み込む | `gameplay-rules` |
| 2D 物理 | 部品を組み込む | `physics` |
| 音 | 部品を組み込む | `audio` |
| 資産と入出力 | 部品を組み込む | `resource-io` |
| シーンの組み立て | 部品を組み込む・描画と演出を整える | `scene-composition` |
| 描画オーケストレーション | 描画と演出を整える | `rendering` |
| パーティクルと VFX | 描画と演出を整える | `rendering` |
| UI 部品とレイアウト | 描画と演出を整える | `ui-layout` |
| ライブ変数編集 | 動かしながら確かめて直す | `observability` |
| 性能計測 | 動かしながら確かめて直す | `observability` |
| ログとフレーム計測 | 動かしながら確かめて直す | `observability` |
| 遠隔テスト連携 | 動かしながら確かめて直す | `observability` |
| 開発者ツールのホスト | 動かしながら確かめて直す・描画と演出を整える | `dev-tools` |
| パッケージ取り込み | 構成を把握して揃える | `build-packages` |
| 構成の可視化 | 構成を把握して揃える | `structure-visualization` |
| 描画パイプラインの可視化 | 構成を把握して揃える・描画と演出を整える | `structure-visualization` |

`foundation`・`vendored-third-party`・`legacy` は体験の責務を持たないので、ビジネスドメインに対応付けない。

## 4. Anatomia ドメイン (コード上の分類)

### 4.1 新しい切り方

| ドメイン | 範囲 |
| --- | --- |
| `foundation` | `common` / `math`、`template/`、リポジトリ直下の `CMakeLists.txt`、`benchmarks/`、`tests/gtest_main.cpp` など `tests/` 直下、`tests/thirdparty/`、`examples/` (モジュールを組み合わせるヘッドレスの使用例) |
| `build-packages` | `cmake/` (`packages/` と `deps/` を含む)、`ergo-packages.json`、`CMakePresets.json`、`tests/packages/`、`lib/`、`.gitmodules`、`package_manager` / `package_watch`、`tools/package_cli/`、`unity/com.ludiars.ergo.packages/`、`tests/unity_packages/`、`contracts/` (テストの契約ランタイム)、`.github/workflows/` |
| `structure-visualization` | `graph_layout` / `tela_graph` / `render_viz` / `package_model` / `package_scan` / `package_viz`、`tools/render_viz/`、`tools/package_viz/` |
| `observability` | `log` / `frame` / `profile` / `bind` / `custos` |
| `dev-tools` | `tools/ergo/` |
| `input` | `input` |
| `physics` | `physics2d` |
| `audio` | `audio` / `sound` |
| `rendering` | `render` / `particle` / `gpu_particle`、`shaders/` |
| `ui-layout` | `ui` / `ui_kit` / `ui_layout` / `vector` |
| `resource-io` | `resource` / `io` / `http` |
| `actor-character` | `actor` / `character` |
| `scene-composition` | `scene` / `cast` |
| `gameplay-rules` | `health` / `score` / `combo_counter` / `timing_judge` / `world_time` / `blackboard` |
| `vendored-third-party` | `third_party/` |
| `legacy` | `shuriken_migrator`、`tools/shuriken_migrator/` (`review/`・`.anatomia/domains/` は §5 の L4・L5 で削除済み) |

モジュール名だけの行は `src/<名>/`・`include/ergo/<名>/`・`tests/<名>/` の 3 本を指す。

### 4.2 旧 14 本との対応

| 旧 | 新 | 変更の理由 |
| --- | --- | --- |
| `foundation` | `foundation` / `build-packages` / `dev-tools` / 各モジュールのドメイン | 旧 `foundation` は `tools/`・`tests/`・`cmake/` をまとめて抱え、Anatomia で見ると何でも入る箱になっていた。パッケージ・ハブで `cmake/packages/` が大きくなったため `build-packages` に、Web ツールは `dev-tools` に、テストはモジュールのドメインへ分けた。 |
| `observability` | `observability` / `structure-visualization` | 可視化系のモジュール (`graph_layout` 〜 `package_viz`、パッケージ・ハブ S4 で入った `package_scan` を含む) は観測ではなく「構成を読む」責務で、パッケージ・ハブで増えたため独立させた。`frame` は計測なので受け入れた。 |
| `physics` | `physics` / `foundation` | `math` は物理専用ではなく、ベンチや他モジュールも使う汎用数学なので `foundation` へ移した。 |
| `rendering` | `rendering` / `legacy` | `shuriken_migrator` を `legacy` へ分けた (§5 L1)。 |
| `scene-frame` | `scene-composition` / `observability` | `frame` は計測、`scene` はシーン文書なので同居の理由が無かった。`cast` (Actor のまとまり) を `actor-character` から移し、シーンの組み立てとしてまとめた。 |
| `actor-character` | `actor-character` / `scene-composition` | `cast` を外した。 |
| `combat-skill` / `ai-behavior` / `world-time` | `gameplay-rules` | どれもエンジン非依存の小さな値と通知で、`blackboard` は AI 専用ではない共有状態だった。1〜4 モジュールずつの細切れをやめてまとめた。 |
| `input` / `audio` / `ui-layout` / `resource-io` / `vendored-third-party` | 同名 | 範囲は同じ。テストを加えた。 |

### 4.3 legacy の扱い

- `legacy` は「削除するかを人が決めるまで分けて見せる置き場」で、ここに入れても動作もビルドも変わらない。
- 人が「残す」と決めたら元のドメインへ戻し、「消す」と決めたら削除 PR で membership ごと外す。
- ファイル単位の名残 (§5 の L2・L3・L6・L7) は、パターンを除外で割ると 1 パス 1 ドメインの規則が崩れやすいので `legacy` に入れていない。本書の一覧が正本。

### 4.4 層 (`.anatomia/layers.json`)

層の考え方は変えていない。可視化系で層の宣言が抜けていた `render_viz` / `tela_graph` (`include` / `src` は infrastructure、`tests` は shared) と
`tools/render_viz` (application) を足し、`anatomia domains program --repo .` の unclassified を 5 モジュールから 0 にした。

## 5. 古い処理の一覧

| # | 対象 | 何か | 根拠 | 推奨 | 結果 (2026-10-14) |
| --- | --- | --- | --- | --- | --- |
| L1 | `src` / `include/ergo` / `tests` の `shuriken_migrator`、`tools/shuriken_migrator/` | Unity の ParticleSystem (Shuriken) を Ergo のパーティクル設定に変換するライブラリと CLI | `module_list.md` / `module_list.yaml` / README / `spec/feature/module/` のどれにも載っていない。リポ内で使うのは自分の CLI だけ (`CMakeLists.txt` の `ergo_shuriken_migrator`)。直近の機能追加は特定タイトル向け書き出しの一本化で、Unity からの移行という一回きりの用途 | **移行**: 使っているタイトルのツール側へ移す。移行が済んだら Ergo から削除 | 未着手: 移行先のリポでの作業が先 (別の委託) |
| L2 | `tools/ergo/src/plugins/vfx-suggest/ks_export.ts` | VFX 提案の結果を特定タイトルの GPU エミッタ JSON に書き出す処理 | 冒頭コメントのとおり特定タイトル専用。CLAUDE.md の「ゲーム固有のものは Ergo に入れず、ホストのプラグインパックで持つ」に反する | **移行**: ホストのプラグインパックへ。`vfx-suggest` 本体は汎用なので残す | 未着手: 移行先のプラグインパックでの作業が先 (別の委託) |
| L3 | `include/ergo/bind/json_min.h` | `ergo::common::jsonm` を `ergo::bind::jsonm` として再公開する互換シム | コメントが 2026-04-21 廃止の `ergo_inspector` との共有を前提にしている。利用は `src/bind/bind_engine.cpp` と `tests/bind/test_json_min.cpp` の 2 か所だけ | **移行**: 2 か所を `ergo/common/json_min.h` に直してシムを削除 | **済み**: `src/bind/bind_engine.cpp` は翻訳単位内の別名、テストは `ergo::common::jsonm` を直接使う。シムを削除 |
| L4 | `.anatomia/domains/render-orchestration.json` | spec/domains へ移す前の旧ドメイン定義 | Anatomia は `spec/domains` があるとそちらを優先し、この置き場を読まない。`.gitignore` は `.anatomia/*` を除外しているのに追跡が残っている。範囲は `rendering` と重なる | **削除** | **済み**: 削除して追跡から外した。`legacy` の membership からも外した |
| L5 | `review/` (2026-05-13 〜 2026-05-30、`latest.json`) | 日付別のコード規約レビュー記録 | 最終更新は 2026-05-30。現在のレビューはリポの外で別に管理する運用で、ここは更新されていない | **削除** (残したい記録はレビューの管理先へ移す) | **済み**: 削除。4 日分の記録 (2026-05-13 / 15 / 17 / 30) はリポ外のレビュー管理先に同じ日付で残っているため移す物は無かった。`legacy` の membership からも外した |
| L6 | `cmake/deps/kazmath.cmake` と CMake プリセットの kazmath | 外部の数学ライブラリの取り込み | `src/` / `include/` のどこからも参照されず、使うのは疎通テスト `tests/thirdparty/deps_smoke.cpp` だけ。数学は `ergo_math` が持つ | **要確認 → 削除**: 利用側が Ergo 経由で取っていなければ外す | **済み**: 利用側 (Ergo を取り込むゲーム・SDK のリポ) の CMake に `kazmath` / `ERGO_WITH_KAZMATH` の参照が無いことを確かめ、`cmake/deps/kazmath.cmake`・登録・プリセット・疎通テストの kazmath 分・通知を外した。curl は残す |
| L7 | `*_dummy` プラグライブラリ (`ergo_<名>_dummy`、`ERGO_BUILD_DUMMY` は当時既定 ON。現在は既定 OFF) | API を何もしない実装で差し替えるためのライブラリ | リポ内にリンクする側もテストも無い。`spec/setup/ide-projects.md` は OFF を勧めている。なお `src/audio/audio_engine_dummy.cpp` は FMOD が無いときの実行時フォールバックで別物 | **残す (要確認)**: 利用側が使っていなければ既定を OFF にする | **済み**: 利用側でリンクしている所は無く、Ergo を取り込むリポはどれも `ERGO_BUILD_DUMMY` を OFF に固定していた。既定を OFF にした (ライブラリは残す。`audio_engine_dummy.cpp` は変えていない) |
| L8 | `spec/feature/tool/acstage.md` / `placer.md` | AdventureCube 固有のツールの仕様 | 実装は AdventureCube 側にあり、`spec/feature/tool/ergo.md` も Ergo から外したと書いている | **移行**: AdventureCube のリポへ | 未着手: 移行先のリポでの作業が先 (別の委託) |
| L9 | 一覧のずれ: `module_list.md` / `.yaml` | モジュール一覧 | `ergo_ui_kit` と `ergo_shuriken_migrator` が載っていない。`ergo_ui_kit` には仕様書も無い。仕様書の列は `spec/module/<名>.md` を指すが、多くの実物は `spec/feature/module/` にある | **直す** (別 PR で一覧と仕様書の置き場をそろえる) | **済み**: `ergo_ui_kit` を一覧に足し、仕様書 `spec/module/ui_kit.md` のひな形を作った。仕様書の列を実物 (`spec/feature/module/` か `spec/module/`) にそろえた。`ergo_shuriken_migrator` は移行予定なので載せず、両方の一覧に理由を注記 |

古い処理と見えて**残す**もの:

- `ergo_custos`: 外部の Custos は Ergo への統合で役目を終える予定 (`spec/feature/tool/ergo-redesign.md`) だが、`ergo_custos` は双方向ブリッジとして拡張する側に入っている。
- `ergo_particle` (CPU) と `ergo_gpu_particle`: どちらも現役 (`particle` プラグインと GPU 経路)。
- `ergo_ui` / `ergo_ui_kit` / `ergo_ui_layout`: 層が違う (ラスタ部品 / retained-mode 部品 / データ駆動レイアウト) ので重複ではない。
- inspector 廃止の履歴 (`spec/feature/tool/ergo.md` の「廃止プラン」など): 履歴として残す。

## 6. 未決事項 (推奨つき)

1. **コア 4 本の粒度**: 「部品を組み込む」に 7 ビジネスが集まっている。推奨は今のまま。分けるなら「ゲームルールと状態」を別のコアにする。
2. **L1〜L9 の採否**: 人の判断は「推奨どおり進める」(2026-10-14)。L3〜L7・L9 は 1 本の PR で済ませた (結果は §5 の表)。L1・L2・L8 は移行先のリポでの作業が先なので別に扱う。
3. **ドメイン名の変更**: `scene-frame` → `scene-composition` などで Anatomia 側の履歴とのつながりが切れる。推奨は受け入れる (旧名が範囲を表していなかったため)。
4. **キャッチコピー**: §2.1 は AI 案。Pf のキャッチコピー欄は API で保存すると出所が「人」として残るため、欄は空のままにし、案は UX の「体験の設計」に「AI 案・人の承認前」と書いて置いた。推奨は人が案を直して欄へ写すこと (Pf は人の文言を正本にする)。
5. **`spec/feature/package-hub.md` の記述**: 「可視化は `observability`、`cmake/packages/` は `foundation`」と書いている。並行作業中の設計文書なので本 PR では触らない。推奨は次にその文書を直すときに §4 に合わせること。
