# クイックスタート: clone から「ブラウザで値を変えて動作に反映」まで

## 目的

Ergo だけで、次の一周を最短で通す。

1. C++ 側の参照実装 `examples/compose_loop` をビルドして動かす
2. 開発者ツール `tools/ergo` を起動する
3. 実行中のプログラムの変数を `http://localhost:5170/variable/` から書き換え、
   プログラム側に反映されるのを見る
4. 性能トレースを `http://localhost:5170/profile/` で見る

ここで使う例はゲーム固有のものを含まない headless のプログラムで、
Pictor / Vulkan / ウィンドウは要らない。各手順の詳細は末尾のリンク先を参照。

## (a) 取得 (submodule 込み)

```bash
git clone --recursive https://github.com/LUDIARS/Ergo.git
cd Ergo
# --recursive を付けずに clone した場合
git submodule update --init -- lib/lapilli
```

`tools/ergo` は `lib/lapilli` (共有 CLI 起動ライブラリ `@ludiars/one-shot`) に
依存する。`src/core/registry.ts` が組み込みプラグイン (`vfx-suggest` /
`vfx-breed`) を静的に import するので、submodule が無いと `variable` /
`profile` を含むサーバ全体が起動しない。

## (b) 前提

| もの | 版 | 備考 |
|---|---|---|
| CMake | `>= 3.20` | `cmake_minimum_required(VERSION 3.20)` |
| C++ コンパイラ | C++17 | `ergo_math` / `ergo_physics2d` だけ C++20 (それらをリンクする側にも C++20 が要る)。例はどちらも使わない |
| Node.js | `>= 22.12.0` | `tools/ergo/package.json` の `engines.node` |

## (c) 例をビルドして検証する

```bash
cmake -S . -B build
cmake --build build --target ergo_example_compose_loop
build/examples/ergo_example_compose_loop          # 既定は --check
```

`[compose_loop F000120] frames=120 z=5.200 hp=83 score=2125 ...` が出て終了コード
0 なら成功。キー入力の注入 → キャラクタ移動 → ヒットストップ → 判定 → コンボ →
スコア → blackboard 経由の HUD 更新、までを 1 本のフレームループで回している
(順序の説明は [integrate-modules.md](integrate-modules.md))。

- Visual Studio などマルチコンフィグ生成器では
  `cmake --build build --config Release --target ergo_example_compose_loop` とし、
  実行ファイルは `build\examples\Release\` に出る。
- テスト込みで構成した場合 (`ERGO_BUILD_TESTS` は既定 ON) は
  `ctest --test-dir build -C Release -R example_compose_loop` でも同じ検証が走る。
  `-C` はマルチコンフィグ生成器では必須で、`--config` に渡したのと同じ構成名を指定する。
  単一コンフィグ生成器では無視される。
- 例は `ERGO_BUILD_EXAMPLES` (Ergo がトップレベルのとき既定 ON) で制御される。

## (d) 開発者ツールを起動する

```bash
cd tools/ergo
npm install
npm run dev          # ヘッドレス、既定ポート 5170。Electron で開くなら npm start
```

## (e) 実行中の値をブラウザから変える

別の端末で、リポジトリのルートから:

```bash
build/examples/ergo_example_compose_loop --connect          # 既定 127.0.0.1 5170
# ポートを変えた場合: --connect 127.0.0.1 <PORT>
```

`connected to tools/ergo` と出たら `http://localhost:5170/variable/` を開く。
`player` の下に `player.speed` (と `player.time_scale`) が見えるので、
`player.speed` を 4 から別の値にすると、例の端末に

```
[compose_loop F000275] player.speed 4.00 -> 6.50
```

のように出て、以降のフレームでキャラクタの移動速度が変わる。Ctrl+C で終了する。

## (f) 性能トレースを見る

```bash
build/examples/ergo_example_compose_loop --trace compose_loop.trace.json
```

(`--connect` と一緒に付けてもよい。終了時に書き出す。) できた
`compose_loop.trace.json` を `http://localhost:5170/profile/` にドロップすると、
フレームごとの `frame` / `gameplay` スコープと `player.z` / `score` の
カウンタがタイムラインに出る。

## (g) 自分のゲームで使う

- 取り込み: ホストの `CMakeLists.txt` で `add_subdirectory(<ergo>)` し、
  使うモジュールだけ `target_link_libraries(mygame PRIVATE ergo_input ergo_bind ...)`。
  サブプロジェクトとして取り込んだときは例はビルドされない。オプションと
  ビルドの詳細は [build-cpp.md](build-cpp.md)、Pictor / Tela など LUDIARS
  パッケージの版の固定は [packages.md](packages.md)。
- フレームループ: [integrate-modules.md](integrate-modules.md) の表の順に呼ぶ。
  `examples/compose_loop/main.cpp` をそのまま写して、スクリプト部分
  (`feed_os_events`) を自分の OS イベント処理に置き換えるのが早い。
- ライブ調整: 初期化時に `ergo::bind::Engine::instance().connect()` を 1 回、
  毎フレーム先頭で `apply_pending_writes()`。変数は `BIND_VAR(name, var)` か
  `Actor::bind_var`。リリースビルドで `ergo_bind` をリンクしなければ
  `BIND_VAR` は空展開される (`ERGO_BIND_ENABLED` は `ergo_bind` が PUBLIC で付ける)。

## うまくいかないとき

| 症状 | 原因と対処 |
|---|---|
| `npm run dev` が `ERR_MODULE_NOT_FOUND: Cannot find package '@ludiars/one-shot'` で止まる | `lib/lapilli` submodule が無い。`git submodule update --init -- lib/lapilli` の後、`npm install` をやり直す (`npm install` 自体は壊れたリンクのまま成功してしまう) |
| `npm install` が `EBADENGINE` を警告する / 起動時に構文エラー | Node が古い。`node --version` が `v22.12.0` 以上になるよう更新する |
| 例が `waiting for tools/ergo (npm run dev, same PORT)` のまま `connected` にならない | tools/ergo が起動していないか、ポートが違う。サーバの起動ログに出るポートを `--connect 127.0.0.1 <PORT>` に渡す (`PORT` 環境変数で変えている場合は特に) |
| `/variable/` に `player.*` が出ない | 例を `--connect` 付きで動かしているか確認する (`--check` は接続しない) |
| configure で `skip example compose_loop: needs ...` と出る | 例が使うモジュールを `-DERGO_BUILD_<名>=OFF` で外している。該当モジュールを ON に戻す |
| MSVC で日本語コメント周りの警告・エラー (C4819 など) | ホスト側のターゲットにも `/utf-8` を付ける ([integrate-modules.md](integrate-modules.md) 「MSVC: `/utf-8`」) |

## 関連

- 用途別の設定ガイド一覧: [README.md](README.md)
- C++ のビルド設定: [build-cpp.md](build-cpp.md)
- tools/ergo の起動モードとポート: [run-tools-ergo.md](run-tools-ergo.md)
- 全設定キー: [config-reference.md](config-reference.md)
