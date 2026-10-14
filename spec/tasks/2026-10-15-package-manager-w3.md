# パッケージマネージャ W3 — 更新の検知

- 日付: 2026-10-15
- タスク参照: `actio:67ea2c74-b79c-467c-9c69-32d2f95724aa` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §2.2 (outdated / watch) / §2.3 / §3 / §7 の W3 行と受入条件 / §8
- 前提: W1 ([`2026-10-14-package-manager-w1.md`](2026-10-14-package-manager-w1.md)、モジュール [`spec/module/package_manager.md`](../module/package_manager.md))
- モジュール: [`spec/module/package_watch.md`](../module/package_watch.md)、形式: [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md)
- ブランチ: `feat/package-manager-w3`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 宣言の `track` キー (`packages.<name>.track.branch` / `host.ergo.track.branch`、既定 `main`)。C++ の読み手に足し、CMake の読み手は読まない (出力不変) | `ergo_package_manager` (`ManifestPackage::track_branch` / `ErgoPin::track_branch`)、`spec/data/ergo-packages.md` |
| 2 | `ergo_package_watch`: 依存の列挙、`IRefSource` (ls-remote / 兄弟チェックアウト / ミラー)、ref 一覧の解析と指紋、semver、観測、判定 (tag / track / off-main)、前回状態、`diff` (重複抑止・`track` の 6 時間畳み込み)、報告と出来事の書き手 (漏れ検査つき) | `include/ergo/package_watch/`、`src/package_watch/`、`spec/module/package_watch.md` |
| 3 | `ergo_pkg outdated` / `watch`。git プロセスは CLI のアダプタ (`git_remote_source` / `git_sibling_source` / `ref_source_router`) にだけ置く | `tools/package_cli/` |
| 4 | 報告・出来事・状態の形式 | `spec/data/ergo-watch-report.md` |
| 5 | テスト (偽ソース・偽リポ、ネットワーク無し) | `tests/package_watch/`、`tests/packages/test_track_key.cmake`、`tests/packages/test_package_watch_cli.cmake` |
| 6 | 文書・登録: モジュール一覧・README・ドメイン・層・統合設計の実装状況 | `module_list.md` / `.yaml`、`README.md`、`spec/domains/build-packages.domain.json`、`.anatomia/layers.json`、`spec/feature/package-manager.md` |

## 受け入れ条件 (契約書式)

- C-1 detect_updates(dep, observation): tag / track / off-main を判定する。距離を添え、semver で `expect_tag` より新しいものだけ、複数タグは最新 1 つ — `test_package_watch` (PackageWatchDetect.*)
- C-2 diff(prev, observations, now): 同じ入力で同じバイト列 (入力の並びにもよらない) — `test_package_watch` (PackageWatchDiff.Deterministic)
- C-3 diff(prev, observations, now): 同じ主体の 2 回目で出来事 0、`track` は同じ依存で 6 時間に 1 回へ畳む (距離は通知時点からの差を添える)、pin を上げると状態から消える — `test_package_watch` (PackageWatchDiff.*)
- C-4 observe_dependency / diff: 取得失敗で `observed:false` と理由、その依存の前回状態 (既知集合と指紋) は不変 — `test_package_watch` (PackageWatchObserve.FailureIsNotObserved、PackageWatchDiff.UnobservedKeepsState)
- C-5 observe_dependency(dep, sibling source): 兄弟チェックアウトでは同期 (fetch) を呼ばず `as_of: sibling-last-fetch`、`sibling_behind` を出す — `test_package_watch` (PackageWatchObserve.SiblingDoesNotFetch)、`test_package_watch_cli` (兄弟の実リポで fetch 先を作らない)
- C-6 observe_dependency(dep, source, prev fingerprint): 指紋が同じならミラーを同期しない (偽ソースの呼び出し回数) — `test_package_watch` (PackageWatchObserve.SameFingerprintSkipsSync)
- C-7 prepare_watch_outputs / `ergo_pkg watch`: 報告と出来事が漏れ検査 (§8) を通り、通らない入力では書かずに終了 3 — `test_package_watch` (PackageWatchReport.*)、`test_package_watch_cli`
- C-8 `track` キー: 追加しても CMake の読み手 (`ergo_pkg_manifest_load`) の出力が変わらず、C++ の読み手は読む — `test_packages_track_key`、`test_package_manager` (PackageManagerDeclaration.TrackKey)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / CMake は対象にならない (W1 と同じ)。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_package_manager` (宣言・Ergo のマニフェストの読み手、`is_full_sha`、漏れ検査 `find_leaks`) | 採用 | 統合設計 §7 の向き `ergo_package_watch → ergo_package_manager`。依存の列挙と §8 の検査をそのまま使う |
| `ergo_common::jsonm` | 採用 | 状態・報告・出来事の読み書き。`JsonObject` が `std::map` なので出力のキー順が決まり、`diff` の決定性に使える |
| `tools/package_cli/process.cpp` (W1 の引用規則つき起動) | 採用 (1 関数足す) | ls-remote / show-ref / rev-list / merge-base を同じ規則で起動する。`merge-base --is-ancestor` の 0 / 1 を区別するため、終了コードを返す `capture_status` を足した |
| `ergo_package_scan` / `ergo_package_model` の兄弟チェックアウトの扱い | 不採用 | モジュール間の cross-import 禁止。兄弟の読み方 (`<root>/<Repo>`、`.worktrees` は見ない) は同じ規則を CLI のアダプタに持つ |
| `ergo_http` | 不採用 (W4) | W3 は HTTP を使わない。Revisor / Concordia の読み手は W4 |
| `cmake/packages/ErgoPackageManifest.cmake` | 変更しない | `track` は構成時に要らない。読み手が知らないキーを無視することをテストで固定した |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `ergo_package_manager` / `ergo_package_watch` / `ergo_pkg_cli` / `test_package_manager` / `test_package_watch` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ)。`test_package_watch` / `test_packages_track_key` / `test_package_watch_cli` が ctest に登録されることを `ctest -N` で確認 |
| `-DERGO_BUILD_PACKAGE_WATCH=OFF` で `ergo_pkg_cli` をビルド | 成功 (`outdated` / `watch` は終了 2 を返す分岐) |
| GCC でのビルド | 未実施。`pclose` の終了状態は `WIFEXITED` / `WEXITSTATUS` で読む (`<sys/wait.h>`) |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない。Revisor の審査に任せる) |
| 秘匿語の辞書照合・絶対パス | 提出前に差分で 0 件を確認 |

## 復旧

- CLI の `outdated` / `watch` だけ止める: `-DERGO_BUILD_PACKAGE_WATCH=OFF` (CLI は W1 のコマンドだけで作られる)。
  モジュールは既存のどれからも依存されない。
- `track` キーは C++ の読み手が読むだけで、構成時の挙動は変わらない。
- 前回状態はリポの外 (`--state`、既定は利用者ディレクトリ) にだけ書く。消せば次の回は全てを新規として出す。

## 残り (W3 の外)

- W4: Revisor / Concordia の読み手 (`IReviewSource` / `IRunSource`)、`run` / `review` / `merged`、由来 (pf / ai / human / unknown)
- W5: 構成時の 1 行と Excubitor の配送ジョブ (日次要約は報告の `pending` を使う)
- `pin-behind-ergo` (§3 の表にあるが W3 の受入に無い)。Ergo のマニフェストの main を読む経路が要るので W4 以降で入れる
- UPM ホストの読み手 (W7)
- ミラーの作成 (`git clone --bare --filter=tree:0`) は `watch` では行わない。あれば同期して距離に使い、無ければ距離を空欄にする

## 提出記録

- 提出: Revisor local PR (ブランチ `feat/package-manager-w3`、base `main`)。GitHub PR は作らない
