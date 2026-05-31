# ブランチ + PR 運用 (main 直 push 禁止)

## 目的

Ergo への変更を正しく main に入れるための運用ルール。**全変更は feat ブランチ
+ PR を経由**し、main への直接コミット / push は禁止 (例外なし)。

> 注意: 古いドキュメント (旧 README / 一部コメント) に「main に直接 push」と
> 読める記述が残るが、これは **2026-05-15 に廃止された旧運用**。現行の正は
> [`../../CLAUDE.md`](../../CLAUDE.md) 「ブランチ運用 (例外なし)」。

## ルール (正本: [`../../CLAUDE.md`](../../CLAUDE.md))

- main への直接コミット / push は禁止。typo・1 行 doc 修正でも feat ブランチ + PR。
- ブランチ命名: `feat/<short>` / `fix/<...>` / `docs/<...>` / `chore/<...>`。
- PR は通常 **squash merge**。main の履歴は 1 機能 1 コミットを目安に保つ。
- `module/<名>` などの旧ブランチ群は履歴保全のみで **新規コミット禁止**。
  該当領域の修正も main から feat ブランチを切って PR で入れる。

## 横断変更 (モジュール + ツール)

schema / protocol の変更は **モジュール側ヘッダとツール側コードを同一 PR
(理想は同一コミット)** で更新する。バージョン分裂を防ぐため必ず両方触る
([`../../CLAUDE.md`](../../CLAUDE.md) 「横断変更」)。

## 手順

```bash
# 1. main からブランチを切る
git checkout main
git pull
git checkout -b feat/<short-name>

# 2. 変更してコミット
git add -A
git commit -F msg.txt

# 3. push して PR を作成
git push -u origin feat/<short-name>
gh pr create
```

うっかり main に直接コミットした場合の退避:

```bash
git checkout -b feat/<name>            # 変更を退避
git checkout main
git reset --hard <prev>                # main を巻き戻し
# → feat/<name> から PR を作る
```

## 注意点

- 新規モジュール追加の一連手順 (spec / include / src / tests / CMake /
  module_list 更新 → 1 PR) は [`../../CLAUDE.md`](../../CLAUDE.md)
  「新規モジュール追加手順」を参照。
- PR / コミットメッセージは丁寧に書く (squash merge で main 履歴にそのまま残る)。
