# 検証の構成とローカルでの実行

## 目的

Ergo の変更をどこでどう検証するか、手元で走らせる構成ごとのコマンドをまとめる。
main に入れるまでの運用は [branch-and-pr.md](branch-and-pr.md) 「マージのゲート」。

## 現在の検証の経路

- **GitHub Actions では Ergo のビルド・テストを走らせない。** 以前の
  `ci` / `packages-integration` ワークフロー (各ツールチェーンの ctest、`tools/ergo`、
  Unity パッケージのコア、Pictor / Tela との統合ビルド) はリポジトリの方針で廃止した。
  下の各構成は、そのとき検査していた内容を手元で回すためのコマンドとして残している。
- **マージのゲートは Revisor のローカルレビュー。** Revisor は登録されたテストケース
  `configure` / `build` / `ctest` (CMake の構成・ビルドと ctest の全テスト) を走らせ、
  あわせて Praesidium の事前検査、Anatomia の検査、漏洩検査を行う。テストケースの
  コマンドと引数は Revisor 側の登録 (運用者の Revisor) にあり、このリポジトリには無い。
- **リポジトリに残るワークフローは `.github/workflows/harness.yml` だけ。** LUDIARS 共通の
  harness ゲート (AIFormat の決定的チェック) で、GitHub PR と main への push で走る。
  **手で編集しない** (正本は AIFormat、Castra が配信する)。

## 構成の一覧

| 構成 | 環境 | 主に捕まえるもの |
|---|---|---|
| windows-msvc | Windows、Visual Studio ジェネレータ、Release、`ERGO_AUDIO_BACKEND=dummy` | 主開発環境 (Windows / MSVC) のビルド・テストの回帰 |
| linux-gcc-cmake320 | Linux、GCC、**CMake 3.20.5**、Ninja、Release | 宣言した最低版 (`CMakeLists.txt:1` の 3.20) に無い CMake 機能 (例: `file(COPY_FILE)` は 3.21)、ポリシー未設定 (`IN_LIST` の CMP0057)、libstdc++ での include 漏れ |
| linux-clang | Linux、clang、`-Wall -Wextra`、Release | clang だけが出すエラー。警告は出すだけで `-Werror` は付けない |
| linux-asan | Linux、GCC、Debug、ASan + LSan + UBSan | 範囲外アクセス、解放後使用、リーク、未定義動作 (下記「サニタイザ」) |
| macos | macOS、AppleClang、Release | macOS 固有の差 (入力 HID / ソケット / オーディオ) |
| tools-ergo | Node 22 | `tools/ergo` の型 (src + tests)、`tsc` ビルド、node:test (サーバコア・plugin pack 読み込み・パスガード・各プラグインの中継) |
| unity-package-core | .NET 8 | Unity パッケージ (`unity/com.ludiars.ergo.packages`) のコアを Unity なしで |
| packages-integration | Windows、プリセット `ci-packages` | Ergo + Pictor + Tela をマニフェストの pin で取得した統合ビルド (下記) |

- C++ の各構成は ctest の全テストを走らせる (後から足したテストも自動で入る)。
  その中の `test_module_catalog` (`tests/test_module_catalog.cmake`、コンパイラ不要) は
  `module_list.{md,yaml}`・README・[config-reference.md](config-reference.md)・
  `tools/ergo` のプラグイン一覧・`spec/domains` とコードの食い違い、壊れた相対リンクを
  検出する。単体では `cmake -DERGO_SOURCE_DIR=. -P tests/test_module_catalog.cmake`。
- CMake プリセットの名前の `ci-` は以前の GitHub Actions のレグに由来する。
  ドキュメントと手元の手順が参照しているので名前は変えていない。中身は手元の検証構成。
- Linux / macOS では ccache を使うと再ビルドが速くなる。構成の前に
  `export CMAKE_CXX_COMPILER_LAUNCHER=ccache` とする (または構成に
  `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` を足す)。Visual Studio ジェネレータは
  コンパイララウンチャを使えないので、Windows は `--parallel` (MSBuild `/m`) と
  `CL=/MP` で並列化する。

## ローカルでの実行

いずれもリポジトリのルートで実行する。

### windows-msvc

```powershell
cmake -B build -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy
$env:CL = "/MP"
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --parallel 4 --output-on-failure
```

テストの後に Ergo 自身をホストとしてパッケージマネージャの P4 ホスト図を作るなら
(dogfood):

```powershell
.\build\Release\ergo_pkg.exe status --build build --host . --ergo .
cmake --build build --config Release --target ergo_package_graphs   # build/graphs/*.tela
```

### linux-gcc-cmake320

プリセット (`CMakePresets.json` は version 3) には CMake 3.21 以上が要るので、
この構成だけ素のコマンドラインで構成する。

```bash
python3 -m venv /tmp/cmake320
/tmp/cmake320/bin/pip install cmake==3.20.5 ninja
export PATH=/tmp/cmake320/bin:$PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ \
      -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy
cmake --build build --parallel
ctest --test-dir build --parallel 4 --output-on-failure
```

### linux-clang / linux-asan (プリセット)

```bash
cmake --preset ci-linux-clang && cmake --build --preset ci-linux-clang && ctest --preset ci-linux-clang
cmake --preset ci-asan        && cmake --build --preset ci-asan        && ctest --preset ci-asan
```

- ビルドディレクトリは `build/<プリセット名>`。テストプリセットは
  失敗時の出力と 4 並列を指定済み。
- `ci-linux-gcc` (GCC + 手元の CMake、Release) も同じ形で使える。最低版の
  CMake 3.20 を確かめるときは上の素のコマンドライン側を使う。
- ASan を clang で回すなら `cmake --preset ci-asan -DCMAKE_CXX_COMPILER=clang++`。

### macos

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy
cmake --build build --parallel
ctest --test-dir build --parallel 4 --output-on-failure
```

### tools-ergo

```bash
git submodule update --init lib/lapilli      # @ludiars/one-shot (file: 依存)
cd tools/ergo
ELECTRON_SKIP_BINARY_DOWNLOAD=1 npm ci       # Electron 本体はテストに要らない
npm run typecheck && npm run build && npm test
```

`npm run typecheck` は `tsconfig.test.json` で src と tests をまとめて型検査する
(`npm run build` の `tsconfig.json` は src だけ)。テストは
`tools/ergo/tests/{core,helpers,plugins/<id>}/` に置き、一時ディレクトリと
ポート 0 だけを使う (固定ポート・ネットワーク・LLM 呼び出しなし)。

### unity-package-core

```bash
dotnet test tests/unity_packages/Ergo.Packages.Core.Tests.csproj
```

パッケージの構造と Fixtures のバイト一致は ctest の `test_unity_package_structure` が見る。

### packages-integration

Windows で、サブモジュールを初期化してから:

```powershell
git submodule update --init --recursive
cmake --preset ci-packages -DERGO_DEPENDENCY_CACHE_DIR=build/deps-cache
cmake --build --preset ci-packages --parallel
ctest --preset ci-packages
```

- `ci-packages` は `ERGO_PACKAGES=pictor;tela` をパッケージハブで取得
  (`ERGO_PACKAGES_FETCH=ON`) し、警告級の所見をすべてエラーにする
  (`ERGO_PACKAGES_STRICT=ON`)。実際の `ergo_render` バックエンドや
  `if(TARGET ...)` の下でだけ登録されるテストを含む統合構成で、
  [packages.md](packages.md) の「Ergo 単体で実際の Pictor / Tela をマニフェストの pin で
  取る構成」にあたる。ネットワークが要る。
- Windows で回す (Linux では Pictor の GLFW が wayland / X11 の開発パッケージを要求する)。
- STRICT では `lib/lapilli` 未初期化 (PKG-16) がエラーになるので、サブモジュールは
  `--recursive` で初期化しておく。
- 取得したソースは FetchContent の基底ディレクトリ (`ERGO_DEPENDENCY_CACHE_DIR`) に
  残るので、同じディレクトリを渡せば次回は取り直さない。

## サニタイザ

### ASan / LSan / UBSan (`linux-asan`)

- `ci-asan` プリセット: Debug、コンパイルとリンクの両方に
  `-fsanitize=address,undefined`。`-fno-sanitize-recover=undefined` で UB は
  その場でテストを落とす。テストプリセットが `ASAN_OPTIONS=detect_leaks=1` と
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` を設定する。
- 新しいカーネルの既定値ではサニタイザのシャドウメモリの配置が壊れることがある。
  起動直後に落ちるときは `sudo sysctl vm.mmap_rnd_bits=28` に下げてから走らせる。
- **ergo_profile の意図的な非解放**: スレッドごとのイベントバッファは
  エクスポートのためにプロセスの寿命の間残す (解放しない)。バッファを指す
  登録簿 (`src/profile/profile.cpp` の `thread_buffers()`) も破棄しない
  ヒープオブジェクトにしてあるので、LeakSanitizer からは到達可能に見え、
  リークとして報告されない。登録簿を普通のグローバル `std::vector` に戻すと
  静的破棄で先に消え、`test_profile` が LSan で落ちる。意図的に解放しない
  オブジェクトを足すときも同じく「到達可能なまま残す」形にし、
  サプレッションで隠さない。

### TSan (未整備)

ThreadSanitizer の構成はまだ無い。スレッドを立てるテスト
(`test_profile` / `test_polling_thread` / `test_double_buffer` / `test_log` /
`test_custos_module` / `test_http_server` / `test_screenshot_bridge` /
`test_particle_system` / `test_audio_engine`) を `-fsanitize=thread` で
回すと、次の 3 件の実データ競合が出る:

| モジュール | 場所 | 競合 |
|---|---|---|
| ergo_input | `include/ergo/input/double_buffer.h` の `swap()` | 新しい添字を公開してから `buffers_[newWrite] = buffers_[oldWrite]` を写すので、旧添字で読んでいる読み手と重なる |
| ergo_sound | `src/sound/audio_engine.cpp` の `setOutputCallback` / `audioThreadFunc` | `std::function outputCallback_` をオーディオスレッドが読んでいる間に無同期で代入する |
| ergo_custos | `src/custos/http_server.{h,cpp}` の `listen_fd_` | `shutdown()` が書き、`accept_loop` が読む素の `int` |

予定: これらのテストに `LABELS threads` を付け、`ci-tsan` プリセットと
`ctest -L threads` で手元で回せるようにする。
