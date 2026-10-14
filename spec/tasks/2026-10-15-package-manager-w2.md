# パッケージマネージャ W2 — 書き換え

- 日付: 2026-10-15
- タスク参照: `actio:31b6bdc3-6666-4077-a5fc-27e3e51b167d` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §2.2 (add / remove / update) / §7 の W2 行と受入条件 / §8 / §9
- 前提: W1 ([`2026-10-14-package-manager-w1.md`](2026-10-14-package-manager-w1.md)、モジュール [`spec/module/package_manager.md`](../module/package_manager.md))
- ブランチ: `feat/package-manager-w2`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 書式を保つ JSON の書き換え (値の範囲だけを差し替え、足すときは兄弟の区切りを写す、CRLF を保つ) | `include/ergo/package_manager/json_edit.h`、`src/package_manager/{json_scan,json_edit}.*` |
| 2 | 持ち主の判定 (`ergo` と非公開はホスト、共有は Ergo のマニフェスト、同名は Conflict) | `package_owner.h` / `.cpp` |
| 3 | 版の解決 (タグ / main / SHA / ブランチ、STRICT の警告、off-main の拒否と `allow_off_main`)。git は `IRevResolver` 越し | `rev_resolution.h` / `.cpp` |
| 4 | 宣言の書き換えの計画 (`plan_add` / `plan_remove` / `plan_pin`)。書き換え後に宣言を検査し直し、新しい error を拒む。`ErgoManifest` に `modules.*.needs` を読む | `declaration_edit.h`、`src/package_manager/declaration_{edit,add,remove}.cpp`、`ergo_manifest.*` |
| 5 | CLI `add` / `remove` / `update`、git のツリーレス bare ミラーでの `IRevResolver` 実装、終了コードの取得 | `tools/package_cli/{cmd_add,cmd_remove,cmd_update,edit_support,git_rev_resolver}.*`、`cli_args.*`、`process.*`、`main.cpp`、`commands.h` |
| 6 | テスト (偽ソース・偽リポ、ネットワーク無し) | `tests/package_manager/test_{json_edit,declaration_edit,rev_resolution}.cpp`、`tests/packages/test_package_cli_edit.cmake` |
| 7 | 文書・登録 | `CMakeLists.txt`、`module_list.md` / `.yaml`、`README.md`、`spec/module/package_manager.md`、`spec/setup/packages.md`、`spec/feature/package-manager.md` (実装状況) |

新しいモジュールとトップレベルのディレクトリは無い (W1 の `ergo_package_manager` と `tools/package_cli/` に足した)。
`spec/domains/build-packages.domain.json` と `.anatomia/layers.json` は既存のパターンで新しいファイルを含むので変更しない。

## 実装内容

- `update <name> --to <tag|main|sha|branch>`: 持ち主のファイルの `rev` と `expect_tag` だけを書く。タグでなければ
  `expect_tag` を外し、STRICT で通らない旨を警告する。main の祖先でない commit は `--allow-off-main` が無ければ終了 2。
  共有パッケージは `--ergo` を明示したときだけ Ergo のマニフェストを書く (明示が無ければ終了 2)。`ergo` と非公開は
  ホストの宣言 (ホストの宣言でない `--host` は終了 2)。終わりに STRICT で通るかと、どのリポで feat ブランチと PR を
  作るかを表示する (コミットはしない)。
- `add <name>`: `host.use` (と `--optional` / `--features`) に足す。Ergo のマニフェストに無い名前は非公開 `packages` に
  足し、`--repository` と `--rev` を要る。`--rev` がタグ・ブランチならミラーで解決し、40 桁ならネットワークに出ない。
  link は `cmake.wiring` (既定 `cmake/packages/<name>.cmake`) を書き、その結線ファイルがホストに無ければ終了 2 で
  ファイルを変えない (結線は生成しない)。
- `remove <name>`: `host.use` / `optional` / `features`、非公開なら `packages` とそれを指す `legacy_vars` から外す。
  Ergo の `modules.*.needs` にある名前は終了 2 (ファイル不変)。
- 版の解決のミラーは `<cache>/mirrors/<Repo>.git` (`git clone --bare --filter=tree:0`、2 回目からは fetch)。
  `<cache>` は `--cache` → `ERGO_PKG_CACHE_DIR` → `ERGO_DEPENDENCY_CACHE_DIR` → 一時ディレクトリの `ergo_pkg` (§2.3 / §8: リポの外)。

### 変更した境界

- ライブラリ `ergo_package_manager` はプロセスを起こさない (git は CLI の `GitRevResolver`)。向きは
  `tools/package_cli → ergo_package_manager → ergo_common` のまま。
- 追跡ブランチは `main` 固定 (`track.branch` は W3 で足す)。
- 構成時 (CMake) の挙動は変えない。

### 復旧方法

- この PR を revert すれば W1 の状態に戻る。構成時の変化は無い。
- CLI だけ止める: `-DERGO_BUILD_PACKAGE_CLI=OFF`。書き換えたファイルは git の差分なので、ホスト / Ergo のチェックアウトで
  `git checkout -- ergo-packages.json` で戻せる (CLI はコミットしない)。
- ミラーは `<cache>/mirrors/` を消せばよい (次の update で作り直す)。

### 再利用探索

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_common::jsonm` で読み直して `serialize` | 不採用 | キー順・改行・字下げが変わる (§2.2 の「該当の値だけ」に反する)。範囲を探す走査器を足し、解析の正しさの確認と宣言の検査には jsonm を使う |
| W1 の `read_host_declaration` / `check_declaration` | 採用 | 書き換え後の宣言を検査し直し、新しい error (機能指定の書式・PKG-02・PKG-15 など) を出す書き換えを拒む |
| W1 の `ErgoManifest` | 拡張して採用 | `modules.*.needs` を足した (remove の拒否に要る) |
| W1 の `tools/package_cli/process.*` | 拡張して採用 | 終了コードを返す `status` を足した (`merge-base --is-ancestor` の 0 / 1 と失敗を分けるため) |
| `ergo_package_model` の JSON 部品 | 不採用 | モジュール間の cross-import 禁止 |
| W3 の `IRefSource` (ls-remote・ミラー・兄弟の読み手) | 不採用 | 並行で実装中で未マージ。W2 は祖先判定とタグの解決だけが要るので、別名の `IRevResolver` を package_manager に置いた。W3 で読み手が揃ったら共通化を検討する |

## 受け入れ条件

- C-1 plan_pin(text, pin, rev, tag): `update --to <tag>` が `rev` と `expect_tag` だけを書き、他はバイト単位で同じ — `test_package_manager` (PackageManagerPin.*)、`test_package_cli_edit`
- C-2 owner_of / `ergo_pkg update`: 共有パッケージは `--ergo` 側、`ergo` と非公開はホスト側。取り違えで終了 2 — `test_package_manager` (PackageManagerOwner.*)、`test_package_cli_edit`
- C-3 resolve_rev(request, resolver): feature ブランチ / main の祖先でない SHA を拒否し、`--allow-off-main` で通る — `test_package_manager` (PackageManagerRevResolution.OffMainIsRefusedUnlessAllowed)、`test_package_cli_edit`
- C-4 resolve_rev / `ergo_pkg update --to main`: STRICT の警告 — `test_package_manager` (PackageManagerRevResolution.MainAndShaWarnAboutStrict)、`test_package_cli_edit`
- C-5 plan_remove(text, ergo, name): `modules.*.needs` を壊すと拒否 (ファイル不変) — `test_package_manager` (PackageManagerRemove.*)、`test_package_cli_edit`
- C-6 plan_add / `ergo_pkg add --kind link`: 結線ファイルが無ければ終了 2 でファイル不変 — `test_package_manager` (PackageManagerAdd.PrivateLinkNeedsItsWiringFile)、`test_package_cli_edit`
- C-7 json_edit: 書式を保つ (兄弟の区切りの写し、空のコンテナ、CRLF、文字列中の括弧) — `test_package_manager` (PackageManagerJsonEdit.*)
- C-8 W1 の受入条件は変えない — `test_package_cli` / `test_package_manager` の W1 部分は無変更

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / CMake は対象にならない (W1 と同じ)。上の受入条件は ctest のテスト名で裏づける。

### 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `ergo_package_manager` / `ergo_pkg_cli` / `test_package_manager` をビルド | 成功 (新しい警告なし) |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、ctest の実行は Revisor 審査に任せる) |
| GCC でのビルド | 未実施。`<sys/wait.h>` の `WEXITSTATUS` は POSIX だけで使い、Windows は `system` の戻り値をそのまま使う |
| 秘匿語の辞書照合・絶対パス | 提出前に差分で 0 件を確認 |

## 残り (W2 の外)

- `track.branch` (追跡ブランチの可変) は W3。W2 の `update` は `main` を追跡ブランチとみなす
- `add` の後に `fetch` を自動では走らせない (明示の `ergo_pkg fetch`)
- W1 の残り (`use` / `optional` / `features` の構成既定値への流し込み、Ergo 自身への PKG-04/05/07) は変えない
