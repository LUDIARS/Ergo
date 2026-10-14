# パッケージマネージャ — ホストの宣言を構成の既定値に流す (L2 / L3 の前提)

- 日付: 2026-10-15
- タスク参照: `actio:56d2eb46-bbfc-4047-bc97-69d285739966` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §2.1 / §5.2 (L1〜L3 の確認欄) / §7 / §8
- 前提: W1 (`2026-10-14-package-manager-w1.md`) の「残り (W1 の外)」の 2 項目
- ブランチ: `feat/package-manager-host-use`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 構成時の読み手が `host.use` / `optional` / `features` / `legacy_vars` / `host.ergo` を読む | `cmake/packages/ErgoPackageManifest.cmake` |
| 2 | 既定値の決め方 (空でないキャッシュ値が勝つ、空なら宣言)、機能指定の分解、`<package>.source_dir` の別名、optional の記録 (純粋関数) | `cmake/packages/ErgoPackageHostDefaults.cmake` (新規) |
| 3 | ハブへの配線: 選択・optional・機能の既定値、使わなかった optional を選択から外す、出所を resolved.json の `host` に | `cmake/packages/ErgoPackages.cmake`、`ErgoPackageResolve.cmake`、`ErgoPackageReport.cmake` |
| 4 | Ergo 自身への PKG-02/04/05/07/08/09 (`host.ergo.rev` と Ergo のチェックアウト)。resolved.json の `host.ergo` に pin / head / 経路 / drift / dirty | `cmake/packages/ErgoPackageErgoPin.cmake` (新規) |
| 5 | 最終判定の場所: ハブを include したのがサブディレクトリ (ブートストラップ) なら、トップの末尾で行う。モジュールの理由変数は Ergo のディレクトリの末尾で控える | `cmake/packages/ErgoPackages.cmake` |
| 6 | L3 の `PICTOR_ENABLE_RIVE` を `features` で渡せるよう、Pictor に機能 `rive` (既定 off、desktop、`cache: PICTOR_ENABLE_RIVE=ON`) | `ergo-packages.json`、`tests/packages/test_manifest.cmake` |
| 7 | テスト | `tests/packages/test_host_defaults.cmake` (`cmake -P`)、`tests/packages/test_host_use.cmake` (偽 Ergo + 偽ホスト、git)、`tests/packages/fixtures/use_{ergo,host}/` |
| 8 | 文書 | `spec/data/ergo-packages.md`、`spec/setup/packages.md`、`spec/feature/package-manager.md` (実装状況・L3) |

## 受け入れ条件 (契約書式)

- C-1 ergo_pkg_host_default(cache, declaration): 空でないキャッシュ値が勝ち (`cache`)、空なら宣言 (`declaration`)、宣言も値も無ければ `none` — `test_packages_host_defaults`
- C-2 ergo_pkg_manifest_load(... HOST): `use` / `optional` / `features` (`+a,-b` と `a;b`) / `legacy_vars` / `host.ergo` を読む — `test_packages_host_defaults`
- C-3 L2 (偽ホスト、`use`/`optional` に alpha、Ergo の後で作り `ERGO_PACKAGE_PROVIDED_ALPHA=ON`): `mode=M1 (alpha=…/provided)`、PKG-05 (behind) と PKG-15 (ホストの FORCE) が記録される — `test_packages_host_use` (l2)
- C-4 optional: 見つからない (provided で作られない場合を含む) と PKG-03 の record を残して使わず `host.unused` に載る。optional でなければ PKG-03 の error — `test_packages_host_use` (l2_absent*, l2_required, l2_optional_cache)
- C-5 L3 (`features: {"alpha": "-gpu"}`、ソースはハブ): `mode=M2 (alpha=…/source)`、機能が宣言どおり、PKG-15 が出ない。`legacy_vars` の `alpha.source_dir` が別名として効く — `test_packages_host_use` (l3, l3_legacy)
- C-6 上書き: キャッシュの `ERGO_PACKAGES` / `ERGO_PACKAGES_OPTIONAL` / `ERGO_PACKAGE_FEATURES_<NAME>` が宣言に勝ち、resolved.json の `host.selected_from` / `optional_from` / `features_from` に `cache` が残る — `test_packages_host_use` (override_*)
- C-7 ergo_pkg_check_ergo_pin: 宣言に Ergo の pin があると resolved.json の `host.ergo` に pin / head / route / drift を出し、drift none / ahead / behind / diverged / unknown を区別、PKG-05 / PKG-09 を記録、dirty は PKG-07、STRICT で止まる — `test_packages_host_use` (l1*, ergo_strict, ergo_dirty)
- C-8 resolved.json: 宣言が無ければ `host` は `{ "id" }` のまま (同じバイト列)、宣言があれば出所と `ergo` が載り決定的 — `test_packages_host_defaults`、既存の `test_packages_report` / `test_packages_host_configure`
- C-9 M0 の結果不変 — `test_packages_m0_invariance` (無変更で継続)

Augur の契約 (`augur.contracts.json` と述語モジュール) は W1 と同じ理由で用意しない (`contract-wrap` は TypeScript の関数を
包む仕組みで、CMake は対象にならない)。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ErgoPackageManifest.cmake` の `HOST` 読み (W1) | 採用・拡張 | 宣言を読む場所は 1 つにし、`host` 節の残りの項目を同じ関数で読む |
| `ErgoPackageRulesAcquire.cmake` の drift / dirty / fetched head / expect_tag の規則と `ErgoPackageGit.cmake` の probe | 採用 | Ergo 自身にも link パッケージと同じ規則を同じ重さで当てる (新しい規則 ID は作らない) |
| 経路 1 の `provided` と終わりの照合 (`ergo_pkg_resolve_late`) | 採用 | L2 の「Ergo より後に作る」はこの経路。最終判定の場所だけトップへ移した |
| C++ `ergo_package_manager` の宣言検査 | 変更なし | `use` / `optional` / `features` の書式と名前の検査は W1 で `ergo_pkg doctor` が持っている。構成時は書式違いを PKG-13 / PKG-01 で拾う |
| resolved.json の `packages` に `ergo` を足す案 | 不採用 | `packages` は Ergo のマニフェストの項目 (kind link / tool) で、C++ の読み手 (`package_model`) と図がそれを前提にする。Ergo 自身はホストの宣言の項目なので `host.ergo` に置く |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64) で Ergo を構成・ビルド | 下の「提出時の確認」に記す |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、ctest の実行は Revisor の審査に任せる) |
| 秘匿語の辞書照合・絶対パス | 提出前に差分で 0 件を確認 |

## 復旧

- 既定値の流し込みを止める: キャッシュに値を入れる (空でない値が常に勝つ)。宣言ごと外すには `ERGO_PACKAGE_MANIFEST_EXTRA` で
  `use` の空の宣言を指す。宣言の無いホストの挙動は変わらない。
- 最終判定の場所の変更・`host.ergo` の記録・機能 `rive` は、この PR の revert で元に戻る。

## 残り

- Ludellus-Native 側の L2 / L3 の PR (Ludellus-Native のリポ)。L1 の「resolved.json に Ergo の drift が出る」は `host.ergo` で満たす
- `ergo_pkg list` / P4 (W6) が `host.ergo` と `host.unused` を読むか (W6 の PR と合わせる)
