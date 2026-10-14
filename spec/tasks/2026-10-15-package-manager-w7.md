# パッケージマネージャ W7 — Unity パッケージ

- 日付: 2026-10-15
- タスク参照: `actio:fab29c6a-34b2-40cc-ae41-31f946b761ac` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §6 / §7 の W7 行と受入条件 / §8 / §9 (W-8)
- 前提: W3 ([`2026-10-15-package-manager-w3.md`](2026-10-15-package-manager-w3.md)、モジュール [`spec/module/package_watch.md`](../module/package_watch.md)、形式 [`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md))
- ブランチ: `feat/package-manager-w7`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | UPM ホストの読み手: Unity プロジェクト (`ergo-packages.json` が無く `Packages/manifest.json` がある) の LUDIARS の git 依存を、更新を確かめる依存にする。持ち主 `upm`、コードは Ergo のマニフェストから、`next` は Unity の窓 | `include/ergo/package_watch/upm_dependency.h`、`src/package_watch/upm_dependency.cpp`、`detect.cpp` の `next` |
| 2 | `ergo_pkg watch` / `outdated` が UPM ホストを読む (コードは `.ergo/host.json`)。UPM の宣言の読み込みを `list` と共有する部品に切り出す | `tools/package_cli/upm_host.*`、`watch_host.cpp`、`cmd_list.cpp` |
| 3 | Unity パッケージ `unity/com.ludiars.ergo.packages` (UPM、Editor 専用、版 `0.1.0`)。Core (純 C#): JSON 読み手、`UpmManifestReader`、`PackagesLockReader`、`WatchReportReader`、`UpdateStatus`、`PinPlanner`、`ManifestJsonEditor`、`EventInbox`、`PackageOverview`。Editor: `ErgoPackagesWindow`、`UpdateNotifier`、`UpmApplier`。全資産に .meta | `unity/com.ludiars.ergo.packages/` |
| 4 | 見本: UPM ホストの manifest / host.json / packages-lock と報告の見本。Unity の Fixtures はバイト一致の写し | `tests/package_watch/fixtures/upm-*.json`・`watch-report.json`、`unity/.../Tests/Editor/Fixtures/` |
| 5 | テスト: C++ (`PackageWatchUpm.*`)、CLI の通し (UPM ホスト)、構造検査 (`cmake -P`)、Core の NUnit (EditMode と `dotnet test` で同じソース) | `tests/package_watch/test_upm.cpp`、`tests/packages/test_package_watch_cli.cmake`、`tests/packages/test_unity_package.cmake`、`unity/.../Tests/Editor/*.cs`、`tests/unity_packages/` |
| 6 | CI: ctest に構造検査が入る (既存ジョブ)、Core の `dotnet test` を 1 ジョブ足す | `.github/workflows/ci.yml` (`unity-package-core`) |
| 7 | 文書・登録: モジュール定義・形式・統合設計 (§6.1 の依存、実装状況)・モジュール一覧・README・ドメイン・層 | `spec/module/package_watch.md`、`spec/data/ergo-watch-report.md`、`spec/feature/package-manager.md`、`module_list.md` / `.yaml`、`README.md`、`spec/domains/build-packages.domain.json`、`.anatomia/layers.json` |

## 受け入れ条件 (契約書式)

- C-1 test_unity_package_structure: `unity/.../Tests/Editor/Fixtures/*.json` が `tests/package_watch/fixtures/*.json` と同じ集合でバイト一致 — `test_unity_package_structure`
- C-2 UpmManifestReader.Read(manifest): manifest.json の git 依存を読み (`git+` と `?path=` を落とした取得元・`#` の後の rev)、`#main` などブランチ指定に警告、レジストリと `file:` は一覧に出すが判定しない — `UpmManifestReaderTests`
- C-3 PinPlanner.Plan(current, rev): ブランチ名・タグ名・短縮 SHA を拒否し、40 桁 SHA なら `?path=` を保った `<url>#<sha>` を作る — `PinPlannerTests`
- C-4 ManifestJsonEditor.TrySetDependency(text, name, value): `dependencies.<name>` の 1 値だけを変え、前後のテキスト (改行・字下げ・他の行) は同じ — `ManifestJsonEditorTests`
- C-5 UpdateStatus.Of(report, name, now): `observed:false`・報告が無い・古い報告を「更新なし (up to date)」にしない — `WatchReportReaderTests` (NotObservedIsNeverUpToDate ほか)、`OverviewAndInboxTests`
- C-6 EventInbox: 未読数を数え、同じ出来事 id の通知は 1 回だけ — `OverviewAndInboxTests`
- C-7 upm_dependencies / `ergo_pkg watch`: UPM ホスト見本から出来事を出す (持ち主 `upm`、`next` は Unity の窓、URL を載せない、`#main` は見ない) — `test_package_watch` (PackageWatchUpm.*)、`test_package_watch_cli` (W7 の節)
- C-8 報告見本 `watch-report.json` が書き手 (`host_report_json`) の出力と同じキーを持つ — `test_package_watch` (PackageWatchUpm.ReportFixtureHasWriterKeys)
- C-9 構造: `package.json` の必須キー、asmdef が Editor 専用、Core が `noEngineReferences`、`Runtime/` が無い、全資産に .meta — `test_unity_package_structure`

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / C# / CMake は対象にならない (W1〜W3 と同じ)。上の受入条件は ctest と NUnit のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_package_manager` の `read_upm_dependencies` / `read_upm_host_code` (W1、`list` 用) | 採用 | UPM ホストの宣言の読み取りはそのまま使う。LUDIARS の判定 (`LUDIARS/` を含む) も同じ規則を C# 側に持つ |
| `cmd_list.cpp` の `load_upm` (W1) | 採用 (切り出し) | `watch` も同じ読み込みが要るので `tools/package_cli/upm_host.*` に移して共有した |
| `ergo_package_watch` の `repository_name` / 観測・判定・`diff`・報告 (W3) | 採用 | UPM の依存も `WatchDependency` にすれば判定・重複抑止・漏れ検査はそのまま通る。変えたのは `next` の文言だけ |
| Tela の `unity/com.ludiars.tela` | 形だけ採用 | 「所有リポの `unity/` に置く」形と package.json / asmdef の書き方。Tela は .meta を持たないが、git で入れる UPM パッケージは不変フォルダで .meta の無い資産が無視されるので、こちらは全資産に .meta を置いた |
| `JsonUtility` (`com.unity.modules.jsonserialize`) | 不採用 | Core は `noEngineReferences` (dotnet でテストするため) で使えない。`null` を 0 と区別できず、値の位置も返さないので `ManifestJsonEditor` に使えない。Core に最小の `JsonParser` を置いた (統合設計 §6.1 を更新) |
| C# の JSON ライブラリ (Newtonsoft など) | 不採用 | 新しい依存を足さない (package.json の依存は無し) |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `ergo_package_watch` / `ergo_pkg_cli` / `test_package_watch` / `test_package_manager` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ)。`test_package_watch` / `test_unity_package_structure` / `test_package_watch_cli` が ctest に登録されることを `ctest -N` で確認 |
| `dotnet build tests/unity_packages` (Core と NUnit テストのコンパイル、.NET 8 SDK) | 成功 (警告 0・エラー 0)。Editor 側 (`UnityEditor` を使う窓・通知・適用) は Unity が無いのでコンパイル未確認 |
| テストの実行 (ctest・`cmake -P`・`dotnet test`・Unity の EditMode) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない。Revisor の審査と CI に任せる) |
| Unity エディタでの手元確認 (UnityFoundation に git URL で入れ、窓が開く・報告が出る・pin の書き換え後に Resolve される) | 未実施 (Unity の実機確認はしない指示) |
| 秘匿語の辞書照合・絶対パス | 差分の追加行で 0 件 (絶対パスの検出は構造検査の正規表現の 1 行だけ) |

## 復旧

- Unity パッケージは利用側が manifest.json に入れたときだけ効く。外せば元どおり (Ergo のビルドには関係しない)。
- UPM ホストの読み手は `ergo-packages.json` が無く `Packages/manifest.json` があるディレクトリにだけ効く。既存のホストの判定は変わらない
  (`next` の文言が変わるのは持ち主 `upm` の依存だけ)。
- 構造検査が邪魔なら `test_unity_package_structure` を外せる (C++ のビルドには関係しない)。

## 残り (W7 の外)

- Unity エディタでの手元確認 (人の許可のもとで行う)
- `ergo_pkg init --upm` は `.gitignore` に `.ergo/watch-report.json` を足さない (W1 の仕様どおり `.ergo/host.json` だけ書く)。
  Unity プロジェクト側で無視する設定を足すかは W1 の `init` の改訂として別に決める
- `outdated` / `list` の UPM ホスト表示の作り込み (今は `watch` / `outdated` が同じ読み手を使うだけ)

## 提出記録

- 提出: Revisor local PR (ブランチ `feat/package-manager-w7`、base `main`)。GitHub PR は作らない
