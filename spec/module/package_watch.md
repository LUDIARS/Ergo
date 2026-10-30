# ergo_package_watch モジュール定義

## 概要

パッケージマネージャ (spec/feature/package-manager.md §2.3 / §3、 段階 W3 「検知」) の更新の検知。
依存 1 つ (ホストがパッケージを pin で使う、 または Ergo のマニフェストがパッケージを pin で持つ) ごとに、
取得元のタグと追跡ブランチの先頭を見て tag / track / off-main を判定し、 前回状態と比べて新しい出来事だけを出す。
報告と出来事の形式は [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md)。

ライブラリはプロセスを起こさず、 ネットワークにも出ない。 取得元は interface (`IRefSource` / `IHistorySource`) で受け、
git の起動は CLI のアダプタ (`tools/package_cli/git_remote_source.*` / `git_sibling_source.*`) だけが行う。
由来 (段階 W4) は `IReviewSource` (Revisor の local PR) / `IRunSource` (Concordia の委託 run) で受け、 HTTP は CLI のアダプタ
(`tools/package_cli/revisor_review_source.*` / `concordia_run_source.*`、 `ergo_http`) だけが行う。

| 部品 | ヘッダ | 中身 |
|---|---|---|
| 依存の列挙 | `watch_dependency.h` | ホストの宣言 (host.ergo・use の共有パッケージ・非公開 link) と Ergo のマニフェスト (link 全て) から。 `track.branch` (既定 `main`)、 持ち主 (`host` / `ergo`)、 取得元の末尾名 |
| UPM ホストの依存 (W7) | `upm_dependency.h` | Unity プロジェクトの `Packages/manifest.json` の LUDIARS の git 依存のうち `#<40 桁 SHA>` のもの。 取得元は `git+` と `?path=...` を落とした URL、 コードは Ergo のマニフェストで同じリポを指すパッケージのコード (Ergo 自身は `Eg`、 無ければ末尾名)、 持ち主 `upm`、 追跡ブランチ `main`、 `expect_tag` 無し。 `#main` などブランチ指定は見られない理由を積む |
| 取得元 | `ref_source.h` | `IRefSource` (タグと追跡ブランチの先頭、 Remote / Sibling) と `IHistorySource` (ミラーの同期・距離・祖先判定、 答えられなければ nullopt) |
| ref の一覧 | `ref_listing.h` | `git ls-remote` / `git show-ref --dereference` の行の解析 (注釈つきタグは commit へ剥がす)、 指紋 (FNV-1a 64) |
| 版 | `semver.h` | `v1.2.3` / `v1.2` / `v1.2.3-rc.1` の解析と比較 |
| 観測 | `observation.h` | 依存 1 つを問い合わせる。 指紋が前回と同じならミラーを同期しない、 兄弟チェックアウトは同期しない (`as_of: sibling-last-fetch`、 `sibling_behind`)、 取れなければ `observed: false` と理由 |
| 判定 | `detect.h` | tag (pin より新しい最新の正式版タグ 1 つ)、 track (先頭が pin と違い pin がその祖先、 距離つき)、 off-main (pin が追跡ブランチの祖先でない、 警告) |
| 前回状態 | `watch_state.h` | ホストごとの既知集合 `(依存, kind, 主体)`・最初に見た時刻・最後に通知した時刻・依存ごとの指紋 |
| 差分 | `watch_diff.h` | 純粋関数 `diff(prev, results, now) → (events, next)`。 同じ主体は 1 回、 track は 6 時間に 1 回へ畳む、 解消したものは消す、 取れなかった依存は前回のまま |
| 報告 | `watch_report.h` | `<host>/.ergo/watch-report.json`・`<out>/events.json`・`outdated` の表と JSON。 書く前に漏れ検査 (ergo_package_manager の `find_leaks`)、 当たれば何も作らない |
| 時刻 | `time_text.h` | UTC 秒 ↔ `YYYY-MM-DDTHH:MM:SSZ` (ロケールに依らない) |
| 由来の取得元 (W4) | `provenance_source.h` | `IReviewSource` (open な local PR と merged) / `IRunSource` (委託 run)。 DTO は突合に要るキーだけで、 PR のタイトル・本文を持たない |
| 一覧の読み手 (W4) | `provenance_json.h` | Revisor の `{pullRequests}` と Concordia の `{runs}` を DTO に写す。 run の対象リポは `sessions[].repo_origin` → `args.target_repo` の末尾名だけ |
| 由来の判定 (W4) | `provenance.h` | run (対象リポの進行中の委託 run)・review (base が追跡ブランチの open な local PR)・merged (前回 review だった PR が merged、 1 回だけ) と由来 pf / ai / human / unknown。 `attach_provenance` で依存の結果に足す |
| URL の検査 (W4) | `loopback_url.h` | `http(s)://` で host が `localhost` / `127.0.0.0/8` / `[::1]`。 `user@` は拒む |

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
- UPM ホスト (W7): `ergo-packages.json` が無く `Packages/manifest.json` があるディレクトリは Unity プロジェクト。 コードは
  `.ergo/host.json` の `code` (`ergo_pkg init --upm` が作る。 無ければディレクトリ名)。 報告は同じく `<host>/.ergo/watch-report.json`
  で、 出来事の `next` は Unity の窓 (Window > LUDIARS > Ergo Packages) を指す (`ergo_pkg update` は UPM の pin を書かない)。
  読む側は Unity パッケージ `unity/com.ludiars.ergo.packages` (統合設計 §6)
- 前回状態: `--state` > `%LOCALAPPDATA%/ergo/pkg-watch` (Windows) / `$XDG_STATE_HOME/ergo/pkg-watch` / `~/.local/state/ergo/pkg-watch`
- ミラー: `--cache` > `ERGO_PKG_CACHE_DIR` > `ERGO_DEPENDENCY_CACHE_DIR` の `<cache>/mirrors/<Repo>.git`。 `watch` は作らない
  (`git clone --bare --filter=tree:0 <repository> <cache>/mirrors/<Repo>.git` で用意する)。 無ければ距離は空欄
- 兄弟チェックアウト: `--root` > `ERGO_PACKAGES_SIBLING_ROOT` の `<root>/<Repo>`。 あればそれを読み (fetch しない)、 無ければ ls-remote
- git の資格情報は利用者の git に任せる。 トークンは引数・ファイルで受け取らない
- `-DERGO_BUILD_PACKAGE_WATCH=OFF` では CLI は W1 のコマンドだけで作られ、 `outdated` / `watch` は終了 2
- 由来 (W4): `watch --revisor-url <u>` > `ERGO_WATCH_REVISOR_URL`、 `--concordia-url <u>` > `ERGO_WATCH_CONCORDIA_URL`。
  loopback 以外は終了 2 (URL そのものは出力しない)。 渡されなければその取得元は見ない (報告の `sources.*` が `off`)。
  読むのは 1 回の `watch` で Revisor 2 往復 (`GET /v1/local-prs?view=full&state=open`、 `?view=summary&state=merged`) と
  Concordia 1 往復 (`GET /v1/delegation/runs?limit=500`)。 どちらも loopback から認可なしで読めるので、 トークンは受け取らない。
  届かない・HTTP 200 以外・形が違うときは `unavailable` にし、 その取得元の `run` / `review` / `merged` を出さず、 その種類の既知は前回のまま残す。
  リダイレクトは追わない。 HTTP は `ergo_http` (`-DERGO_BUILD_HTTP=ON`) があるときだけ組み込まれ、 無いビルドでは URL を渡しても `unavailable`

## 変数

| 変数 | 意味 |
|---|---|
| `ERGO_BUILD_PACKAGE_WATCH` | モジュールと CLI の `outdated` / `watch` (既定 ON、 `ergo_package_manager` が要る) |
| `ERGO_PKG_CACHE_DIR` / `ERGO_DEPENDENCY_CACHE_DIR` | 実行時: ミラーの `<cache>` |
| `ERGO_PACKAGES_SIBLING_ROOT` | 実行時: 兄弟チェックアウトの根 |
| `ERGO_WATCH_REVISOR_URL` / `ERGO_WATCH_CONCORDIA_URL` | 実行時 (W4): Revisor / Concordia の基底 URL (loopback だけ) |
| `ERGO_BUILD_HTTP` | 構成時: ON なら CLI に Revisor / Concordia の HTTP アダプタを組み込む (既定 OFF、 libcurl を取得してビルドする) |

## 作業

- 入力: 上の「必要なデータ」
- 出力: 報告・出来事・次の状態 (形式は `spec/data/ergo-watch-report.md`)
- タスク: W4 (由来) は実装済み。 W5 (構成時の 1 行と Excubitor の配送) はこのモジュールの上に足す。 `pin-behind-ergo` は未実装 (Ergo のマニフェストの main を読む経路が要る)。
  W7 (UPM ホスト) は `upm_dependency.h` と CLI の `watch_host` / `upm_host` で実装済み (`spec/tasks/2026-10-15-package-manager-w7.md`)

## テスト

| テスト | 何を確かめるか |
|---|---|
| `test_package_watch` | 偽ソースで: semver、 ref の解析と注釈つきタグ、 指紋、 tag / track / off-main (距離・expect_tag・複数タグの最新 1 つ・pre-release を除く・履歴無しでは距離が空欄)、 取得失敗で `observed: false`、 兄弟は同期しない、 指紋が同じなら同期しない (呼び出し回数)、 `diff` の決定性・2 回目で出来事 0・track の 6 時間畳み込み・pin を上げると解消・取れない依存は前回のまま、 状態の読み直し、 依存の列挙、 漏れ検査で報告を作らない、 URL を載せない |
| `test_package_watch` (PackageWatchProvenance.* / PackageWatchLoopback.*) | Revisor / Concordia の JSON 見本 (`fixtures/provenance/*.json`。 API の応答で Unity と共有しないので平らな `fixtures/*.json` の外) で: 一覧の読み手、 pf / ai (trigger 写し) / human / unknown の突合、 run と review の対象 (別リポ・別 base・終わった run は出ない)、 open → merged で merged が 1 回 (状態に残さない)、 UPM ホストの merged の `next` は Unity の窓で `ergo_pkg` を含まない、 落ちていると run / review を出さず `unavailable` で既知は前回のまま、 Concordia だけ落ちると review の由来が unknown、 出来事・報告・状態にタイトル・本文・ブランチ名・セッション id が入らない (見本にわざと入れてある)、 入力の並びによらない、 loopback の URL の判定 |
| `test_package_watch` (PackageWatchUpm、 W7) | UPM ホスト見本 (`fixtures/upm-*.json`) から依存を列挙 (`#main` は見ない、 コードの対応)、 偽ソースで tag / track の出来事を出す、 取れない依存は `observed:false`、 報告が漏れ検査を通り URL を載せない、 Unity と共有する報告見本 `fixtures/watch-report.json` が書き手の出力と同じキーを持つ (`sources` の子キーを含む) |
| `test_package_watch_cli` (`cmake -P`) | 偽リポ (`file://`) で CLI を通しで: `outdated` の tag と track と `--fail-on`、 `watch` の 1 回目と 2 回目 (出来事 0、 状態のバイト一致)、 届かない取得元で状態不変、 兄弟チェックアウトを fetch せずに読む、 漏れる報告で終了 3 と何も書かない、 W4: loopback 以外の URL (引数と環境変数) で終了 2、 誰も聞いていない loopback で `sources.*` が `unavailable` かつ run / review / merged が無い、 URL 無しは `off`、 W7: Unity プロジェクトを UPM ホストとして読み出来事を出す |
| `test_unity_package_structure` (`cmake -P`、 W7) | Unity パッケージの構造 (package.json の必須キー・asmdef が Editor 専用・`Runtime/` が無い・全資産に .meta・GUID の重複無し・絶対パス無し) と、 `Tests/Editor/Fixtures` が `tests/package_watch/fixtures/*.json` とバイト一致 |
| `test_packages_track_key` (`cmake -P`) | `track` を足しても CMake の読み手 (`ergo_pkg_manifest_load`) の出力が変わらない |
| `test_package_manager` (TrackKey) | C++ の読み手が `track.branch` を読み、 型違いは MANIFEST |

テストはネットワークを使わない。
