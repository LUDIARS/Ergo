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

## 取り込みの流れ

実際の取り込みは次の 2 通り。どちらも下の「マージのゲート」を通ってから main に入る。

- **Revisor local PR** (AI の委託作業の通常経路): feat/* ブランチで作業し、
  base `main` の Revisor local PR として提出する。Revisor が審査し、現在の
  main の上に squash したコミット (候補) を作り、その候補がローカルレビューを
  通ってから main をその SHA へ進める (fast-forward)。GitHub PR は作らない。
- **GitHub PR**: 下の手順で feat ブランチを push して `gh pr create`。
  squash merge で main に入る。GitHub Actions で走るのは LUDIARS 共通の
  harness (`.github/workflows/harness.yml`) だけで、Ergo のビルド・テストは
  Revisor のローカルレビューで確かめる。

## 手順 (GitHub PR)

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

## マージのゲート

main に入る SHA は、入る前にその SHA 自体で Revisor のローカルレビューを通っていること。
検証の構成と手元での実行は [ci.md](ci.md)。

- **Revisor のローカルレビュー**: 登録されたテストケース `configure` / `build` /
  `ctest` (CMake の構成・ビルドと ctest の全テスト)、Praesidium の事前検査、
  Anatomia の検査、漏洩検査。テストケースのコマンドは運用者の Revisor に登録されている。
- GitHub Actions は Ergo のビルド・テストを走らせない (以前のワークフローは廃止)。
  GitHub PR 経由なら `harness / harness` (`.github/workflows/harness.yml`) も通す。
- **候補 SHA は現在の main の上に載せてから検査する。** 古い main を土台に
  通った SHA は数えない。main が先に進んだら rebase (squash し直し) して
  新しい SHA で検査をやり直す。並行作業がそれぞれ古い土台で通り、main で
  合わさって壊れるのを防ぐため。
- **main のテストが落ちている間は main に push しない。** 先に直す変更を同じゲートで
  通す。壊れた上に別の変更を積むと原因の切り分けができなくなる。
- Revisor 側の設定 (登録テストケース・自動マージの有無)・main の保護・merge commit / rebase merge の
  無効化は、リポジトリの外 (Revisor と GitHub の管理設定) で行う。
- 手元で追加の構成 (clang・サニタイザ・Windows・統合ビルド) を回すときのコマンドは
  [ci.md](ci.md)。

## 注意点

- 新規モジュール追加の一連手順 (spec / include / src / tests / CMake の option と
  add_library / module_list / README / config-reference.md / spec/domains →
  `test_module_catalog` で確認 → 1 PR) は [build-cpp.md](build-cpp.md) の「注意点」を参照。
- PR / コミットメッセージは丁寧に書く (squash merge で main 履歴にそのまま残る)。
