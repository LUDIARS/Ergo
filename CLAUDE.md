# Ergo プロジェクト — Claude Code ルール

## プロジェクト概要

Ergo はモジュラー型の C++17 フレームワーク + 付随する Web ツール群。
**全モジュール・全ツールを main ブランチに集約して開発する。**

## 運用方針

### 旧運用 (廃止)

以前は `module/<名>` ブランチで各モジュールを独立開発していたが、以下の
理由で main 集約に戻した:

- モジュール数 + 付随 Web ツール数の増加で全容把握が困難
- モジュール ↔ ツール schema 整合のような横断変更が散らかる
- ホスト側 worktree 取り込みパターンも追跡コスト大

### 現運用

- **main に全コード集約** (モジュールごとのブランチは作らない)
- 各モジュールは `include/ergo/<名>/`, `src/<名>/`, `tests/<名>/` に配置
- Web 開発者ツールは **`tools/ergo/` に統合**。新規ツールは単独
  パッケージではなく `tools/ergo/src/plugins/<id>/` にプラグインとして追加する
  (詳細は `spec/tool/ergo.md`)。
- 仕様書は `spec/module/<名>.md`
- 既存の `module/<名>` ブランチ (`module/input`, `module/particle`,
  `module/bind` など) は履歴保全のため削除しないが、**新規開発は main 上で行う**
- `module/inspector` ブランチも履歴として残すが、`ergo_inspector` モジュール
  自体は 2026-04-21 に廃止 (機能は `ergo_bind` に完全吸収)

### 新規モジュール追加手順

1. `spec/module/<名>.md` を `template/module_template.md` に従い作成
2. `include/ergo/<名>/`, `src/<名>/`, `tests/<名>/` を作成
3. トップレベル `CMakeLists.txt` に `add_library(ergo_<名>)` を追加
4. `module_list.md` / `module_list.yaml` に行を追加
5. (Web ツールが要るなら) `tools/ergo/src/plugins/<id>/` にプラグインとして追加
6. main に直接コミット → push

### 横断変更 (モジュール + ツール)

schema や protocol の変更はモジュール側ヘッダとツール側コードを同一コミットで
更新する。バージョン分裂を防ぐため、必ず両方触ること。

## モジュール定義書

各モジュールは `spec/module/<名>.md` を持つ。`template/module_template.md`
の以下を含める:

1. 概要
2. カテゴリ (UI / ロジック / システム / ゲームオブジェクト)
3. 所属ドメイン
4. 必要なデータ
5. 依存
6. 変数
7. 作業 (入力 / 出力 / タスク)
8. テスト

不要な項目は省略してよい。

## ホストアプリからの取り込み

AdventureCube などの利用側は ergo リポジトリ全体を取り込む:

```cmake
add_subdirectory(<path-to-ergo>)
target_link_libraries(myapp PRIVATE ergo_input ergo_bind)
```

旧 worktree-per-module パターン
(`git worktree add external/ergo/<mod> module/<mod>`) は使わない。

## Web ツールの起動

全ての Web 系開発ツールは `tools/ergo/` に統合済み (`particle` /
`variable` プラグインなど)。

```bash
cd <ergo>/tools/ergo
npm install
npm run dev           # watch, default port 5170
```

利用側 worktree 経由の場合: `<host>/external/ergo/tools/ergo`。
旧 `tools/particle-editor/` / `tools/variable-editor/` は削除済み。
仕様は `spec/tool/ergo.md` を参照。

## 注意事項

- `module/*` ブランチへの新規コミットは原則不要 (どうしても必要な場合のみ
  main へ反映する PR を切ること)
- main へ直接 push する運用なので、コミットメッセージは丁寧に書く
- README.md に最新のモジュール一覧と運用方針を反映する
- 横断変更 (モジュール側 + ツール側) は 1 コミットで行うのが原則
