# Ergo プロジェクト — Claude Code ルール

## プロジェクト概要

Ergo はモジュラー型の C++17 フレームワークである。各モジュールは独立したブランチ (`module/<名前>`) で開発・管理される。

## ブランチ運用ルール

### モジュールブランチ (`module/<名前>`)
- 各モジュールは独立したブランチで開発する
- ブランチ名は `module/<モジュール名>` とする
- モジュール内に `CMakeLists.txt`, `include/`, `src/`, `tests/` を配置する
- テストは GoogleTest を使用する

### main ブランチ
- ドキュメント、ルール定義、モジュールリストのみ配置する
- モジュールのソースコードは配置しない

### release ブランチ
- **自動生成ブランチ** — 全モジュール情報を集積したリリース用ブランチ
- **毎回 soft reset & force push** される（ヒストリを持たない）
- soft reset により前回の情報は読める状態を維持する
- main から派生し、以下を含む:
  - `doc/modules/<モジュール名>/README.md` — 各モジュールの設計書
  - `module_list.md` — 全モジュール一覧（人間用 Markdown テーブル）
  - `module_list.yaml` — 全モジュール一覧（AI/自動処理用 YAML）
  - `README.md` — モジュールサポートリスト付き

## モジュール開発ワークフロー

### 1. テンプレート定義の作成
モジュールを新規作成する際は、`/template/module_template.md` に合わせる形でテンプレート定義を作成し、それを基にモジュールのコードを実装する。

手順:
1. `/template/module_template.md` を参照し、モジュール定義書を作成する
2. 定義書には概要・カテゴリ・所属ドメイン・必要なデータ・依存・変数・作業を記載する
3. 不要な情報は記載しない
4. 作成した定義書は `/spec/module` に格納する
5. 定義書に基づきモジュールのコードを実装する
6. コンパイル短縮用にダミープラグ（実装の無いモジュールファイル）を必ず用意する
7. テストケースを作業に対応する形で作成する

### 2. プラットフォーム別モジュール作成
各プラットフォーム別にモジュールのコードを作成する。

対応プラットフォームは `/template/platform_list.md` を参照。現在の対応:
- Windows
- Linux
- MacOS
- WebGL

手順:
1. モジュール定義書の作業・タスクをプラットフォームごとに確認する
2. プラットフォーム固有の処理が必要な場合、各プラットフォーム向けの実装を分離して作成する
3. 共通処理はプラットフォーム間で共有する
4. 各プラットフォームでテストを実施する

## モジュール設計書フォーマット

各モジュールの設計書 (`doc/modules/<名前>/README.md`) には以下を含める:

1. **概要** — モジュールの目的と特徴
2. **クラス定義** — 各クラスのメソッド一覧（入力/出力/説明のテーブル）
3. **型定義・列挙型** — 使用する型の一覧
4. **設計パターン** — 採用した設計パターン
5. **テストカバレッジ** — テスト項目一覧
6. **ビルド方法** — CMake ビルド手順

## モジュールリストフォーマット

### module_list.md
Markdown テーブル形式で以下の列を含む:
- モジュール名、概要、対応 OS、プラットフォーム、最終更新日、依存モジュール、ブランチ

### module_list.yaml
AI/自動処理用 YAML で以下を含む:
- モジュール名、説明、名前空間、言語、ブランチ、最終更新日
- 対応 OS、対応プラットフォーム、依存関係
- クラス一覧（名前、ヘッダ、説明）
- テスト一覧、設計書パス

## コマンド定義

### `/release` — リリースブランチ更新

全モジュールブランチの情報を集積し、release ブランチを更新する。

**手順:**
1. `git fetch --all` で全ブランチを取得
2. `module/*` ブランチを列挙
3. 各モジュールのヘッダファイル・CMakeLists.txt を解析
4. `doc/modules/<名前>/README.md` に設計書を生成・更新
5. `module_list.md` と `module_list.yaml` を生成・更新
6. `README.md` のサポートリストを更新
7. release ブランチに soft reset & force push

```bash
# 実行イメージ
git checkout release || git checkout -b release origin/main
git reset --soft origin/main
# ... ドキュメント生成 ...
git add -A
git commit -m "Release: update module documentation ($(date +%Y-%m-%d))"
git push -f origin release
```

### `/module-doc <モジュール名>` — モジュール設計書生成

指定モジュールの設計書を生成・更新する。

**手順:**
1. `module/<モジュール名>` ブランチからヘッダファイルを読み取る
2. クラス定義、メソッド I/O、列挙型を解析する
3. `doc/modules/<モジュール名>/README.md` を生成する

### `/module-list` — モジュールリスト更新

全モジュールの一覧ファイルを更新する。

**手順:**
1. `module/*` ブランチを列挙
2. 各ブランチの CMakeLists.txt と最終コミット日を取得
3. `module_list.md` と `module_list.yaml` を再生成
4. `README.md` のサポートリストを更新

### `/deploy-dlls` — DLL デプロイ

ビルド済み DLL を ErgoDLLs リポジトリにコミットする。

**手順:**
1. 各モジュールをビルド（CMake）
2. 生成された DLL/so/dylib を収集
3. ErgoDLLs リポジトリにコピー・コミット・プッシュ

## スキル定義

### release-update スキル
- **トリガー:** ユーザーが `/release` と入力した場合
- **動作:** 上記 `/release` コマンドの手順を実行
- **出力:** release ブランチへの force push

### module-doc スキル
- **トリガー:** ユーザーが `/module-doc <名前>` と入力した場合
- **動作:** 指定モジュールの設計書を生成
- **出力:** `doc/modules/<名前>/README.md` の更新

### module-list スキル
- **トリガー:** ユーザーが `/module-list` と入力した場合
- **動作:** モジュール一覧の再生成
- **出力:** `module_list.md`, `module_list.yaml`, `README.md` の更新

## 注意事項

- モジュールブランチを直接変更する場合は、そのブランチに checkout してから作業する
- release ブランチは手動でコミットしない（自動生成のため）
- DLL デプロイは ErgoDLLs リポジトリの存在を前提とする
- 設計書は日本語で記述する
