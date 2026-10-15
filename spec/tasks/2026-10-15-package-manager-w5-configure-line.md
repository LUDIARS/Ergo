# パッケージマネージャ W5 — 構成時の 1 行 (Ergo 側)

- 日付: 2026-10-15
- タスク参照: `actio:54c4d314-c38c-43f0-8973-057351f6b712` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §4 通知の表「構成時の 1 行」/ §7 の W5 行と受入条件 (Ergo 側)
- 前提: W3 ([`2026-10-15-package-manager-w3.md`](2026-10-15-package-manager-w3.md)) の状態ファイル (形式 [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md))
- ブランチ: `feat/package-manager-w5-configure-line`
- 範囲外: Excubitor のジョブ (W5 の配送側、Excubitor の PR)

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 状態ファイルを読んで 1 行を作る純関数と、`ergo_pkg watch` と同じ既定の状態ディレクトリ | `cmake/packages/ErgoPackageWatchNotice.cmake` (新規) |
| 2 | 構成の final 段で 1 行を出す (ホストのコードは resolved.json の `host.id` と同じもの) | `cmake/packages/ErgoPackages.cmake` |
| 3 | テスト (`cmake -P`、ネットワーク無し) | `tests/packages/test_watch_notice.cmake`、`CMakeLists.txt` (`test_packages_watch_notice`) |
| 4 | 文書 | 本ファイル、`spec/feature/package-manager.md` (実装状況) |

## 受け入れ条件 (契約書式)

- C-1 ergo_pkg_watch_notice(state): 状態ファイルが無いとき何も出さない — `test_packages_watch_notice` (no state file gives no line)
- C-2 ergo_pkg_watch_notice(state): 壊れている (途中で切れた JSON・空・配列・schema 違い・entries が配列でない・ディレクトリ) とき何も出さない — 同 (a broken state file gives no line)
- C-3 ergo_pkg_watch_notice(state): 別ホストの状態 (host が違う・無い、コードとして不正) のとき何も出さない — 同 (another host's state gives no line)
- C-4 ergo_pkg_watch_notice(state): ホストの状態に N 件あれば `ergo packages: N update(s) known for host <code>` — 同 (a state of the host gives the count)
- C-5 ergo_pkg_watch_default_state_dir(): `ergo_pkg watch` の既定 (`watch_paths.cpp`) と同じ場所を見る — 同 (default state directory follows ergo_pkg watch)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、CMake は対象にならない (W1〜W4・W6 と同じ)。上の受入条件は ctest のテスト名で裏づける。

## 判断

- **CMake だけで読む**: 構成時にはまだ `ergo_pkg` が無いので、`string(JSON)` で状態ファイルを直接読む (CMake 3.20 で使える)。
  読むのはローカルの 1 ファイルだけで、ネットワークにも git にも出ない。
- **件数は `entries` の数**: 状態の `entries` は未解消の既知の更新 (`pending`) の集合 (§3)。`entries` が無いときは 0 件。
- **出さない条件は全て黙る**: 無い・壊れている・別ホスト・コードが不正のどれも、警告も出さず構成を止めない (§7 W5)。
  別ホストは「ファイルの `host` がこのホストのコードと違う」で判定する。
- **ホストのコード**: resolved.json の `host.id` と同じ (host.code → `ERGO_PACKAGES_HOST_ID` → プロジェクト名)。
  `ergo_pkg watch` は host.code が無いとディレクトリ名を使うので、宣言に code を書いていないホストでは一致しないことがあるが、
  その場合は 1 行が出ないだけ。
- **出す位置**: final 段の要約 (`ergo packages: mode=...`) と警告の後、エラーで止める前。include 段では出さない (2 回出さない)。
- **状態ディレクトリは既定だけ**: `ergo_pkg watch --state` に当たる上書きは足さない (タスクの範囲)。
- **再利用**: C++ の `read_state` (`ergo_package_watch`) は構成時に使えないので再利用しない。既定ディレクトリの規則は
  `tools/package_cli/watch_paths.cpp` と同じものを CMake で書いた。

## 検証

- テストは実行していない (委託の指示でテスト実行は明示指示があるときだけ)。`test_packages_watch_notice` は `cmake -P` で動く。
