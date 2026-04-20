# Ergo

LUDIARS / AdventureCube などのアプリで共通利用する C++17 モジュール群と、
それらに付随する開発ツール群を集約するリポジトリ。

## 運用方針

**main ブランチに全モジュールを集約する。** 旧運用 (モジュールごとに `module/<名>`
ブランチで開発) は廃止した。理由:

- モジュール数 + 付随ツール数の増加で全容把握が難しくなった
- 複数ブランチをまたいだ変更 (例: モジュール ↔ ツールの schema 整合) が
  分割コミットで散らかっていた
- ホストアプリ側 (worktree 経由取り込み) も枝分かれの追跡コストが大きかった

main 一本に集約することで、横断変更を 1 コミットで行え、検索・レビュー・
リリース管理が単純になる。

## ディレクトリ構成

```
ergo/
├── README.md              # ← このファイル
├── CLAUDE.md              # AI/自動化エージェント向けルール
├── CMakeLists.txt         # 全モジュールをまとめてビルドするトップレベル
├── module_list.md         # モジュール一覧 (人間用)
├── module_list.yaml       # モジュール一覧 (機械処理用)
├── spec/
│   └── module/<name>.md   # 各モジュールの定義書
├── include/ergo/<name>/   # 各モジュールの公開ヘッダ
├── src/<name>/            # 各モジュールの実装
├── tests/<name>/          # 各モジュールのテスト
├── tools/<tool-name>/     # 各モジュールに付随する Web ツール等 (TS/Node など)
├── third_party/           # 同梱依存 (mini-gtest など)
└── doc/                   # 補助ドキュメント
```

## モジュール

| 名前 | 概要 | ヘッダ | tools |
|---|---|---|---|
| `ergo_input`     | マウス・キーボード・ゲームパッド・USB HID 統一入力       | `include/ergo/input/`     | — |
| `ergo_inspector` | ブラウザ in-process WS サーバでホスト変数をライブ編集    | `include/ergo/inspector/` | (内蔵 UI) |
| `ergo_particle`  | CPU パーティクル sim + Pictor Vulkan ビルボード描画       | `include/ergo/particle/`  | `tools/particle-editor/` |
| `ergo_bind`      | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`)     | `include/ergo/bind/`      | `tools/variable-editor/` |

詳細は `spec/module/<名>.md` 参照。

## ビルド

トップレベル CMake が全モジュールを `add_library(ergo_<name>)` として公開する。
ホストアプリは `add_subdirectory(<ergo>)` するか、必要なモジュールだけを
ピックして取り込めばよい。

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

各モジュールの個別オプション (例 `ERGO_BIND_BUILD_SERVER=OFF`) はそのまま動作。

## ホストアプリからの利用

旧 worktree パターン (`git worktree add external/ergo/<mod> module/<mod>`) は
不要。AdventureCube のような利用側はリポジトリ全体を `external/ergo/` に
clone (or worktree) して、CMake で `add_subdirectory` するだけでよい。
