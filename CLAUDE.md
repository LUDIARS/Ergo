# Ergo プロジェクト — Claude Code ルール

## プロジェクト概要

Ergo はモジュラー型の C++17 フレームワーク + 付随する Web ツール群。
**全モジュール・全ツールを 1 つの main 系列に集約して開発する** (per-module の
長命ブランチは作らない)。 一方で **変更は必ず feat ブランチ + PR で main に
入れる** — main 直 push は禁止 (下記「ブランチ運用」参照)。

## 運用方針

### ブランチ運用 (例外なし)

- **main への直接コミット / push は禁止**。 全変更は短命な feat / fix / docs /
  chore ブランチを切って PR を経由する
  - 例外なし。 typo 修正・1 行の doc 修正でも feat ブランチ + PR
- 推奨ブランチ命名: `feat/<short-name>` / `fix/<...>` / `docs/<...>` / `chore/<...>`
- PR は通常 squash merge。 main の履歴は 1 機能 1 コミットを目安に保つ
- うっかり main に直接コミットした場合は `git checkout -b feat/<name>` で
  退避 → `git checkout main && git reset --hard <prev>` で巻き戻してから PR
- 旧運用 (「main に直接 push」) は 2026-05-15 に廃止。 本ファイルが正

### コード配置の方針

- **全モジュール / 全ツールのソースは main 系列の 1 ツリーに置く**
  (per-module の独立ブランチは作らない。 長命ブランチで分散させない)
- 各モジュールは `include/ergo/<名>/`, `src/<名>/`, `tests/<名>/` に配置
- Web 開発者ツールは **`tools/ergo/` に統合**。 新規ツールは単独
  パッケージではなく `tools/ergo/src/plugins/<id>/` にプラグインとして追加する
  (詳細は `spec/tool/ergo.md`)。 ゲーム固有のエディタは Ergo に入れず、
  ホストリポの plugin pack + `ERGO_PLUGIN_DIR` でロードする
- 仕様書は `spec/module/<名>.md`

### 旧運用 (履歴保全のみ・参照禁止)

- `module/<名>` ブランチ群 (`module/input` / `module/particle` / `module/bind`
  など) は履歴保全のため残しているが、**新規コミット禁止**
- `module/inspector` ブランチも履歴として残すが、 `ergo_inspector` モジュール
  自体は 2026-04-21 に廃止 (機能は `ergo_bind` に完全吸収)
- 旧 worktree-per-module パターン (`git worktree add external/ergo/<mod>
  module/<mod>`) は使わない

### 新規モジュール追加手順

1. `git checkout -b feat/<モジュール名>` で main からブランチを切る
2. `spec/module/<名>.md` を `template/module_template.md` に従い作成
3. `include/ergo/<名>/`, `src/<名>/`, `tests/<名>/` を作成
4. トップレベル `CMakeLists.txt` に `add_library(ergo_<名>)` を追加
5. `module_list.md` / `module_list.yaml` に行を追加
6. (Web ツールが要るなら) `tools/ergo/src/plugins/<id>/` にプラグインとして追加
7. 1 PR にまとめて push → レビュー → squash merge

### 横断変更 (モジュール + ツール)

schema や protocol の変更はモジュール側ヘッダとツール側コードを同一 PR (理想は
同一コミット) で更新する。バージョン分裂を防ぐため、必ず両方触ること。

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

- `module/*` ブランチへの新規コミットは禁止 (履歴保全のみ)。 該当領域への
  修正は main から feat ブランチを切って PR で入れる
- 全変更は feat / fix / docs / chore ブランチ + PR を経由する (上記「ブランチ運用」)
- PR メッセージとコミットメッセージは丁寧に書く (squash merge で main の履歴に
  そのまま残る)
- README.md に最新のモジュール一覧と運用方針を反映する
- 横断変更 (モジュール側 + ツール側) は 1 PR (理想は 1 コミット) で行うのが原則
