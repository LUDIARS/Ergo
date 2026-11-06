# ergo (unified developer tool) 仕様

## 概要

Ergo の開発者向けツール (particle / variable / scene_editor / log / Studio など) を
**1 つの C++ ネイティブアプリ `ergo_studio.exe` + プラグインアーキテクチャ** に
統合したもの。ソースは `tools/studio/` (exe とプラグイン) と `src/studio/` +
`include/ergo/studio/` (ライブラリ)、テストは `tests/studio/`。

旧版 (`tools/ergo/`、Node + Hono + Electron + TypeScript) は 2026-11-06 に削除した。
ネイティブ化の設計判断は [2026-11-05-ergo-studio-native.md](../../plan/2026-11-05-ergo-studio-native.md)。

統合した理由 (旧ツール群からの経緯):

- ツールごとにビルド設定を保守するのが冗長
- ポートが増え続けるのが開発者側のノイズ (5173, 5174, ...)
- 複数エディタを 1 画面で横断的に使いたいケースがある (ビルド設定を
  切り替えずに WebSocket 経由でリアルタイム編集を並行する)

## 運用方針

- **1 プロセス / 1 ポート / 複数プラグイン**
- `ergo_studio` は **共有プラグインホスト**。ホスト + 汎用プラグインだけを持ち、
  ゲーム固有のエディタは持たない
- 各プラグインは自身の HTTP ルートと WS ハンドラを提供する
- プラグインの 2 系統:
  - **組み込み (汎用)** — `tools/studio/plugins/<dir>/` に配置し
    `tools/studio/builtin_plugins.cpp` に登録した、どのホストプロジェクトでも使うもの。
    実在する組み込みプラグインの正本は `builtin_plugins.cpp` で、一覧は下の
    「プラグイン一覧」 (ctest `test_module_catalog` が両者の一致を検査する)
  - **外部 (ゲーム固有)** — ホストリポ側の plugin pack (C ABI の共有ライブラリ) に置き、
    `ERGO_PLUGIN_DIR` 環境変数で指定したディレクトリから起動時に読み込む
    (`src/studio/host/external_loader.cpp`)。Ergo を fork せずに各ゲームが自分の
    エディタを足せる
- 新しいツールは **新規プラグインとして追加** (新規サーバは作らない)。
  汎用なら組み込み、特定ゲーム向けなら外部プラグインにする

## 起動モード

- **既定: ウィンドウ付き** (`ergo_studio.exe`)
  - GLFW + Vulkan + Pictor の `UIRenderer` / `FrameComposer` でタブ UI を描く
    (`src/studio/shell/`)。文字は `ERGO_STUDIO_FONT` (既定 `<Windows>\Fonts\meiryo.ttc`)
    を Pictor の FontLoader で 1 枚のアトラスにし、CPU のキャンバスに組んで重ねる
  - エンジンクライアント (`ergo_bind` / `ergo_particle_link` / `ergo_scene_link` /
    `ergo_log_live`) は従来通り `ws://127.0.0.1:5170/<id>/ws` に接続する (同じポート)
- **ヘッドレス: `ergo_studio.exe --headless`**、または Pictor 無し
  (`ERGO_PACKAGES` に pictor が無い) でビルドした exe
  - ウィンドウを開かずサーバのみ。CI・リモート接続などで使う

ビルド: `cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON` →
`cmake --build build --config Release --target ergo_studio` (Vulkan SDK が要る。
`ERGO_BUILD_STUDIO=OFF` で Studio 一式を外す)。出力は `build/studio/` に
`ergo_studio.exe`・`shaders/`・`sdk/include/ergo/studio/plugin_abi.h`。
起動は Excubitor の `ergo-studio` (`excubitor.catalog.yaml`、`app_kind: native`)。

## ポートと URL

既定ポート: **5170** (`PORT` 環境変数で上書き可)

| URL                                  | 役割                                            |
|--------------------------------------|------------------------------------------------|
| `http://localhost:5170/api/plugins`  | 登録プラグイン一覧                               |
| `http://localhost:5170/api/health`   | 全プラグイン集約ヘルス                           |
| `http://localhost:5170/<id>/api/*`   | プラグイン固有の REST                            |
| `ws://localhost:5170/<id>/ws`        | プラグイン固有の WebSocket                        |

UI はブラウザではなくアプリのウィンドウ (タブ) にある。HTTP / WS はエンジン・ホスト・
外部ツールとの接続口として残す。

### 接続元の制限 (クロスサイト / DNS リバインディング)

127.0.0.1 に bind していても、ブラウザは任意のサイトから
`ws://127.0.0.1:5170/...` を開かせ、プリフライト無しの POST (`text/plain` など) も
送らせる。DNS リバインディングのページは自分のホスト名のままここへ届く。そのため
サーバ (`src/studio/net/request_guard.cpp`) は次を満たさない要求を **403** で拒む:

- HTTP: `Host` が `localhost` / `*.localhost`、IP リテラル、または `ERGO_HOST` の名前で
  あること。`Origin` があるなら、そのホストが `Host` と一致すること。
  `Origin: null` はクロスサイト扱い
- WebSocket: `Origin` があるなら HTTP と同じ検査。`Origin` 無し (エンジンなどの
  ネイティブクライアント、`ergo_common_ws` は送らない) はどのホスト名で
  つないでもよい (ブラウザは WebSocket のハンドシェイクで必ず `Origin` を送る)

WebSocket の 1 メッセージは最大 **32 MiB**。超えたらその接続を 1009 で閉じる。
`ergo_log_live` の 1 バッチ (512 行 × 8 KiB、全バイトがエスケープされても約 24 MiB) と
シーン JSON はこれに収まる。

## ディレクトリ構造

```
include/ergo/studio/
├── net/        # loopback HTTP/1.1 + WebSocket サーバ、request_guard
├── host/       # IPlugin 契約、ローダ、PluginHost (ルーティング)、外部 pack
├── platform/   # WinHTTP クライアント、資格情報ストア、プロセス実行、SHA-256
├── ui/         # StudioApp / ViewBuilder / IPluginView (GPU 非依存)
├── shell/      # フォントアトラス、テキストキャンバス、UiLayer、StudioWindow (Pictor)
└── plugin_abi.h  # 外部 plugin pack の C ABI (v1)
src/studio/{net,host,platform,ui,shell}/   # 上の実装 (ergo_studio_<層> ライブラリ)
tools/studio/
├── main.cpp / studio_settings.* / builtin_plugins.*   # exe
└── plugins/    # 組み込み (汎用) プラグイン。1 プラグイン = 1 ディレクトリ
    ├── common/ # プラグイン共通の部品 (JSON・パス検査・ファイル入出力・claude CLI)
    └── <dir>/  # studio / particle / variable / rive / profile / pictor_perf /
                # render_pipeline / visus / ui_layout / vfx_suggest / vfx_breed /
                # scene_editor / log / musa_melpomene / musa_terpsichore / musa_thaleia
tests/studio/   # ctest (mini-gtest)。fake_ws.h / fake_http.h はテスト用の部品
```

ゲーム固有プラグイン (`placer` / `terrain` / `acstage` / `spawn` / `skill`
など) は **このツリーには無い**。各ホストリポの plugin pack に置き、
`ERGO_PLUGIN_DIR` でロードする (後述「外部プラグイン」)。

## Plugin I/F (C++)

`include/ergo/studio/host/plugin.h`。能力ごとにインタフェースを分け (ISP)、
プラグインは持つものだけを返す:

```cpp
class IPlugin {
public:
    virtual const PluginInfo& info() const = 0;    // id / title / description / icon
    virtual IPluginHttp*   http()   { return nullptr; }   // /<id>/*  (接頭辞を外して渡す)
    virtual IPluginSocket* socket() { return nullptr; }   // /<id>/ws
    virtual IPluginHealth* health() { return nullptr; }   // 真偽値 "ok" を持つ JSON
    virtual IPluginView*   view()   { return nullptr; }   // ウィンドウのタブ
};
using PluginFactory = std::function<std::unique_ptr<IPlugin>(const LogFn& log)>;
```

- HTTP / WS のコールバックは接続ごとのスレッドで呼ばれる。状態はプラグインが自分で守る
- `IPluginView` (`include/ergo/studio/ui/plugin_view.h`) は `view_revision()` と
  `build(ViewBuilder&)` だけを持ち、GPU に依存しない (ui_kit のノードを組むだけ)
- プラグインは環境変数を自分で読まない。設定は `tools/studio/studio_settings.cpp` が
  1 か所で読み、`builtin_plugins.cpp` がファクトリに渡す

組み込み (汎用) プラグインの追加は `tools/studio/plugins/<dir>/` を作り、
`builtin_plugins.cpp` にファクトリを 1 行足し、`CMakeLists.txt` の
`ergo_studio_plugins` にソースを足す。

## 外部プラグイン (ゲーム固有)

ゲーム固有のエディタは Ergo リポには置かず、**各ホストリポの plugin pack**
に置く。Ergo を fork せず、各ゲームが自分のエディタを持てるようにするための仕組み。

- 起動時に `ERGO_PLUGIN_DIR` 環境変数 (OS のパス区切りで複数指定可) を読み、
  指定ディレクトリ直下の各サブディレクトリの `ergo_studio_plugin.dll`
  (`.so` / `.dylib`) を読み込んで `ergo_studio_plugin_v1_create` を呼ぶ
  (`src/studio/host/external_loader.cpp`)。契約は C ABI
  `include/ergo/studio/plugin_abi.h` (v1): 文字列は (data, len) の借用、
  応答は `release_response()` まで有効、コールバックは任意のスレッドから並行に呼ばれる
- 壊れた pack エントリはログを出してスキップ — 起動は止めない (下の「プラグイン id と
  読み込みの規則」)。読み込み失敗の OS ダイアログは出さない
- **plugin pack の作り方** (ホストリポ側): `plugin_abi.h` (ビルド出力の
  `build/studio/sdk/include/` にもある) を include した C / C++ の共有ライブラリを
  `<pack>/<id>/ergo_studio_plugin.dll` として置く。例はテストの
  `tests/studio/fixtures/` の echo pack
- **取得した plugin pack**: `studio` プラグインの「プラグイン」画面で GitHub から取得・承認した
  pack は `<studio home>/plugins/ledger.json` (`tools/studio/plugins/studio/plugin_ledger.cpp`)
  に記録され、取得時に CMake でビルドする。有効なものを `ERGO_PLUGIN_DIR` と同じ扱いで
  次回起動時に読み込む。取得元の許可・リビジョン固定・承認は [studio.md](studio.md)。

### プラグイン id と読み込みの規則

組み込み・外部の両方に同じ規則を当てる (`src/studio/host/plugin_loader.cpp`)。

- **id**: `/^[a-z0-9][a-z0-9_-]*$/` に合うこと (URL の 1 セグメントとルートの接頭辞に
  なる)。`api` と `shell` はサーバ自身のルートなので使えない
- **形**: `title` が空でないこと
- **壊れたプラグインの扱い**: 組み込みが壊れていたら起動を止める (このリポのバグ)。
  外部は、ファクトリが例外を投げた・形が違う・id が重複した、のどれでもログを出して
  スキップする。ライブラリが読めない・エントリ関数が無い・ABI 版が違うものもスキップする
- **実行中の失敗**: WS ハンドラが例外を投げたらログを出してその接続だけを 1011 で閉じ、
  サーバは動き続ける。ヘルスの例外も `ok:false` で表示するだけ
- **id の重複**: 先に名乗ったものが勝つ。組み込みが外部より先で、外部は
  `ERGO_PLUGIN_DIR` に並べた順、pack の中ではエントリ名の順
- **`/api/health`**: ファクトリ・形・重複で外したものを
  `rejected: [{id, source, ok:false, err}]` に並べ、そのときは全体の `ok` も false。
  ヘルスが真偽値の `ok` を返さないプラグインは `ok:false` と `err` 付きで表示する

組み込みプラグインを足すときは、`tests/studio/test_studio_plugins.cpp` などで
`GET /<id>/api/health` が 200 で真偽値の `ok` を返すことを検査する。

## プラグイン一覧

`tools/studio/builtin_plugins.cpp` に登録された組み込みプラグインの全部
(16 個)。「供給元」はプラグインにデータを送る側で、次の 4 種類:

- **リポ内クライアント**: Ergo のターゲットがエンジン側から WS で接続する
- **ホスト提供**: ホスト (ゲーム・Pictor を回すプログラム) が自分で送る / 自分で HTTP を出す。Ergo にクライアントは無い
- **ファイル**: 環境変数やホームディレクトリで指したファイル
- **アプリのみ**: サーバは状態を持たない (必要なら LLM を呼ぶだけ)

どのプラグインも `GET /<id>/api/health` を持つ。

<!-- plugin-matrix:begin -->
| id | C++ モジュール | 供給元 | ワイヤ契約と主なエンドポイント | 仕様 |
|---|---|---|---|---|
| `studio` | — | LUDIARS の各サービス (Cc / Actio / Br / Curare / Revisor / Excubitor) を Ergo Studio のプロセスから呼ぶ。接続先は Excubitor catalog | REST `/studio/api/{projects,tasks,runs,templates,sources,resources,plugins,connections,endpoints,status}`。変更系は loopback の Host・同一 Origin・JSON のみ。WS なし | [studio.md](studio.md) |
| `particle` | `ergo_particle` | リポ内クライアント: `ergo_particle_link` (受信のみ、`include/ergo/particle/editor_link.h`) | WS `/particle/ws`: UI が `set` / `replace` を送り、全接続へ `state` を配信。REST `GET` / `POST /particle/api/effect` | 本書「`particle`」、[particle.md](../module/particle.md) |
| `variable` | `ergo_bind` (`ergo_actor` の変数も) | リポ内クライアント: `ergo::bind::Engine::connect` (既定 `127.0.0.1:5170`、`/variable/ws`) | WS `/variable/ws`: `hello` でロール (`engine` / `ui`) を名乗り、登録と値を中継。REST `GET /variable/api/vars`・`/api/actors` | 本書「`variable`」、[bind.md](../module/bind.md) |
| `rive` | — | アプリのみ | WS `/rive/ws`: `publish` (メタ情報) を全接続へ `meta` で配信。REST `GET /rive/api/meta` | [rive.md](rive.md) |
| `profile` | `ergo_profile` | ファイル (`ergo_profile` の Chrome Trace JSON)、またはホスト提供の WS `{op:"publish", trace}` | WS `/profile/ws`: `publish` を全接続へ `trace` で配信 | 本書「`profile`」、[profile.md](../module/profile.md) |
| `pictor_perf` | — (Pictor の `PerfQueryAPI`) | ホスト提供: WS `{op:"publish", snapshot}` か `POST /pictor_perf/api/publish` | WS `/pictor_perf/ws` へ `snapshot` を配信。REST `GET /pictor_perf/api/snapshot` | [pictor_perf.md](pictor_perf.md) |
| `render_pipeline` | — (Pictor のプロファイル) | ファイル: `ERGO_PICTOR_PROFILE_DIR` の `*.profile.json`。GPU 時間はホスト提供の WS `{op:"timing"}` | REST `GET /render_pipeline/api/profiles`・`/api/profile/:file`、`POST /api/profile`。WS: 保存で `profiles-changed`、`timing` を中継 | [render_pipeline.md](render_pipeline.md) / [render_pipeline_system_b.md](render_pipeline_system_b.md) |
| `visus` | — (Pictor の描画定義) | ファイル: `VISUS_PROJECT_ROOTS` 配下の `*.visus.json` | REST `GET /visus/api/roots`・`/api/scan`・`/api/load`、`POST /api/save` (ルートの外は拒否)。WS: 保存を通知 | 本書「`visus`」 |
| `ui_layout` | `ergo_ui_layout` | ホスト提供: ホストが `POST /ui_layout/patch` と `GET /ui_layout/state` を自分で出す (既定 `http://127.0.0.1:8080`、`POST /ui_layout/api/bridge/config` で変更)。`ergo_custos` はこの 2 つを提供しない | REST `/ui_layout/api/bridge/{config,patch,pull}`・`/api/file/{open,save,svg}`。WS `/ui_layout/ws` へ `patch` / `document` を配信 | 本書「`ui_layout`」、[ui_layout.md](../module/ui_layout.md) |
| `vfx-suggest` | `ergo_particle` (設定の形) | アプリのみ + 任意で LLM (`ERGO_VFX_BACKEND`) | REST `GET /vfx-suggest/api/games`・`/api/catalog`、`POST /api/generate`・`/api/ks-emitter` (`ks-emitter` は legacy の L2)。WS なし | 本書「`vfx-suggest`」 |
| `vfx-breed` | `ergo_particle` (設定の形) | アプリのみ + 任意で LLM (方向指示) | REST `POST /vfx-breed/api/generation`、`GET /api/seeds`・`/api/genes`。WS なし | 本書「`vfx-breed`」 |
| `scene_editor` | `ergo_scene` | リポ内クライアント: `ergo_scene_link` (`include/ergo/scene/editor_link.h`) | WS `/scene_editor/ws`: エンジンが `{op:"scene", id, json}` を送り、UI の `{op:"patch", scene_id, actor_id, field, value}` をそのシーンの持ち主へ転送。REST `GET /scene_editor/api/scenes` | [scene_editor.md](scene_editor.md) |
| `log` | `ergo_log` | リポ内クライアント: `ergo_log_live` (`include/ergo/log/live_sink.h`) | WS `/log/ws`: エンジンが `hello` と `lines` を送り、UI は接続時に `backlog`、以後 `lines`。REST `GET /log/api/apps`・`/api/lines` | [log.md](log.md) |
| `musa_melpomene` | — (Ars-Musa Melpomene) | GitHub の REST (Issue / PR / レビュー / Actions)。設定は `<home>/.ars/melpomene/settings.json`、トークンは OS の資格情報ストア | REST `/musa_melpomene/api/{config,issues,issue,issue/update,issue/close,comments,issues/by-scene,issues/by-object,issues/near,cache-stats,notifications,pulls,reviews,runs,milestones,user}`。トークンは書き込みのみ。WS なし | 本書「`musa_melpomene`」 |
| `musa_terpsichore` | — (Ars-Musa Terpsichore) | ファイル: `<home>/.ars/terpsichore/config.json`。コマンドサーバを Studio の中で起動・停止する | REST `/musa_terpsichore/api/{config,status,start,stop}`。WS なし | 本書「`musa_terpsichore`」 |
| `musa_thaleia` | — (Ars-Musa Thaleia) | ファイル: `<home>/.ars/spec` 配下の Markdown | REST `/musa_thaleia/api/{tree,document,render,categories,search,script,rescan}` (render は HTML でなくブロック + 目次の JSON)。WS なし | 本書「`musa_thaleia`」 |
<!-- plugin-matrix:end -->

ポートを変えた場合 (`PORT`) は、エンジン側クライアントの `connect` にも同じポートを渡す。

## 現行プラグイン (組み込み・汎用)

### `studio`
- デスクトップでゲームを AI と作る画面。LUDIARS の各サービス (Cc / Actio / Br / Curare / Revisor /
  Excubitor) と Ergo Studio のプロセスから話す。仕様は [studio.md](studio.md)

### `particle`
- 旧 `tools/particle-editor/` の移植
- 単一 `ParticleEffectConfig` を保持し、すべての WS 接続に `state` を配信
- UI ↔ engine client (`ergo_particle_link`。`ergo_particle` 本体はソケットを持たない)
- 設定の形と値域は `particle_schema.{h,cpp}` の 1 表で、`vfx-suggest` の sanitize と
  `vfx-breed` の遺伝子が共有する

### `variable`
- 旧 `tools/variable-editor/` の移植
- `BIND_VAR()` 登録を受ける engine ↔ UI ハブ
- `hello` メッセージでロール (`engine` / `ui`) を識別

### `rive`
- `.riv` のメタ情報 (アートボード / state machine / アニメーション) を中継する。
  サーバは開いているファイルのメタ情報を全接続へ配るだけ。
  仕様は [rive.md](rive.md)

### `profile`
- `ergo_profile` が書いた Chrome Trace Event JSON をタイムライン表示する
  (フレームグラフ + カウンタトラック)。WS の `publish`
  は直近のトレースを他の接続へ配るだけ。仕様は [profile.md](../module/profile.md)
  の「タイムライン UI」

### `pictor_perf`
- Pictor の `PerfQueryAPI::export_json()` の性能スナップショット (SoA のアライメント・
  帯域・バッチ適格性・GPU 時間) を表示する。ホストが WS か HTTP で publish する。
  仕様は [pictor_perf.md](pictor_perf.md)

### `render_pipeline`
- Pictor の `*.profile.json` (`ERGO_PICTOR_PROFILE_DIR`) を単一のノードグラフで表示・編集し、
  保存する。仕様は [render_pipeline.md](render_pipeline.md) と
  [render_pipeline_system_b.md](render_pipeline_system_b.md)、ディレクトリの指定は
  [plugin-data-roots.md](../../setup/plugin-data-roots.md)

### `visus`
- 各プロジェクトの `assets/visus/*.visus.json` (Pictor の描画定義) を一覧・編集する。
  ルートは `VISUS_PROJECT_ROOTS` (`;` 区切り、未設定なら cwd)。全 API は
  `plugins/common/safe_path.h` の `resolve_within` でルートの外へ出るパスを拒む。`geometry.kind ==
  "custom"` はフォームで、それ以外は生 JSON で編集する

### `ui_layout`
- `ergo_ui_layout` の `*.uilayout.json` を Figma 風に編集する (キャンバス・階層・
  プロパティ・ファイル入出力。ファイルはツールの cwd の外へ出られない)
- 実行中のゲームとの往復は **ホストが出す HTTP** を経由する: 編集は
  `POST /ui_layout/api/bridge/patch` → ホストの `/ui_layout/patch`、取り込みは
  `POST /ui_layout/api/bridge/pull` → ホストの `/ui_layout/state`。既定の転送先は
  `http://127.0.0.1:8080` で、`POST /ui_layout/api/bridge/config` で変える。
  `ergo_custos` のブリッジは `/health` / `/screenshot` / `/key` だけで、この 2 つは
  提供しない。仕様は [ui_layout.md](../module/ui_layout.md)

### `vfx-suggest`
- ゲームとシーンの文脈から LLM で `ParticleEffectConfig` のプリセットを作る
  (`POST /vfx-suggest/api/generate`)。結果は `particle_schema` の値域で丸め (`vfx_sanitize.cpp`)、
  プレビューは `particle` プラグインの WS へ送る。LLM のバックエンドは
  `ERGO_VFX_BACKEND` (`cli` のみ。`claude` CLI を `CLAUDE_BIN` で呼ぶ) と `ERGO_VFX_MODEL`
  ([config-reference.md](../../setup/config-reference.md))
- `ks_export.cpp` (`POST /vfx-suggest/api/ks-emitter`) は特定タイトル向けの書き出しで、
  ホストの plugin pack へ移す予定の legacy (`spec/architecture/domains-and-ux.md` §5 の L2)

### `vfx-breed`
- エフェクトを「選んで育てる」対話的進化。 変異体を並べ、 人間が選んだ個体を親に
  次世代を作って `ParticleEffectConfig` を詰める。 適応度は人間の目そのもので、
  自動の品質指標・画像類似度は持たない。 `vfx-suggest` (文章 → LLM → 設定) と並ぶ
  もう 1 つの作り方で、 両者は排他ではない (vfx-suggest の結果を起点にできる)
- 配置: `tools/studio/plugins/vfx_breed/` (id は `vfx-breed`)。 schema は `particle_schema` の
  `ParticleEffectConfig` をそのまま使い、 数値の値域の 1 表を vfx-suggest の sanitize と
  共有する (重複実装しない)
- 遺伝子 (`breed_genes.cpp` / `breed_genome.cpp`): 数値項目 (emission / initial / overLife /
  forces) を値域で 0..1 に正規化したもの。 `emission.rate` / `maxAlive` は対数尺度、
  発射方向と色相は環状 (0 と 1 が同じ)。 色 3 つ (`initial.color` /
  `overLife.colorStart` / `colorEnd`) は HSV + alpha の 4 遺伝子ずつ。
  `render.blend` / `render.shape` は離散遺伝子。 `positionShape` / `svgSource` /
  `svgScale` は遺伝子にせず親から引き継ぐ。 戻すときは値域に丸め、
  `speedMin ≤ speedMax`・`lifetimeMin ≤ lifetimeMax` を保ち、 `maxAlive` は整数にする
- 世代 (`breed_ops.cpp`): 既定 8 体 (4〜16)。 親が 1 体なら変異だけ、 2 体以上なら
  子ごとに相異なる 2 親を選んで一様交叉 → 変異。 変異 (`breed_ops.cpp`) は正規化空間で
  `strength × (bias + N(0,1))`、 離散遺伝子は `min(0.25, 0.5 × strength)` の確率で
  反転。 乱数は seed 付きの決定的 RNG (mulberry32、`breed_rng.cpp`) で、 同じ入力と seed なら同じ子。
  seed を省くとサーバが決めて応答に載せる
- 方向指示 (`breed_direction.cpp`): 自由文 (例「もっと鋭く短く」) が付いた
  ときだけ、 LLM に「どの遺伝子をどちらへ動かすか」 の重み (-1..1) だけを返させ、
  変異の平均をずらす。 LLM は値そのものを作らない。 バックエンドは vfx-suggest と同じ
  `ERGO_VFX_BACKEND` (`cli`) / `ERGO_VFX_MODEL`。 LLM が無い・失敗したときは
  `direction.status` が `unavailable` / `failed` になるだけで、 進化はそのまま動く
- API (サーバは状態を持たない。 履歴は UI 側):
  - `POST /vfx-breed/api/generation` — `{ parents, selected?, direction?, strength?,
    seed?, count?, generation? }` → `{ ok, children: [{ config, parents }], seed,
    generation, direction: { status, weights?, reason? } }`。 `selected` は parents の
    添字 (省略で全部)、 親が無ければ既定エフェクトが親
  - `GET /vfx-breed/api/seeds` — 起点 (既定エフェクト + vfx-suggest のカタログ)
  - `GET /vfx-breed/api/genes` — 遺伝子表 (id / 値域 / 尺度)
  - `GET /vfx-breed/api/health` — 方向指示 (LLM) の状態
- UI: 起点 (既定 / カタログ / particle エディタの現在値 / JSON 貼り付け) →
  世代のグリッド、 複数選択、 方向の自由文、 変異の
  強さ、 体数、 seed、 次世代ボタン、 世代履歴 (戻れる。 古い世代から育てると以降の
  履歴は置き換わる)。 各個体は particle プラグインの WS (`replace`) へ送ってエンジン
  側で即プレビュー、 JSON 書き出し、 vfx-suggest の `ks-emitter` での書き出しができる
- テスト: `tests/studio/test_studio_vfx_breed.cpp`

### `scene_editor`
- `ergo_scene` のシーンをツリー表示し、アクターのフィールドを編集する。エンジン
  (`ergo_scene_link`) が publish したシーンだけを見せ、UI のパッチはそのシーンを
  publish したエンジンへ転送する。エンジンが `Scene::apply_patch` で適用して
  publish し直すので、UI は常にエンジンの状態を表示する。仕様は
  [scene_editor.md](scene_editor.md)

### `log`
- 動作中の `ergo_log` の行を表示する。エンジン (`ergo_log_live` の `LiveSink`) ごとに
  直近 10k 行のリングを持ち、切断後も残す。レベル・文字列・フレーム範囲で絞り込む。
  仕様は [log.md](log.md)

### `musa_melpomene`
- Ars-Musa Melpomene (ゲーム側の Issue 管理) を Studio に取り込んだもの。GitHub の
  Issue をシーン・オブジェクト・座標で引けるようにキャッシュし (`ticket_cache.cpp`)、
  PR のレビューと Actions の実行を通知としてまとめる (`notification_tracker.cpp`)
- 設定は `<home>/.ars/melpomene/settings.json`。GitHub トークンは設定ファイルに書かず
  OS の資格情報ストア (Windows は Credential Manager) に置き、`GET /api/config` は
  `has_token` だけを返す

### `musa_terpsichore`
- Ars-Musa Terpsichore のコマンドサーバを Studio から設定・起動・停止する。
  設定は `<home>/.ars/terpsichore/config.json`。`POST /api/start` は設定の host:port に
  bind し (起動中なら 409)、`POST /api/stop` で止める

### `musa_thaleia`
- Ars-Musa Thaleia (仕様書ビューア) を Studio に取り込んだもの。LUDIARS の
  Thaleia (トレーサビリティサービス) と区別するため `musa_` を付けた。
  `<home>/.ars/spec` 配下の Markdown をツリー・カテゴリ・全文検索で引き、描画は
  HTML ではなくブロックと目次の JSON で返す (`markdown_*.cpp`)。スクリプト名から
  対応する仕様書も引ける (`GET /api/script?name=`)

> `placer` / `terrain` / `acstage` は AdventureCube 固有のため Ergo から
> 分離し、`AdventureCube/tools/ac-web/plugins/` の plugin pack へ移設した。
> 仕様は AC リポ側を参照。


## タブ UI (ネイティブ)

ウィンドウは左のサイドバーにプラグインを並べ、選んだもののタブを右に出す
(`src/studio/ui/studio_app.cpp`)。タブの中身は `IPluginView::build()` が `ViewBuilder` で組む
(ラベル・ボタン・一覧・テキスト入力 / 秘匿入力 / 複数行入力・折り返し。キー入力と Ctrl+V の
貼り付けはフォーカス中の入力欄へ入る)。

プラグイン自身が `view()` を持たないときは、exe が `tools/studio/views/` (`ergo_studio_views`)
のビューを id で付ける。ビューはプラグインの **自分の HTTP API** をプロセス内で
ワーカースレッドから呼んで描くだけで、プラグインのクラスには依存しない (外部 pack にも効く)。

| id | タブ |
|---|---|
| `studio` | プロジェクト (ソース状態・fetch / fast-forward・AI への指示)、タスク (登録・AI に実装させる)、実行、接続 |
| `log` | アプリ一覧と行 (level / 文字列 / frame で絞り込み、1 秒ごとの追従) |
| `musa_thaleia` | 文書ツリー・全文検索・描画した文書 |
| `musa_terpsichore` | 状態・起動 / 停止・設定の編集 |
| `musa_melpomene` | Issue 一覧と詳細・コメント・作成 / クローズ・通知・設定 (トークンは伏せ字) |
| `vfx-suggest` | ゲームを選びシーンを書いて生成 (LLM)、カタログ、プリセットを `particle` へ送る (`POST /particle/api/effect`) |
| 上記以外 (外部 pack を含む) | エクスプローラ: ヘルス、既知の GET エンドポイントのボタン、任意の要求 (method / path / JSON body) と応答の JSON 表示 |
旧版のシェル拡張 API (`window.ergo.shell`) と
Electron IPC は廃止した (ブラウザの iframe が無いため)。拡張はプラグイン (組み込み
または外部 pack) として足す。

## 廃止プラン (履歴) — `inspector` プラグイン統合は実施せず

当初は `inspector` を tools/ergo プラグインとして取り込む案 (Phase 2) を
検討したが、機能調査の結果 **`ergo_inspector` は `ergo_bind` の完全な
サブセット** と判定し、2026-04-21 に **inspector モジュール自体を廃止**
した。

| 機能 | bind | (旧) inspector |
|------|------|----------------|
| 型消し変数登録 | ✅ | ✅ |
| メタデータ (min/max/step/unit/read_only) | ✅ | ✅ |
| 値変化検出 → 自動 broadcast | ✅ (apply_pending_writes 経由) | ✅ |
| Actor tree 階層 | ✅ | ❌ |
| Win32 含む全 OS で同等動作 | ✅ | ❌ (Windows は dummy) |
| WS サーバ位置 | アウトバウンド (ergo_studio) | エンジン内蔵 POSIX |

新規ライブチューニング機能は `ergo_bind` + `tools/studio/plugins/variable/`
に追加する。

## 互換性

- 旧 `tools/particle-editor/` / `tools/variable-editor/` は **削除済み**
  (2026-04-20)。旧ポート (5173 / 5174) での接続試行は到達できないため、
  ホストアプリの接続先も同時に更新する必要がある。
- `ergo_bind::Engine::connect(...)` のデフォルト引数は
  `(host, 5170, "/variable/ws")`。
- 旧 `tools/ergo/` (Node / Electron) は **削除済み** (2026-11-06)。ポート・URL・
  WS のワイヤ契約はそのまま引き継いだので、エンジン側クライアントの変更は要らない。
  TypeScript の外部 plugin pack (`index.ts`) は読めなくなったので、C ABI の pack に
  作り直す。

## テスト

ctest (`tests/studio/`、mini-gtest)。hermetic で、一時ディレクトリとポート 0 を使い、
外部のサービスやネットワークに出ない (HTTP は `fake_http.h`、WS は `fake_ws.h`)。

| 場所 | 対象 |
|---|---|
| `test_studio_net.cpp` | HTTP パーサ・WS フレーム・request_guard・サーバ |
| `test_studio_host.cpp` | id と形の検査、ルーティング、`/api/health` の `rejected` |
| `test_studio_external.cpp` | C ABI の pack 読み込み (`fixtures/echo_pack.c`)、壊れたエントリ |
| `test_studio_plugins.cpp` ほか `test_studio_<plugin>.cpp` | 組み込みプラグインごとの API |
| `test_studio_studio_api.cpp` / `test_studio_studio_domain.cpp` | `studio` の API・台帳・接続図 |
| `test_studio_ui.cpp` | StudioApp のタブとクリック (GPU 無し) |
| `test_studio_shell_text.cpp` | フォントアトラスとテキストキャンバス (`ERGO_STUDIO_TEST_FONT`) |
