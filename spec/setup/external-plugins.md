# ゲーム固有エディタを plugin pack として読み込む設定

## 目的

ゲーム固有のエディタ (例: PrivateGame の spawn / skill、AdventureCube の
placer / terrain / acstage) を、`ergo_studio` の本体 (`tools/studio/` /
`src/studio/`) を fork せずに読み込む。**一般則**: ゲームのエディタは Ergo を
fork せず、ホストリポ側の **plugin pack** + `ERGO_PLUGIN_DIR` で拡張する
(詳細は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「外部プラグイン」)。

plugin pack は **C ABI の共有ライブラリ** (`ergo_studio_plugin.dll` / `.so` /
`.dylib`)。旧 `tools/ergo` (2026-11-06 削除) の TypeScript pack (`index.ts` の
`default` export) は読めなくなったので、C ABI の pack に作り直す。

## 設定キー

| 環境変数 | 役割 |
|---|---|
| `ERGO_PLUGIN_DIR` | plugin pack ディレクトリ。OS のパス区切り (Windows は `;`、他は `:`) で複数指定可。各ディレクトリ直下のサブディレクトリで `ergo_studio_plugin.dll` (`.so` / `.dylib`) を持つものを読み込み、`ergo_studio_plugin_v1_create` を呼んで組み込みプラグインに併合する (`src/studio/host/external_loader.cpp`) |

- `ERGO_PLUGIN_DIR` 未設定なら外部プラグインは 0 件 (組み込みのみ)。読めない
  ディレクトリは警告を出して飛ばす。`studio` プラグインの台帳で有効にした pack は
  `ERGO_PLUGIN_DIR` の後ろに足される。
- 解決順: `ERGO_PLUGIN_DIR` に並べた順 → pack の中はエントリ名の順。

### プラグイン id と壊れたエントリの扱い

規則の正本は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) の
「プラグイン id と読み込みの規則」。要点:

- id は `/^[a-z0-9][a-z0-9_-]*$/` に合い、`api` / `shell` 以外であること。`title` は
  空でないこと
- 壊れた外部エントリは **ログを出してスキップ** し、起動は止めない: ライブラリが
  読めない、エントリ関数 `ergo_studio_plugin_v1_create` が無い、ABI 版が違う、
  ファクトリが失敗した、形の違反、id の重複。読み込み失敗の OS ダイアログは出さない
- 動いてからの WS ハンドラの例外はログを出してその接続だけを 1011 で閉じる (サーバは
  止まらない)。ヘルスの例外も `ok:false` で表示するだけ
- id が重なったら先に名乗ったものが勝つ。組み込みが先、外部は `ERGO_PLUGIN_DIR` の
  順、pack の中はエントリ名の順。組み込みの id (`particle` など) は外部から上書きできない
- ファクトリ・形・重複で外したものは `/api/health` の
  `rejected: [{id, source, ok:false, err}]` に出て、全体の `ok` が false になる

## plugin pack 側の作法 (ホストリポで用意するもの)

正本は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「外部プラグイン」と
契約ヘッダ `include/ergo/studio/plugin_abi.h` (v1)。要点のみ:

- `<pack>/<id>/ergo_studio_plugin.dll` (`.so` / `.dylib`) を置く。1 サブディレクトリ = 1 プラグイン
- `plugin_abi.h` を include した C / C++ の共有ライブラリで、
  `ergo_studio_plugin_v1_create(const ergo_studio_host_v1*)` を export し、
  `ergo_studio_plugin_v1` (id / title / HTTP / WS / health / destroy) を返す
- 文字列は (data, len) の借用。応答は `release_response()` まで有効で、
  どちらの側も相手のメモリを解放しない。コールバックは任意のスレッドから並行に
  呼ばれるので、状態はプラグインが自分で守る。コールバックから境界を越えて例外を投げない
- ヘッダはビルド出力の `build/studio/sdk/include/ergo/studio/plugin_abi.h` にもある。
  例はテストの `tests/studio/fixtures/echo_pack.c`

## 手順

```bash
# ビルド (Vulkan SDK が要る。run-tools-ergo.md)
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON
cmake --build build --config Release --target ergo_studio

# 例 (PowerShell): ホストの plugin pack を読み込んで起動
$env:ERGO_PLUGIN_DIR = "E:\Document\Ars\PrivateGame\tools\game-web\plugins"
build\studio\ergo_studio.exe

# 複数 pack を渡す場合 (Windows は ; 区切り)
$env:ERGO_PLUGIN_DIR = "C:\a\plugins;C:\b\plugins"
build\studio\ergo_studio.exe --headless
```

## Studio から取得した plugin pack

`studio` プラグインの「プラグイン」画面で GitHub から取得・承認した pack は
`<studio home>/plugins/ledger.json` に記録され、取得時に CMake でビルドする。
有効なものは `ERGO_PLUGIN_DIR` と同じ扱いで次回起動時に読み込む。取得元の
許可・リビジョン固定・承認は [`../feature/tool/studio.md`](../feature/tool/studio.md)。

## 既存の plugin pack (参照)

- PrivateGame: `tools/game-web/plugins/{spawn,skill}`
- AdventureCube: `tools/ac-web/plugins/{placer,terrain,acstage}`

(いずれも Ergo リポには存在せず、各ホストリポ側。TypeScript 時代の pack は
C ABI の pack への作り直しが要る)

## 注意点

- ゲーム固有プラグインを `tools/studio/plugins/` や `builtin_plugins.cpp` に**入れない**
  (組み込みは汎用エディタのみ)。Ergo を fork せず外側で足すのが原則。
- 起動時の cwd はホスト側になり得るため、pack 内のパス解決は自分のライブラリの
  場所などを基準にし、cwd 相対にしない。
