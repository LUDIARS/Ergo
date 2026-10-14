# ergo_pkg status の状態ファイル `ergo-packages.status.json` (schema 1)

- 正本の設計: [`spec/feature/package-manager.md`](../feature/package-manager.md) §5.1 / §8 / §9 (W-6)
- 書く側: `ergo_pkg status` (`ergo_package_manager` の `host_status.h` / `test_results.h`、 [`spec/module/package_manager.md`](../module/package_manager.md))
- 読む側: P4 ホスト状態図 (`ergo_package_model` の `host_state_reader.h` → `ergo_package_viz --view host`、 [`spec/module/package_viz.md`](../module/package_viz.md))

置き場所は `<build>/ergo-packages.status.json` (`--out` で変えられる)。 ビルドディレクトリの中なので追跡しない。
JSON はキーを辞書順に並べた 1 行 (改行 1 つで終わる)。 同じ入力なら同じバイト列。 書く時刻は載せない。

## 載せないもの (§8)

パス (ビルドディレクトリ・JUnit のファイル・ホストの場所)、 テスト名、 テストの出力。 書く前に ergo_package_manager の
漏れ検査を当て、 当たれば **何も書かずに** 終了 3。

## キー

| キー | 型 | 意味 |
|---|---|---|
| `schema` | `1` | 違えば読み手は「無い」と同じに扱う (図の札は `—`) |
| `host` | 文字列 | ホストのコード。 ホストの宣言の `host.code` → `<build>/ergo-packages.resolved.json` の `host.id` → `""` |
| `head` | 文字列 | ホストの `git rev-parse HEAD` (40 桁)。 取れなければ `""` |
| `configured_at` | 文字列 | `<build>/CMakeCache.txt` の更新時刻 `YYYY-MM-DDTHH:MM:SSZ` (UTC)。 無ければ `""` |
| `tests` | オブジェクト | 下表 |

`tests`:

| キー | 型 | 意味 |
|---|---|---|
| `source` | `"junit"` / `"last-test-log"` / `"unknown"` | 数の出所。 `--junit <xml>` (ctest の `--output-junit`) を優先し、 無ければ `<build>/Testing/Temporary/LastTest.log`、 どちらも無い・数えられなければ `unknown` (未実行) |
| `total` / `passed` / `failed` / `skipped` | 数 | `source` が `unknown` のときは書かない。 `failed` は failure と error の和、 `skipped` は ctest の Not Run / Disabled |

数え方:

- **JUnit**: `<testcase>` を 1 件と数える。 子に `<failure>` / `<error>` があるか `status="fail"` なら失敗、 子に `<skipped>` があるか
  `status="notrun"` / `"disabled"` なら未実行、 それ以外は成功。 `<testcase>` が 1 つも無ければ数えられない (明示した `--junit` なら終了 2)
- **LastTest.log**: 行頭が `Test Passed.` の行を成功、 `Test Failed` を失敗、 `Test Not Run` を未実行と数える (テストの出力の中の字下げした行は数えない)

例 (見本 `tests/package_model/fixtures/host_state/ergo-packages.status.json`、 `tests/package_manager/fixtures/status/ctest-junit.xml` から作る):

```json
{"configured_at":"2026-10-15T01:00:00Z","head":"1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d","host":"Example","schema":1,"tests":{"failed":1,"passed":3,"skipped":1,"source":"junit","total":5}}
```

## 初版で取らないもの (§9 W-6)

ビルドの成否と CI の結果。 足すときは `tests` と並ぶキーにし、 schema は上げない (読み手は知らないキーを無視する)。
