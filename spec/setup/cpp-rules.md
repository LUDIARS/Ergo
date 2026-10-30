# C++ 規約のツール設定 (AIFormat/cpp の同期コピー)

Ergo の C++ は AIFormat の [`RULE_CODE_CPP.md`](../../../AIFormat/RULE_CODE_CPP.md) に従う。
整形・静的解析・エディタ設定・CMake baseline・規則表の **正本は `AIFormat/cpp/`** にあり、
Ergo にあるのは版を付けた同期コピー。Ergo 側で中身を書き換えない (§C9.3)。
変えたいときは AIFormat へ PR を出し、ここへ同期し直す。

同期元の版は各ファイル 1 行目の `# synced-from AIFormat/cpp@<commit>`
(規則表は `tools/cpp-rules/SYNCED_FROM`) で判定する。

## ファイル

| Ergo のパス | 正本 | 役割 |
|---|---|---|
| `.clang-format` | `cpp/.clang-format` | 整形 (`@REPO@` → `ergo`) |
| `.clang-tidy` | `cpp/.clang-tidy` | 静的解析 (`@REPO_UPPER@` → `ERGO`) |
| `.editorconfig` | `cpp/.editorconfig` | エディタ既定値 |
| `third_party/.clang-format` | — | 外部コードは整形しない (`DisableFormat`) |
| `cmake/LudiarsCxxBaseline.cmake` | `cpp/LudiarsCxxBaseline.cmake` | usage / warnings baseline と configure 検査 |
| `tools/cpp-rules/` | `cpp/rules/` (テストを除く) | 規則表 (`cpp-build-policy` gate) |
| `cpp-rules.json` | — (Ergo の設定) | 規則表の Ergo 用設定 |

## 規則表 tools/cpp-rules

`tools/cpp-rules/cpp-rules.mjs` が規則表と CLI の入口。merge-base からの変更の追加行に
§C の規則を掛ける。

```sh
node tools/cpp-rules/cpp-rules.mjs . --base origin/main
```

### 規則表の lib (tools/cpp-rules/lib)

- `tools/cpp-rules/lib/cli.mjs` — CLI 引数の解釈と finding の出力
- `tools/cpp-rules/lib/config.mjs` — `cpp-rules.json` の読み込みと検証 (未知キーは例外)
- `tools/cpp-rules/lib/engine.mjs` — 規則の適用 (ファイル単位・変更集合単位)
- `tools/cpp-rules/lib/git-diff.mjs` — merge-base からの変更と追加行の収集
- `tools/cpp-rules/lib/checks-cpp.mjs` — C++ ソースの規則 (キャスト・スレッド注記など)
- `tools/cpp-rules/lib/checks-cmake.mjs` — CMake の規則 (標準指定・file(GLOB) など)
- `tools/cpp-rules/lib/checks-changeset.mjs` — 変更集合の規則 (concept テスト・fuzz ハーネス)
- `tools/cpp-rules/lib/mask.mjs` / `syntax.mjs` — コメント・文字列の除外と字句処理
- `tools/cpp-rules/lib/suppression.mjs` — `NOLINT` / `NOLINTNEXTLINE` 注記の解釈
- `tools/cpp-rules/lib/paths.mjs` — パスの正規化と glob

### cpp-rules.json (Ergo の設定)

| キー | 値 | 意味 |
|---|---|---|
| `prefix` | `ERGO` | マクロ prefix (§C6.7 / §C4.7 / §C4.10) |
| `logger.files` | `src/log/log.cpp` | 共有ロガーの実装 (§C8.4 の除外) |

## CMake baseline (ERGO_CXX_BASELINE)

baseline は C++20 を下限にする (§C4.1)。Ergo とそれを取り込むホストは C++17 なので、
移行が済むまで `ERGO_CXX_BASELINE` (既定 `OFF`) で切り替える。

- `OFF` (既定): baseline を include しない。ビルドは従来どおり。
- `ON`: `cmake/LudiarsCxxBaseline.cmake` を include し、configure の最後に
  `ludiars_cxx_check_targets(PREFIX ergo_ MODE WARNING)` で baseline 未リンクの
  `ergo_` target を WARNING で一覧する (移行の残りを見る用途)。

C++20 への移行 (`set(CMAKE_CXX_STANDARD 17)` の撤去、全 target の
`ludiars_cxx_usage` PUBLIC / `ludiars_cxx_warnings` PRIVATE リンク、`MODE FATAL_ERROR`) は
ホスト (AdventureCube / KuzuSurvivors など) のビルド確認と合わせて別 PR で行う。

## 既知の扱い

- §C3.2 `tests-main`: `tests/*/fixtures/*/main.cpp` はホストゲーム役の入れ子ビルドなので
  `main` を持つのが正しい。規則表は変更行だけを見るので触らなければ出ない。
  fixtures の除外は正本 (AIFormat/cpp) 側で扱う。
- C++17 では書けない指摘 (§C8.3 指示付き初期化子、§C1.6 `std::span`) は C++20 移行時に直す。
