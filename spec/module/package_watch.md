# ergo_package_watch モジュール定義

## 概要

パッケージマネージャ (spec/feature/package-manager.md §2.3 / §3、 段階 W3 「検知」) の更新の検知。
依存 1 つ (ホストがパッケージを pin で使う、 または Ergo のマニフェストがパッケージを pin で持つ) ごとに、
取得元のタグと追跡ブランチの先頭を見て tag / track / off-main を判定し、 前回状態と比べて新しい出来事だけを出す。
報告と出来事の形式は [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md)。

ライブラリはプロセスを起こさず、 ネットワークにも出ない。 取得元は interface (`IRefSource` / `IHistorySource`) で受け、
git の起動は CLI のアダプタ (`tools/package_cli/git_remote_source.*` / `git_sibling_source.*`) だけが行う。
由来 (Revisor / Concordia の読み手、 `run` / `review` / `merged`) は W4 で `IReviewSource` / `IRunSource` として足す。

| 部品 | ヘッダ | 中身 |
|---|---|---|
| 依存の列挙 | `watch_dependency.h` | ホストの宣言 (host.ergo・use の共有パッケージ・非公開 link) と Ergo のマニフェスト (link 全て) から。 `track.branch` (既定 `main`)、 持ち主 (`host` / `ergo`)、 取得元の末尾名 |
| 取得元 | `ref_source.h` | `IRefSource` (タグと追跡ブランチの先頭、 Remote / Sibling) と `IHistorySource` (ミラーの同期・距離・祖先判定、 答えられなければ nullopt) |
| ref の一覧 | `ref_listing.h` | `git ls-remote` / `git show-ref --dereference` の行の解析 (注釈つきタグは commit へ剥がす)、 指紋 (FNV-1a 64) |
| 版 | `semver.h` | `v1.2.3` / `v1.2` / `v1.2.3-rc.1` の解析と比較 |
| 観測 | `observation.h` | 依存 1 つを問い合わせる。 指紋が前回と同じならミラーを同期しない、 兄弟チェックアウトは同期しない (`as_of: sibling-last-fetch`、 `sibling_behind`)、 取れなければ `observed: false` と理由 |
| 判定 | `detect.h` | tag (pin より新しい最新の正式版タグ 1 つ)、 track (先頭が pin と違い pin がその祖先、 距離つき)、 off-main (pin が追跡ブランチの祖先でない、 警告) |
| 前回状態 | `watch_state.h` | ホストごとの既知集合 `(依存, kind, 主体)`・最初に見た時刻・最後に通知した時刻・依存ごとの指紋 |
| 差分 | `watch_diff.h` | 純粋関数 `diff(prev, results, now) → (events, next)`。 同じ主体は 1 回、 track は 6 時間に 1 回へ畳む、 解消したものは消す、 取れなかった依存は前回のまま |
| 報告 | `watch_report.h` | `<host>/.ergo/watch-report.json`・`<out>/events.json`・`outdated` の表と JSON。 書く前に漏れ検査 (ergo_package_manager の `find_leaks`)、 当たれば何も作らない |
| 時刻 | `time_text.h` | UTC 秒 ↔ `YYYY-MM-DDTHH:MM:SSZ` (ロケールに依らない) |

## カテゴリ

システム

## 所属ドメイン

build-packages (`spec/domains/build-packages.domain.json`。 ergo_package_manager と CLI `tools/package_cli/` と同じ)

## 必要なデータ

- ホストの `ergo-packages.json` (`host` 節) と Ergo の `ergo-packages.json` (ergo_package_manager が読む)
- 取得元 (CLI のアダプタが渡す): `git ls-remote --tags --heads <repository>`、 任意のミラー `<cache>/mirrors/<Repo>.git`、
  任意の兄弟チェックアウト `<root>/<Repo>`
- 前回状態 `<state>/<code>.watch-state.json`

## 依存

- `ergo_package_manager` (宣言とマニフェストの読み手、 `is_full_sha`、 漏れ検査)、 `ergo_common` (jsonm)。 新しい third-party は無い
- `ergo_package_model` / `ergo_package_scan` / `ergo_package_viz` とは互いに import しない

向き: `tools/package_cli → ergo_package_watch → ergo_package_manager → ergo_common`。

## CLI (`ergo_pkg`、 ターゲット `ergo_pkg_cli`)

| コマンド | 何をするか | 書き換えるもの | ネットワーク |
|---|---|---|---|
| `outdated [--json] [--fail-on tag\|track\|off-main] [--cache <dir>] [--root <dir>]` | 今すぐ判定して表示。 前回状態は読み書きしない。 取れない依存は「未確認」。 `--fail-on` はその種類があれば終了 1 | 無し (ミラーの同期だけ) | 要 (無ければ未確認) |
| `watch --host <dir>... [--state <dir>] [--out <dir>] [--cache <dir>] [--root <dir>]` | 判定 → 前回状態との差分 → `<out>/events.json` (既定 `<state>`)、 ホストごとの `<host>/.ergo/watch-report.json`、 `<state>/<code>.watch-state.json`。 漏れ検査に当たれば何も書かずに終了 3 | 状態と報告のみ | 要 |

- ホスト: `host` 節があればホストの宣言、 無く `ergo-packages.json` が Ergo のマニフェストなら Ergo 自身 (コード `Ergo`)。
  コードは `host.code` (無ければディレクトリ名がコードとして書ける形のとき)。 同じコードの 2 ホストは終了 2
- 前回状態: `--state` > `%LOCALAPPDATA%/ergo/pkg-watch` (Windows) / `$XDG_STATE_HOME/ergo/pkg-watch` / `~/.local/state/ergo/pkg-watch`
- ミラー: `--cache` > `ERGO_PKG_CACHE_DIR` > `ERGO_DEPENDENCY_CACHE_DIR` の `<cache>/mirrors/<Repo>.git`。 `watch` は作らない
  (`git clone --bare --filter=tree:0 <repository> <cache>/mirrors/<Repo>.git` で用意する)。 無ければ距離は空欄
- 兄弟チェックアウト: `--root` > `ERGO_PACKAGES_SIBLING_ROOT` の `<root>/<Repo>`。 あればそれを読み (fetch しない)、 無ければ ls-remote
- git の資格情報は利用者の git に任せる。 トークンは引数・ファイルで受け取らない
- `-DERGO_BUILD_PACKAGE_WATCH=OFF` では CLI は W1 のコマンドだけで作られ、 `outdated` / `watch` は終了 2

## 変数

| 変数 | 意味 |
|---|---|
| `ERGO_BUILD_PACKAGE_WATCH` | モジュールと CLI の `outdated` / `watch` (既定 ON、 `ergo_package_manager` が要る) |
| `ERGO_PKG_CACHE_DIR` / `ERGO_DEPENDENCY_CACHE_DIR` | 実行時: ミラーの `<cache>` |
| `ERGO_PACKAGES_SIBLING_ROOT` | 実行時: 兄弟チェックアウトの根 |

## 作業

- 入力: 上の「必要なデータ」
- 出力: 報告・出来事・次の状態 (形式は `spec/data/ergo-watch-report.md`)
- タスク: W4 (由来)、 W5 (構成時の 1 行と Excubitor の配送)、 W7 (UPM ホスト) はこのモジュールの上に足す。 `pin-behind-ergo` は W4 以降

## テスト

| テスト | 何を確かめるか |
|---|---|
| `test_package_watch` | 偽ソースで: semver、 ref の解析と注釈つきタグ、 指紋、 tag / track / off-main (距離・expect_tag・複数タグの最新 1 つ・pre-release を除く・履歴無しでは距離が空欄)、 取得失敗で `observed: false`、 兄弟は同期しない、 指紋が同じなら同期しない (呼び出し回数)、 `diff` の決定性・2 回目で出来事 0・track の 6 時間畳み込み・pin を上げると解消・取れない依存は前回のまま、 状態の読み直し、 依存の列挙、 漏れ検査で報告を作らない、 URL を載せない |
| `test_package_watch_cli` (`cmake -P`) | 偽リポ (`file://`) で CLI を通しで: `outdated` の tag と track と `--fail-on`、 `watch` の 1 回目と 2 回目 (出来事 0、 状態のバイト一致)、 届かない取得元で状態不変、 兄弟チェックアウトを fetch せずに読む、 漏れる報告で終了 3 と何も書かない |
| `test_packages_track_key` (`cmake -P`) | `track` を足しても CMake の読み手 (`ergo_pkg_manifest_load`) の出力が変わらない |
| `test_package_manager` (TrackKey) | C++ の読み手が `track.branch` を読み、 型違いは MANIFEST |

テストはネットワークを使わない。
