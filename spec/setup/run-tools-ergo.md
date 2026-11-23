# 開発者ツール `ergo_studio` をビルドして起動するための設定

## 目的

Ergo の共有プラグインホスト型の開発者ツール、C++ ネイティブアプリ
`ergo_studio.exe` (Ergo Studio) をビルドして起動する。
1 プロセス / 1 ポートで、組み込み (汎用) プラグインをアプリのウィンドウの
タブとして表示し、エンジンクライアント (`ergo_bind` / `ergo_particle_link` /
`ergo_scene_link` / `ergo_log_live`) は同じポートの WebSocket に接続する。
全体仕様は [`../feature/tool/ergo.md`](../feature/tool/ergo.md)、ソースの説明は
[`../../tools/studio/README.md`](../../tools/studio/README.md)。初めてなら
[quickstart.md](quickstart.md) が clone から「Studio で値を変えて反映」までを通す。

旧 `tools/ergo` (Node + Hono + Electron + TypeScript、`npm run dev` / `npm start`)
は 2026-11-06 に削除した。ポート・URL・WS のワイヤ契約は同じなので、
エンジン側クライアントの接続先は変わらない。

## 前提

- C++ モジュールと同じ CMake `>= 3.20` と C++17 コンパイラ ([build-cpp.md](build-cpp.md))
- ウィンドウ付きで動かすには **Vulkan SDK** と Pictor (`-DERGO_PACKAGES=pictor`、
  [packages.md](packages.md))。Pictor 無しでも同じターゲットがヘッドレス専用の exe
  としてビルドされる
- ソースの置き場所: exe は `tools/studio/` (`main.cpp` / `studio_settings.*` /
  `builtin_plugins.cpp`)、組み込みプラグインは `tools/studio/plugins/<dir>/`、
  ライブラリは `src/studio/{net,host,platform,ui,shell}` + `include/ergo/studio/`、
  テストは `tests/studio/` (ctest、mini-gtest)

## ビルドと起動モード

```bash
# リポジトリのルートで
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON
cmake --build build --config Release --target ergo_studio
```

出力は `build/studio/ergo_studio.exe`。CMake オプション `ERGO_BUILD_STUDIO`
(既定 ON) で Studio 一式を外せる ([config-reference.md](config-reference.md))。

| コマンド | モード | 内容 |
|---|---|---|
| `build/studio/ergo_studio.exe` | **既定: ウィンドウ付き** | GLFW + Vulkan + Pictor の UIRenderer でタブ UI を描き、同じプロセスでサーバを動かす |
| `build/studio/ergo_studio.exe --headless` | ヘッドレス | ウィンドウを開かずサーバのみ。CI・リモート接続・エンジンからの接続確認に使う |
| ctest `tests/studio/` | テスト | 一時ディレクトリとポート 0 を使う hermetic なテスト。通常の ctest に含まれる ([ci.md](ci.md)) |

LUDIARS の環境では手で起動せず、Excubitor のサービス `ergo-studio`
(`excubitor.catalog.yaml`) から起動する。

## 設定キー

| 環境変数 | 既定 | 役割 |
|---|---|---|
| `PORT` | `5170` | サーバの listen ポート (1..65535。それ以外は起動しない) |
| `ERGO_HOST` | `127.0.0.1` | bind するアドレス。既定は LAN に出さない。LAN に出しても外部からは IP アドレスか `ERGO_HOST` の名前で接続する (他の名前は `Host` 検査で 403。[`../feature/tool/ergo.md`](../feature/tool/ergo.md)「接続元の制限」) |

`PORT` を変えるとエンジンクライアントの接続先 (`ws://127.0.0.1:<PORT>/<id>/ws`)
も変わる点に注意 (後述)。プラグイン固有の変数 (`ERGO_PLUGIN_DIR` / `ERGO_PICTOR_PROFILE_DIR` /
`VISUS_PROJECT_ROOTS` / `ERGO_VFX_BACKEND` など) の一覧は
[config-reference.md](config-reference.md) の「環境変数 (ergo_studio)」。
環境変数を読むのは `tools/studio/studio_settings.cpp` だけで、プラグインは自分で読まない。

## ポートと URL

既定ポート **5170**。主な URL (詳細は [`../feature/tool/ergo.md`](../feature/tool/ergo.md)
「ポートと URL」):

| URL | 役割 |
|---|---|
| `http://localhost:5170/api/plugins` | 登録プラグイン一覧 (ウィンドウのタブ) |
| `http://localhost:5170/api/health`  | 全プラグイン集約ヘルス |
| `http://localhost:5170/<id>/api/*`  | プラグイン `<id>` の REST |
| `ws://localhost:5170/<id>/ws`       | プラグイン `<id>` の WebSocket |

UI はブラウザのページではなくアプリのウィンドウ (タブ) にある。HTTP / WS は
エンジン・ホスト・外部ツールとの接続口。

## 組み込み (汎用) プラグイン

`tools/studio/builtin_plugins.cpp` に登録されたものだけが組み込み
(どのホストでも使う汎用エディタ)。一覧・対応する C++ モジュール・エンジン側の
供給元・仕様書は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) の
「プラグイン一覧」にある (正本は `builtin_plugins.cpp`。ctest `test_module_catalog` が
両者の一致を検査する)。ゲーム固有エディタ (placer / spawn / skill 等) はここには
無く、外部ロードする ([external-plugins.md](external-plugins.md))。

`render_pipeline` / `visus` / `ui_layout` はホスト側の資産 (Pictor profile /
描画定義 / UI レイアウト) を参照するため env 設定が要る。未設定ならその API は 503 を返す
([plugin-data-roots.md](plugin-data-roots.md))。

## 手順

```bash
# ビルド (Vulkan SDK が要る)
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON
cmake --build build --config Release --target ergo_studio

# 既定: ウィンドウ付き
build/studio/ergo_studio.exe

# もしくはヘッドレス (サーバのみ)
build/studio/ergo_studio.exe --headless

# ポートを変える場合 (PowerShell)
$env:PORT = "5180"; build\studio\ergo_studio.exe --headless
```

## 注意点

- `build/` は追跡外。exe のほかに `build/studio/shaders/` と、外部 plugin pack が
  include する `build/studio/sdk/include/ergo/studio/plugin_abi.h` が出る。
- 旧版のシェル拡張 API (`window.ergo.shell`) と Electron IPC は無い。拡張は
  プラグイン (組み込みまたは外部 pack) として足す。
- ウィンドウの文字は `ERGO_STUDIO_FONT` (既定 `<WINDIR>\Fonts\meiryo.ttc`) のフォントで描く。
