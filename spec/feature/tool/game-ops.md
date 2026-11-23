# ゲームのワークスペース操作 (services / tuning / git)

Ergo Studio から、ゲームのワークスペースを日々動かす操作をまとめて行うための 3 つのプラグイン。
どれもゲームの名前を知らない。何を扱うかは「ワークスペースのプロフィール」で決まる。

## ワークスペースのプロフィール

JSON 1 ファイルで 1 ワークスペース (`tools/studio/plugins/common/workspace_profiles.h`)。

```json
{
  "name": "amicii",
  "root": "${LUDIARS_ROOT}/Amicii",
  "services": ["amicii-server", "amicii-server-tune"],
  "tuning": { "amicii-server": "games/september/balance/versions", "amicii": "games/september/looks/versions" },
  "git": ["."]
}
```

- 読む場所: `<Studio home>/profiles/*.json` と、`ERGO_PLUGIN_DIR` の各フォルダの `*.studio.json`
  (ゲーム側のプラグインパックが、ライブラリの横にプロフィールを置く)
- パスは `root` からの相対。`root` を省くとファイルのあるフォルダ
- `${NAME}` は環境変数。未設定ならそのプロフィールは使わず、理由を health に出す (空のまま進めない)
- 読めないファイルは health の `profile_errors` に出す

## services (サービス)

- 一覧は Excubitor の `GET /api/v1/services` を、プロフィールの `services` で絞ったもの。
  どのプロフィールも `services` を書いていなければ全サービスを出す
- 起動・再起動の前に Concordia へテストの一報 (`POST /v1/testing/claim`、session_id は `ergo-studio`) を出す。
  別の作業がテスト中なら 409 で止め、画面で「それでも起動」を選んだときだけ進める
- 停止の後に `POST /v1/testing/release` で一報を取り下げる
- 操作は Excubitor の `POST /api/v1/services/<code>/control` (`start` / `stop` / `restart`)。
  起動は Excubitor 経由だけで、Studio が自分でプロセスを立てることはない
- プロフィールに無いサービスの操作は 403

## tuning (調整値)

- 画面: 接続中のゲーム (ergo_bind のアプリ) ごとに、全カテゴリの変数をスライダーと数値欄で調整する。
  `look/...` に限らず `balance/...` なども出る (lookdev タブは見た目の調整と撮影・投稿、こちらは全変数と版)
- 版: 今の値 (読み取り専用でない変数) を名前とメモ付きで保存する。1 版 1 ファイル
  (`<yyyymmdd-hhmmss>-<名前>.json`)。上書きはしない (フォルダがそのまま履歴になり、リポジトリで追える)
- 保存先: プロフィールの `tuning` がアプリに割り当てたフォルダ、無ければ `<Studio home>/tuning/<アプリ>`
- 差分: 版の値と今の値で違うものを並べる。戻す: 版の値を ergo_bind 経由でゲームに書く
  (今のゲームに無い変数は飛ばし、その数を知らせる)

## git

ST-INV-10 (studio.md)。プロフィールの `git` に書いたリポジトリだけを、画面の番号で指して操作する。

| 操作 | git |
|---|---|
| 状態 | `status --porcelain=v2 --branch --untracked-files=all` |
| 履歴 | `log -30` |
| ブランチ | `branch`、切り替え `switch`、作成 `switch -c` (名前は検査する) |
| コミット | `add -A` のあと `commit -F <一時ファイル>` (全変更を含める) |
| 取得 / 取り込み / 送信 | `fetch --prune origin` / `pull --ff-only` / `push --set-upstream origin HEAD` |

## テスト

`tests/studio/test_studio_game_ops.cpp`: プロフィールの解決 (環境変数・相対パス・壊れたファイル)、
サービスの絞り込みと一報の本文、版の保存・一覧・読込・上書きしないこと・差分、
`git status` の解析とブランチ名の検査、一時リポジトリでのコミット・ブランチ作成・履歴。
