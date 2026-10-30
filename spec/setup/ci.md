# CI (GitHub Actions) のレグとローカル再現

## 目的

Ergo の CI が何をどの環境で検査しているか、どのチェックが必須か、各レグを
手元で再現するコマンドをまとめる。main に入れる前に通すべきチェックと
その運用は [branch-and-pr.md](branch-and-pr.md) 「CI ゲート」。

## ワークフロー

| ファイル | トリガ | 役割 |
|---|---|---|
| `.github/workflows/ci.yml` | main への push / pull_request / workflow_dispatch | C++ の全テスト (各ツールチェーン)、`tools/ergo`、Unity パッケージのコア |
| `.github/workflows/packages-integration.yml` | 毎晩 (schedule) / workflow_dispatch | `ci-packages` プリセットで Ergo + Pictor + Tela を統合ビルド。PR のゲートにはしない |
| `.github/workflows/harness.yml` | main への push / pull_request | LUDIARS 共通 harness (AIFormat)。**手で編集しない** (正本は AIFormat、Castra が配信) |

`ci.yml` は main への push と pull_request で自動実行される。feat ブランチの
SHA を PR なしで検査したいときは workflow_dispatch で起動する
(`gh workflow run ci.yml --ref feat/<name>`)。同じ ref の実行は 1 つずつ走る
(`concurrency`): PR では新しい push が走行中の実行を取り消す。main の push 実行は
マージ後の確認で、走行中のものは取り消さないが、GitHub は待機中の実行を ref ごとに
1 つしか持たないので、さらに新しい push が来ると待機中のものは置き換えられ
"cancelled" になる。ゲートが見るのは候補 SHA 自身の実行 (PR か workflow_dispatch)。

## レグ (`ci.yml`)

| チェック名 | 必須 | 環境 | 主に捕まえるもの |
|---|---|---|---|
| `build-and-test (windows-msvc)` | 必須 | windows-latest、Visual Studio ジェネレータ、Release、`ERGO_AUDIO_BACKEND=dummy` | 主開発環境 (Windows / MSVC) のビルド・テストの回帰 |
| `build-and-test (linux-gcc-cmake320)` | 必須 | ubuntu-24.04、GCC、**CMake 3.20.5**、Ninja、Release | 宣言した最低版 (`CMakeLists.txt:1` の 3.20) に無い CMake 機能 (例: `file(COPY_FILE)` は 3.21)、ポリシー未設定 (`IN_LIST` の CMP0057)、libstdc++ での include 漏れ |
| `build-and-test (linux-clang)` | 必須 | ubuntu-24.04、clang、`-Wall -Wextra`、Release | clang だけが出すエラー。警告はログに出すだけで `-Werror` はまだ付けない |
| `build-and-test (linux-asan)` | 必須 | ubuntu-24.04、GCC、Debug、ASan + LSan + UBSan | 範囲外アクセス、解放後使用、リーク、未定義動作 (下記「サニタイザ」) |
| `build-and-test (macos-14)` | 任意 | macos-14、AppleClang、Release | macOS 固有の差 (入力 HID / ソケット / オーディオ)。workflow_dispatch のときだけ走り、失敗してもワークフローは落とさない |
| `tools-ergo` | 必須 | ubuntu-latest、Node 22 | `tools/ergo` の型 (src + tests)、`tsc` ビルド、node:test (サーバコア・plugin pack 読み込み・パスガード・各プラグインの中継) |
| `unity-package-core` | 必須 | ubuntu-latest、.NET 8 | Unity パッケージ (`unity/com.ludiars.ergo.packages`) のコアを Unity なしで |

- C++ の各レグは ctest の全テストを走らせる (後から足したテストも自動で入る)。
  その中の `test_module_catalog` (`tests/test_module_catalog.cmake`、コンパイラ不要) は
  `module_list.{md,yaml}`・README・[config-reference.md](config-reference.md)・
  `tools/ergo` のプラグイン一覧・`spec/domains` とコードの食い違い、壊れた相対リンクを
  検出する。手元では `cmake -DERGO_SOURCE_DIR=. -P tests/test_module_catalog.cmake`。
- 結果は成否に関わらずアーティファクト `ctest-<レグ名>` に上がる:
  `ctest-junit.xml` と `Testing/Temporary/LastTest.log`。CMake 3.20 の ctest
  には `--output-junit` が無いので、`linux-gcc-cmake320` は LastTest.log だけ。
- Linux / macOS のレグは ccache (レグ名ごとのキャッシュ) を使う。Visual Studio
  ジェネレータはコンパイララウンチャを使えないので、Windows は
  `--parallel` (MSBuild `/m`) と `CL=/MP` で並列化する。
- `windows-msvc` はテスト後に Ergo 自身をホストとして `ergo_pkg status` と
  `ergo_package_graphs` を走らせ、`build/graphs/*.tela` と
  `ergo-packages.{status,resolved}.json` をアーティファクト
  `ergo-package-graphs` に上げる (パッケージマネージャの P4 ホスト図の
  dogfood。失敗してもレグは落とさない)。

## ローカル再現

いずれもリポジトリのルートで実行する。CI と同じく ccache を使うなら
構成に `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` を足す。

### windows-msvc

```powershell
cmake -B build -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy
$env:CL = "/MP"
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --parallel 4 --output-on-failure
```

### linux-gcc-cmake320

プリセット (`CMakePresets.json` は version 3) には CMake 3.21 以上が要るので、
このレグだけ素のコマンドラインで構成する。

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
- `ci-linux-gcc` (GCC + 手元の CMake、Release) も同じ形で使える。CI の
  GCC レグは最低版の検査を兼ねるので CMake 3.20 の素のコマンドライン側。
- ASan を clang で回すなら `cmake --preset ci-asan -DCMAKE_CXX_COMPILER=clang++`。

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

### packages-integration

Windows で、サブモジュールを初期化してから:

```powershell
git submodule update --init --recursive
cmake --preset ci-packages -DERGO_DEPENDENCY_CACHE_DIR=build/deps-cache
cmake --build --preset ci-packages --parallel
ctest --preset ci-packages
```

## packages-integration

- `ci-packages` は `ERGO_PACKAGES=pictor;tela` をパッケージハブで取得
  (`ERGO_PACKAGES_FETCH=ON`) し、警告級の所見をすべてエラーにする
  (`ERGO_PACKAGES_STRICT=ON`)。実際の `ergo_render` バックエンドや
  `if(TARGET ...)` の下でだけ登録されるテストを含む統合構成で、
  [packages.md](packages.md) の「Ergo 単体で実際の Pictor / Tela をマニフェストの pin で
  取る構成」にあたる。
- Windows だけで走らせる (Linux では Pictor の GLFW が wayland / X11 の
  開発パッケージを要求する)。
- STRICT では `lib/lapilli` 未初期化 (PKG-16) がエラーになるので、checkout は
  `submodules: recursive`。
- 取得したソース (FetchContent の基底ディレクトリ =
  `ERGO_DEPENDENCY_CACHE_DIR`) は `ergo-packages.json` と
  `third_party/dependencies.cmake` のハッシュをキーにキャッシュする。
- **まだ PR のゲートではない**。最初の緑が出るまで `continue-on-error: true`。
  緑が続いたら `continue-on-error` を外し、必要なら pull_request を
  `ergo-packages.json` / `CMakePresets.json` / `cmake/**` / `third_party/**` /
  `src/render/**` / `src/tela_graph/**` / `src/package_*/**` /
  `tests/packages/**` の paths 絞りで足す。
- Pictor / Tela は公開リポジトリなので既定のトークンで取得できる。非公開に
  なった場合は、読み取り専用トークンを secret に置き、構成の前に
  `git config --global url."https://x-access-token:<token>@github.com/".insteadOf "https://github.com/"`
  を実行するステップを足す。

## サニタイザ

### ASan / LSan / UBSan (`linux-asan`、必須)

- `ci-asan` プリセット: Debug、コンパイルとリンクの両方に
  `-fsanitize=address,undefined`。`-fno-sanitize-recover=undefined` で UB は
  その場でテストを落とす。テストプリセットが `ASAN_OPTIONS=detect_leaks=1` と
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` を設定する。
- CI ではランナーの `vm.mmap_rnd_bits` を 28 に下げてから走らせる
  (新しいカーネルの既定値ではサニタイザのシャドウメモリの配置が壊れることがある)。
- **ergo_profile の意図的な非解放**: スレッドごとのイベントバッファは
  エクスポートのためにプロセスの寿命の間残す (解放しない)。バッファを指す
  登録簿 (`src/profile/profile.cpp` の `thread_buffers()`) も破棄しない
  ヒープオブジェクトにしてあるので、LeakSanitizer からは到達可能に見え、
  リークとして報告されない。登録簿を普通のグローバル `std::vector` に戻すと
  静的破棄で先に消え、`test_profile` が LSan で落ちる。意図的に解放しない
  オブジェクトを足すときも同じく「到達可能なまま残す」形にし、
  サプレッションで隠さない。

### TSan (後続)

ThreadSanitizer のレグはまだ無い。スレッドを立てるテスト
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
`ctest -L threads` の夜間レグを報告のみで足す。3 件が直ったら必須にする。

## 必須チェック

main のルールセット (GitHub の管理設定) で必須にするチェック名は次のとおり
(名前は完全一致で指定する):

- `build-and-test (windows-msvc)`
- `build-and-test (linux-gcc-cmake320)`
- `build-and-test (linux-clang)`
- `build-and-test (linux-asan)`
- `tools-ergo`
- `unity-package-core`
- `harness / harness` (GitHub PR 経由で入れる場合だけ。harness は
  pull_request と main への push でしか走らない)

`build-and-test (macos-14)` と `packages-integration` は必須にしない。
レグを足したり名前を変えたりしたら、このリストとルールセットを同じ変更で
更新する。運用 (候補 SHA の扱い、main が赤のとき) は
[branch-and-pr.md](branch-and-pr.md) 「CI ゲート」。
