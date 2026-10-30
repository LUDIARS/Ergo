# パッケージマネージャ W4 — 由来

- 日付: 2026-10-15
- タスク参照: `actio:291af7a7-c212-429f-8da0-164d582bad7f` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §1 (Pf / AI による更新の定義) / §3「由来」 / §7 の W4 行と受入条件 / §8 / §9 (W-5・W-9 は推奨どおり)
- 前提: W3 ([`2026-10-15-package-manager-w3.md`](2026-10-15-package-manager-w3.md)、モジュール [`spec/module/package_watch.md`](../module/package_watch.md))
- 形式: [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md)
- ブランチ: `feat/package-manager-w4`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 由来の取得元の interface と DTO (`IReviewSource` / `IRunSource`)。DTO は突合に要るキーだけを持ち、PR のタイトル・本文を持たない | `include/ergo/package_watch/provenance_source.h` |
| 2 | Revisor (`{pullRequests}`) と Concordia (`{runs}`) の一覧の読み手。run の対象リポは `sessions[].repo_origin` → `args.target_repo` の末尾名だけ | `provenance_json.h` / `.cpp` |
| 3 | 判定: `run` / `review` / `merged` と由来 `pf` / `ai` / `human` / `unknown` (`triggered_by == "praeforma"` が pf)。`attach_provenance` で依存の結果に足す | `provenance.h` / `.cpp`、`detect.h` (`UpdateKind` と `Candidate` の追加キー) |
| 4 | `diff` を取得元ごとに: 見られなかった取得元の既知は前回のまま、`merged` は 1 回だけ出して状態に残さない | `watch_diff.h` / `.cpp`、`watch_state.cpp` |
| 5 | 報告と `events.json` に `sources.{revisor,concordia}` (`ok` / `unavailable` / `off`) | `watch_report.h` / `.cpp` |
| 6 | loopback の URL の検査 | `loopback_url.h` / `.cpp` |
| 7 | CLI: `watch --revisor-url / --concordia-url` (`ERGO_WATCH_REVISOR_URL` / `ERGO_WATCH_CONCORDIA_URL`)、loopback 以外は終了 2。HTTP は `ergo_http` のアダプタ (`ERGO_BUILD_HTTP=ON` のときだけ組み込む) | `tools/package_cli/provenance_endpoints.*` / `provenance_sources.*` / `http_json_get.*` / `revisor_review_source.*` / `concordia_run_source.*`、`cmd_watch.cpp`、`cli_args.*`、`CMakeLists.txt` |
| 8 | テスト (JSON 見本・偽リポ、ネットワーク無し) | `tests/package_watch/test_provenance.cpp`、`tests/package_watch/fixtures/provenance/{revisor-open,revisor-merged,concordia-runs}.json` (Unity の `Tests/Editor/Fixtures` と共有しない API 応答の見本なので、平らな `fixtures/*.json` の外に置く)、`tests/packages/test_package_watch_cli.cmake` |
| 9 | 文書: モジュール・形式・統合設計の実装状況・一覧 | `spec/module/package_watch.md`、`spec/data/ergo-watch-report.md`、`spec/feature/package-manager.md`、`module_list.md` / `.yaml`、`README.md` |

新しいモジュールと新しいトップレベルは無い (全て `ergo_package_watch` と `tools/package_cli/` の中)。`spec/domains/build-packages.domain.json`
と `.anatomia/layers.json` は既存のパターン (`src/package_watch/*` など) が新しいファイルを含むので変えない。

## 受け入れ条件 (契約書式)

- C-1 classify_review(pr, runs): local PR 見本 + run 見本の突合で pf (sessionId == child_session_id、triggered_by == praeforma) / ai (headRef == spawn_branch かつ同じリポ、trigger に triggered_by を写す) / human (突合する run が無い) / unknown (Concordia に届かない・見ていない) — `test_package_watch` (PackageWatchProvenance.OriginMatrix、ConcordiaDownMakesOriginUnknown)
- C-2 detect_provenance(dep, inputs, previous): run は対象リポの進行中の run だけ、review は同じリポで base が追跡ブランチの open な PR だけ — `test_package_watch` (PackageWatchProvenance.DetectsRunsAndReviews)
- C-3 diff(prev, results, now): open → merged で `merged` が 1 回、由来は前回の review から写し、状態に残さない。3 回目は 0 件 — `test_package_watch` (PackageWatchProvenance.OpenToMergedOnce)
- C-4 attach_provenance / diff / prepare_watch_outputs: Revisor / Concordia が落ちていると run / review を出さず `sources.* = "unavailable"`、その種類の既知は前回のまま — `test_package_watch` (PackageWatchProvenance.UnavailableSourcesKeepState)、`test_package_watch_cli` (誰も聞いていない loopback)
- C-5 is_loopback_url / `ergo_pkg watch`: loopback 以外の URL (引数と環境変数) で終了 2 — `test_package_watch` (PackageWatchLoopback.AcceptsOnlyLoopback)、`test_package_watch_cli`
- C-6 prepare_watch_outputs / state_to_json: 出来事・報告・状態に PR のタイトル・本文・ブランチ名・セッション id が入らない (見本にわざと入れてある) — `test_package_watch` (PackageWatchProvenance.EventsCarryNoTitleBodyOrBranch)
- C-7 diff / candidate_to_json: 入力の並びによらず同じバイト列、tag / track / off-main の形は W3 のまま — `test_package_watch` (PackageWatchProvenance.DeterministicOverInputOrder、RefKindsKeepTheirW3Shape、既存の PackageWatchDiff.*)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / CMake は対象にならない (W1〜W3 と同じ)。上の受入条件は ctest のテスト名で裏づける。

## 判断 (§9 と実装を見て決めたこと)

| 論点 | 決めたこと | 根拠 |
|---|---|---|
| 読み取りの認可 | 要らない。トークンは受け取らない | Revisor の `GET /v1/local-prs` は loopback だけで読める (workflow token は書き込みだけ)。Concordia の `GET /v1/delegation/runs` も loopback から読める |
| Revisor の一覧の取り方 | `view=full&state=open` と `view=summary&state=merged` の 2 往復 | `view=summary` は `headRef` / `sessionId` / `baseRef` を持たず突合できない。merged の判定は番号と `mergeCommitSha` だけで足りるので軽い summary にする。full の open 一覧はタイトル・本文を含むが、読み手が捨てる |
| Concordia の一覧 | `GET /v1/delegation/runs?limit=500` (上限) の 1 往復 | local PR を出し終えた run は completed になるので、突合には進行中以外の最近の run も要る。500 より古い run に属する PR は human と判定される (残りに記す) |
| run の対象リポ | `sessions[].repo_origin` の末尾名、無ければ `args.target_repo` の末尾名 | `target_repo` は開発機のパスなので末尾名だけを持ち、パスは DTO にも残さない |
| review の主体 | `#<番号>@<head SHA7>` | §3 の「local PR id + headSha」と §8 の「local PR は番号と headSha7 だけ」を両立する。push で head が変われば新しい出来事 |
| merged | 1 回だけ出し、状態に残さない | 前回の review が消えるので再び出ない。`pending` (日次要約) に終わったものを残さない |
| 取得元ごとの既知 | 届かなかった取得元の既知は前回のまま | 届かない回を「閉じた」と扱うと、復旧後に merged を判定できない (W3 の「取れなければ前回状態不変」と同じ) |
| `outdated --fail-on run` など | 受け付けない (終了 2) | `outdated` は ref だけを見る (§2.2 の表で URL を取るのは `watch` だけ) |
| HTTP の組み込み | `ergo_http` があるときだけ (`ERGO_BUILD_HTTP=ON`)。無ければ URL を渡しても `unavailable` | `ergo_http` は libcurl をネットワークから取得してビルドする任意の部品で、既定 OFF。CLI の既定ビルドを重くしない |
| リダイレクト | 追わない | loopback の外へ出ない |

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_http` (`IHttpClient` / `make_curl_client`) | 採用 | 統合設計 §7「HTTP は既存の `ergo_http`」。CLI のアダプタだけが使い、ライブラリは interface (`IReviewSource` / `IRunSource`) だけを持つ |
| `ergo_package_watch` の `diff` / 状態 / 報告の書き手 / 漏れ検査 | 採用 (拡張) | run / review / merged を同じ `Candidate` に載せ、重複の抑止・報告・漏れ検査 (§8) をそのまま通す。tag / track / off-main の JSON の形は変えない |
| `ergo_common::jsonm` | 採用 | 一覧応答の読み取り |
| `repository_name` (W3 の `watch_dependency.h`) | 不採用 (`repo_key` を足した) | `repository_name` は `/` 区切りの URL 用で大文字小文字を保つ。Revisor の `LUDIARS/Pictor`・Windows のパス・`git@host:x.git` を同じ鍵に揃える比較用の関数が別に要る |
| Revisor / Concordia の TypeScript / JS クライアント | 不採用 | Ergo の C++ CLI から使えない。応答の形 (`summaryProjection`、`serializeRunForList`) だけを見本に写した |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `ergo_package_watch` / `test_package_watch` / `ergo_pkg_cli` をビルド (既定の `ERGO_BUILD_HTTP=OFF`) | 成功 (警告は既存の `json_min.h` の C4819 のみ) |
| HTTP アダプタ (`http_json_get` / `revisor_review_source` / `concordia_run_source` / `provenance_sources` を `ERGO_PKG_CLI_WITH_HTTP=1` で) | MSVC `/W4` でコンパイルが通ることを確認 (警告なし)。`ERGO_BUILD_HTTP=ON` の全体ビルド (libcurl の取得とリンク) は未実施 |
| GCC でのビルド | 未実施 |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない。Revisor の審査に任せる) |
| 実際の Revisor / Concordia に対する `watch` | 未実施 (テストはネットワークを使わない。サービスへの接続確認は人の許可のもとで行う) |
| 秘匿語の辞書照合・絶対パス | 提出前に差分で 0 件を確認 |

## 復旧

- 由来だけ止める: `--revisor-url` / `--concordia-url` と環境変数を渡さない (W3 と同じ動き、`sources.*` は `off`)。
- 前回状態の review / run の既知は、消せば次の回に新規として出る (merged は前回の review が無いので出ない)。
- tag / track / off-main の JSON の形は W3 から変わらない。報告と `events.json` に `sources` が足されただけ。

## 残り (W4 の外)

- `pin-behind-ergo` (§3 の表にあるが W4 の受入に無い)。Ergo のマニフェストの main を読む経路が要る
- Pf 側で `triggered_by: "praeforma"` を定数にして spec に書く (W-9、Praeforma の PR)
- 500 件より古い run に属する open な PR は human と判定される。Concordia に run を子セッション id で引く API があれば突合を広げられる
- W5 (構成時の 1 行と Excubitor の配送)。Excubitor のジョブが `--revisor-url` / `--concordia-url` を catalog から渡す
- `ERGO_BUILD_HTTP=ON` の CI ジョブ (libcurl の取得込み) で CLI のリンクまで確かめる

## 提出記録

- 提出: Revisor local PR (ブランチ `feat/package-manager-w4`、base `main`)。GitHub PR は作らない
