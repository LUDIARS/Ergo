# 設計: パッケージマネージャ・更新の定期確認と通知・Ludellus-Native の状態確認・Unity パッケージ (Fable 案)

- 日付: 2026-10-14
- 分類: plan (設計書。実装はしない)
- 依頼: `spec/tasks/2026-10-14-package-manager-watch-design-request.md` (5 章 1〜8 をすべて扱う)
- 対象: Ergo (Eg)。表示は Tela (Tl)。通知の配送は Excubitor (Ex)。Unity 側は Ergo 内の UPM パッケージ
- 前提: `spec/feature/package-hub.md` (統合設計、S1〜S3 実装済み)。本書はその **続き** であり、
  S1〜S3 で決めたこと (マニフェスト = ロック、40 桁 SHA、6 経路、STRICT、PKG-xx、P1/P2) は変えない
- 立場: 二本立ての「Fable」側。もう一方の案は見ていない

---

## 1. 結論

1. **パッケージマネージャは「マニフェストを安全に編集・照会する一本の CLI」に留める。** 新 CLI `ergo_pkg`
   (C++、`tools/package_cli/`、S3 の `ergo_package_viz` と同じ切り方) が `list / outdated / verify / fetch /
   update / add / remove / watch / status` を持つ。**別のロックファイルは作らない** (`ergo-packages.json` の `rev` が
   ロック、S1 で決定済み)。`update` は Ergo の checkout 内でマニフェストの `rev` / `expect_tag` を書き換える
   だけで、取り込みは従来どおり構成時の 6 経路に任せる。これで N7 (pin の更新はホスト側の人が Ergo に PR)
   と STRICT のタグ要求 (N4) を崩さない。
2. **「更新」は 4 種に分けて検知する**: (a) 新しいタグ、(b) 追跡ブランチ (既定 `main`) が pin より進んだ、
   (c) Revisor で審査中のローカル PR、(d) Concordia の委託 run が進行中。(a)(b) は `git ls-remote` だけで
   取れ、認証不要 (公開リポ)。(c)(d) は loopback の Revisor / Concordia を読むだけ。
   **Praeforma はブランチを作らない** (§4.2 で確認)。「Pf 由来」という区別は成り立たず、AI 由来は
   「Concordia の run / Revisor の local PR として観測できるもの」と定義し直す。
3. **定期実行の担い手は Ergo ではなく Excubitor の既存の日次ジョブ** (`package-update-audit`、Task Scheduler /
   cron、常駐なし)。Ergo は一回実行の `ergo_pkg watch` が **機械可読の報告 `ergo-packages.watch.json`** を書く
   所まで。配送 (Discord webhook、秘匿語の redaction、2,000 字分割) は Excubitor が既に持つ経路を使う。
   C++ ライブラリ群の Ergo に常駐プロセスを足さない。
4. **前回状態と重複抑止は利用者ディレクトリ** (`%LOCALAPPDATA%/ergo/package-watch/` または
   `$XDG_STATE_HOME/ergo/package-watch/`) に置く。通知は (ホスト, パッケージ, 種別, 対象 SHA / タグ / PR id)
   の組で 1 回。公開リポの中にも、ビルドディレクトリにも置かない。
5. **Ludellus-Native の状態確認は P1 の派生図 P4「ホスト状態」**。中心にホスト、周りに依存パッケージ、札に
   pin / 経路 / drift / 遅れ / 審査中 PR / 最後のビルド・テスト結果を載せる。新しい配置は作らず
   `ergo_graph_layout` を再利用する。Ludellus-Native には **独自のマニフェストは要らない** (正本は Ergo の
   `ergo-packages.json`)。移行は L1 (照合 M1、`ERGO_PACKAGE_PROVIDED_PICTOR`) → L2 (委譲 M2、`SUIKA_PICTOR_DIR`
   を `ERGO_PACKAGE_SOURCE_DIR_PICTOR` の別名に) の 2 PR (Ludellus-Native 側)。
6. **Unity 向けは Ergo 内の UPM パッケージ `unity/com.ludiars.ergo.packages/`** (Tela の
   `unity/com.ludiars.tela/`、Praeforma の `Packages/jp.ludiars.praeforma/` と同じ「所有リポに置く」慣習)。
   Unity 側は **JSON の読み手** に徹し、ネイティブ CLI を呼ばない。マニフェストに top-level `upm` 節 (LUDIARS の
   UPM パッケージの git URL と rev) を足し、Unity の `packages-lock.json` の hash と照合して「遅れ」を出す。
   CMake の読み手は `packages` / `modules` / `observe` しか読まないので **CMake 側の変更なし** で足せる。
7. **段階は S3 の後に W1〜W5 を差し込み、S4 と並行**。触るディレクトリを分ける (`tools/package_cli/`、
   `include|src|tests/package_watch/`、`package_viz/host_view.*`、`unity/`) ので S4 (scan / P3) と衝突しない。
   `ergo-packages.json` は追加キーだけ (`track` / `upm`)。
8. **公開リポの注意**: 報告・通知・状態ファイルには **プロジェクトコード・リポ URL・SHA・タグ・PR id** だけを
   載せ、絶対パスは載せない (ホストの位置は `host_id` で表す)。webhook は Excubitor の暗号化設定、
   トークンは環境変数だけ。S3 と同じ秘匿語辞書の照合を提出条件に入れる。

---

## 2. 解釈 (親の解釈への同意・異論)

| 親の解釈 | 判断 | 根拠 |
|---|---|---|
| パッケージマネージャ = 追加・削除・更新・取得できる道具へ広げる | 同意。ただし **編集対象は Ergo のマニフェストだけ**、ホストの `CMakeLists.txt` は書き換えない | N7 で「pin の更新はホスト側の人が Ergo の `rev` を上げる PR」と決めた。CLI が書き換えるのはその PR の中身 (`ergo-packages.json`) で、ホストの構成は 6 経路の変数で受ける (S2) |
| lock ファイルを持つかが論点 | **持たない** | S1 で「マニフェストがロックを兼ねる、別のロックは作らない」と決め、`spec/data/ergo-packages.md` に明記済み。実際に解決した内容は `<build>/ergo-packages.resolved.json` が担う。lock を足すと正本が 3 つになる |
| 更新 = Pf や AI がブランチを更新・main へマージしたこと | **異論**: Pf はブランチを作らない | Praeforma の server は `project-git-activity.ts` で HEAD の reflog を **読むだけ** (一覧の並べ替え用、「git は起動しない」)。データ正本は DB、git に置くのは schema / migrations / export snapshot (`praeforma.md` §4)。ブランチを作るのは Concordia の委託 (feat ブランチ + worktree) と、それを受ける Revisor の local PR。したがって「Pf 由来」は「Pf のプロジェクトから起きた Concordia 委託」としか言えず、Cc の run の `project` / `triggered_by` で表す |
| 通知先と担い手は論点、Ergo に常駐を持たせるかは判断すること | **Ergo に常駐は持たせない** | Ergo は C++ ライブラリ群で Excubitor catalog に常駐サービスを持たない。Excubitor には「日次でパッケージ更新を集めて専用 Discord webhook に出す」ジョブが既にあり、絶対パス・URL・資格情報の redaction も実装済み (`package-update-audit.md`)。同じ形で乗せるのが最小 |
| Ludellus-Native を最初の実利用ホストにし、Tela で 1 か所で見る | 同意 | `native/CMakeLists.txt` は `SUIKA_ERGO_DIR` の Ergo を `add_subdirectory` した **後** に `SUIKA_PICTOR_DIR` の Pictor を `add_subdirectory` している。ハブの `host` 経路 (Ergo より前にターゲットがある) には乗らず、`provided` 経路 (ディレクトリ末尾で検証) が入口になる |
| Ludellus-Native の `ergo-packages.json` 化 | **ホスト側マニフェストは作らない** | 正本は Ergo のマニフェスト 1 つ (S1 「ホスト側の別マニフェストは作らない」)。Ludellus-Native が要るのは **変数** (`ERGO_PACKAGES` / `ERGO_PACKAGE_SOURCE_DIR_PICTOR`) と、ホストを識別する `host_id` だけ。非公開の名前が要るときだけ `ERGO_PACKAGE_MANIFEST_EXTRA` |
| Unity 用パッケージ: 何を共有するか、置き場所 | 同意。共有するのは **ファイル形式** (JSON 3 種)、置き場所は Ergo 内 | Tela と Praeforma は自リポに UPM を置く。UnityFoundation は Unity 2022.3 の **プロジェクト** で、他リポの UPM を取り込む側 (`unity/Packages/manifest.json`)。Unity は CMake を使わないので、共有できるのは「同じ JSON を同じ意味で読む」ことだけ。C# からネイティブ CLI を呼ぶと Editor の起動環境にビルド済みバイナリを要求してしまう |

追加で立てた解釈:

- **「ホスト」の識別子が要る。** 通知に「どのホストの依存か」を書くには、構成したホストに名前が要る。
  `ERGO_PACKAGES_HOST_ID` (キャッシュ変数、プロジェクトコード推奨、例 `Ln`) を足し、resolved.json に
  `host.id` として写す。未設定なら `CMAKE_PROJECT_NAME`。パスは写さない。
- **「遅れ」の基準は追跡ブランチ。** `rev` が追跡ブランチ (既定 `main`) のどこにいるかで `behind N` を出す。
  パッケージが別の既定ブランチを使うときのために `track.branch` を任意で足す (追加キー、CMake は読まない)。
- **Revisor の版は固定子ではない** (S1)。タグ更新の検知は `git ls-remote --tags` の **新しいタグの出現**
  で行い、semver 比較は表示の並べ替えだけに使う。

---

## 3. 比較した案

### 3.1 パッケージマネージャの実体

| 案 | 内容 | 長所 | 短所 |
|---|---|---|---|
| A. CMake だけ (`cmake -P` スクリプト + カスタムターゲット) | `ergo_pkg_outdated` 等を `add_custom_target` で提供 | 追加ツールチェーン無し。構成時の情報をそのまま使える | `string(JSON)` は書き戻しに弱い (整形・キー順が壊れる)。`ls-remote` の解析や HTTP は CMake で書くと読めない。Unity と形式を共有する JSON ライターが 2 系統になる |
| **B. C++ CLI `ergo_pkg`** (推奨) | `tools/package_cli/`、`ergo_package_model` と `jsonm` を再利用、HTTP は `ergo_http` | S3 の `ergo_package_viz` と同じ切り方・同じ読み手。resolved.json / File API を既に読める。Excubitor から一回実行で呼べる | Ergo をビルドしないと CLI が無い (→ 構成時の最低限 (状態ファイルの 1 行表示) は CMake 側に残す、§4.3) |
| C. Node CLI (`tools/ergo` のプラグイン) | TS で書き、Excubitor と言語を揃える | Excubitor / Revisor と同じ言語。Discord 送信も書ける | `tools/ergo` は「Web 開発者ツールのプラグインホスト」であり、`npm install` が要る。統合設計 U6 で「`tools/ergo` の版一覧は低優先の任意の後段」と決めている。C++ の読み手 (S3) と二重実装になる |

### 3.2 lock ファイル

| 案 | 長所 | 短所 |
|---|---|---|
| **a. 作らない** (マニフェスト = ロック、現状維持) | 正本が増えない。S1 で決定済み。`rev` は既に 40 桁 SHA | 「希望の版 (`expect_tag`)」と「解決した版 (`rev`)」が同じファイルにあるので、`update` は両方を同時に書く規律が要る (CLI がやる) |
| b. `ergo-packages.lock.json` を足す | npm / cargo の慣習に近い | ロックが意味を持つのは「範囲指定を解決する」ときで、本マニフェストに範囲指定は無い。差分レビューの対象が 2 ファイルに増える |
| c. resolved.json を追跡する | 「実際に何が解決されたか」が履歴に残る | ホストごと・構成ごとに違う内容で、Ergo リポに置けない。経路やパスを含む |

### 3.3 更新検知の取得元

| 取得元 | 取れるもの | 認証 | 採否 |
|---|---|---|---|
| **`git ls-remote <repository>`** | タグ一覧、`refs/heads/<track>` の先頭 SHA | 公開リポは不要。非公開は呼び出し環境の git 資格情報 (CLI は触らない) | 採用 (a)(b)。先頭 SHA しか取れないので `behind N` の **数** は手元の checkout (兄弟 / キャッシュ) があるときだけ `rev-list --count` で補う |
| `git fetch` (兄弟 checkout / キャッシュへ) | 距離と履歴 | 同上 | `--fetch` を付けたときだけ (作業ツリーを触らない。`FETCH_HEAD` だけ更新) |
| GitHub API (`/repos/.../tags`, `/releases`) | リリースノート | トークン無しは 60 req/h | 初版は使わない。Excubitor の `release-notes.ts` が同じ用途を既に持つので、要るなら配送側で足す |
| **Revisor `GET /v1/local-prs?view=summary&state=open`** (loopback 4240) | 審査中 PR の `repository` / `headRef` / `baseRef` / `headSha` / status | 読み取り。破壊操作だけ workflow token (`pr-lifecycle.md` §6)。読み取りの認可は実装で確認 (未確認) | 採用 (c)。`repository` がパッケージの `repository` と一致するものだけ |
| **Concordia `GET /v1/delegation/runs?status=running`** (loopback 11111) | 進行中 run の `project` / `target_repo` / `call_name` | 無し | 採用 (d)。`target_repo` の basename がパッケージの `REPO_NAME` と一致するもの |
| Praeforma | (ブランチ情報なし) | — | 使わない (§4.2) |

### 3.4 定期実行の担い手

| 案 | 長所 | 短所 |
|---|---|---|
| Ergo に常駐 (watch デーモン) | 自己完結 | Ergo は C++ ライブラリ群で、catalog の常駐サービスを持たない。プロセス管理・ログ・再起動・秘匿情報の保管をすべて新規に持つことになる |
| **Excubitor の既存日次ジョブに 1 ステップ足す** (推奨) | `package-update-audit` が「catalog の作業ディレクトリを回り、更新を集め、専用 webhook に出す」形を既に持つ。redaction・分割・Task Scheduler 登録済み。常駐を増やさない方針とも一致 | Excubitor 側の PR が要る (Ergo 外)。報告の形式を Ergo と Excubitor で合わせる契約 (`ergo-packages.watch.json` schema) が要る |
| CI (GitHub Actions の schedule) | 開発機に依存しない | Revisor / Concordia は loopback にしかいない。(c)(d) が取れない。Discord webhook を GitHub secrets に置く運用が増える |
| エディタ (tools/ergo / Tela / Unity) 起動時 | 開発者が見ている瞬間に出る | 起動のたびにネットワークへ出る。開発機ごとに重複する。起動が遅くなる |
| 構成時 (`cmake`) | 全ホストで必ず通る | **構成でネットワークに出ない**原則 (S2: 既定で取りに行かない) を破る。採るのは「状態ファイルがあれば読んで 1 行出す」だけ |

### 3.5 Ludellus-Native の表示

| 案 | 長所 | 短所 |
|---|---|---|
| P1 (版一覧) に札を足すだけ | 実装最小 | P1 は Ergo を中心にした「全パッケージ」。ホストの依存だけに絞れず、ビルド・テストの状態を置く場所が無い |
| **P4 ホスト状態図 (P1 の派生、ホストを焦点に)** (推奨) | 既存の配置・出口を再利用。ホスト固有の札 (ビルド / テスト / 審査中 PR) を持てる | 図が 1 種増える (`host_view.*` を 1 ファイル追加) |
| Tela 以外 (`tools/ergo` の Web 画面) | HTML 表が向く情報 | 人間の指示は「可視化は Tela」。U6 で低優先 |

### 3.6 Unity パッケージの置き場所と機能

| 案 | 長所 | 短所 |
|---|---|---|
| **Ergo 内 `unity/com.ludiars.ergo.packages/`** (推奨) | 形式 (JSON) の正本と同じリポで、形式変更と読み手の変更を 1 PR にできる (横断変更ルール)。Tela の `unity/com.ludiars.tela` と同じ置き方。git URL + `?path=` で入る | Ergo リポに C# が入る (Editor 専用 asmdef に閉じる) |
| UnityFoundation に置く | Unity プロジェクトが既にある | UnityFoundation は「取り込む側」。Ergo の形式が変わるたびに別リポ PR。Foundation は Unity 2022.3 と CRI に縛られる |
| 新規リポ | 独立に版管理 | 置き場所が増え、形式の正本と離れる。初版の規模 (読み手 + 窓 1 枚) に対して過大 |

Unity 側の機能の切り方:

| 案 | 長所 | 短所 |
|---|---|---|
| ネイティブ CLI (`ergo_pkg`) を `Process` で呼ぶ | 実装 1 つ | Editor の環境にビルド済み `ergo_pkg` が要る。Unity だけのプロジェクトには無い |
| **C# で JSON 3 種 (マニフェスト / watch 報告 / `packages-lock.json`) を読む** (推奨) | Unity 単体で動く。ネットワークに出るのは任意の loopback 読み取りだけ | 書き手 (C++) と読み手 (C#) で形式を合わせる契約が要る → `spec/data/` に schema を置き、両方の fixture テストで同じ見本を読む |
| C# で `git ls-remote` も呼ぶ | Unity 単体で (a)(b) が取れる | git の有無・資格情報の扱いが Editor 内に入る。初版は見送り (N-W4) |

---

## 4. 推奨案の詳細

### 4.1 パッケージマネージャの範囲 (依頼 5-1)

**CLI `ergo_pkg`** (ターゲット `ergo_pkg_cli`、`tools/package_cli/`、`ERGO_BUILD_PACKAGE_CLI` で ON/OFF)。
S3 の `tools/package_viz/` と同じ構成 (`main.cpp` は配線だけ、`cli_options.*` / 各コマンド 1 ファイル)。
終了コード: 0 成功 / 1 検出あり (`outdated` / `verify` / `watch`) / 2 引数・入力の誤り / 3 書けない・取れない。

| コマンド | 入力 | 動き | 書くもの |
|---|---|---|---|
| `list [--manifest <f>] [--resolved <build>]` | マニフェスト (+ resolved.json) | パッケージ・kind・`rev` (SHA7)・`expect_tag`・解決済みなら経路 / mode / drift | 標準出力 (表、`--json`) |
| `outdated [--resolved <build>] [--remote] [--fetch]` | 同上 + `ls-remote` | 各 link / tool / upm について新タグ・追跡ブランチの先頭 SHA・(手元に履歴があれば) `behind N` | 標準出力。`--json` で報告と同じ形。1 件以上で終了 1 |
| `verify [--resolved <build>] [--strict]` | resolved.json | S2 の findings (PKG-xx) を再表示し、`--strict` なら error 相当で終了 1。構成し直さずに「今の構成は STRICT を通るか」を見る | 標準出力 |
| `fetch [--cache <dir>] [<name>...]` | マニフェスト | 各 `repository@rev` を **FetchContent と同じ置き場所** (`ERGO_DEPENDENCY_CACHE_DIR` 相当、既定 `<build>/_deps`) に取る。構成時の経路 5 が **オフラインでも当たる** ようにする事前取得。作業ツリーは触らない | キャッシュ dir。標準出力に取得した SHA |
| `update <name> --to <tag>\|<sha>\|--track [--dry-run]` | Ergo checkout のマニフェスト | `rev` を対象 commit の 40 桁 SHA に、タグ指定なら `expect_tag` も書く。`--track` は追跡ブランチの先頭。**ファイルは整形を保って書き戻す** (キー順維持、2 スペース)。`--dry-run` で差分だけ | `ergo-packages.json`。標準出力に「次にやること」(ブランチ・PR) |
| `add <name> --repository <url> --rev <sha> --kind link\|tool\|upm [...]` / `remove <name>` | 同上 | 骨格だけ足す / 消す。`link` は `cmake.wiring` のファイルが要るので **存在しなければ終了 2** (結線は人が書く。生成しない) | `ergo-packages.json` |
| `watch [--host <id>] [--resolved <build>] [--state <dir>] [--out <file>] [--revisor <url>] [--concordia <url>]` | §4.2 | 更新検知 → 前回状態と比較 → 報告 | `ergo-packages.watch.json`、状態ファイル。新規通知 1 件以上で終了 1 |
| `status --build <dir> [--out <file>]` | ホストのビルドディレクトリ | §4.4 のビルド・テスト状態 | `ergo-packages.status.json` |

**マニフェストとの関係**: 正本は変わらず `ergo-packages.json` (schema 1)。追加キーは任意で、CMake 側は読まない。

```json
"pictor": {
  "...": "既存キーはそのまま",
  "track": { "branch": "main" }
},
"upm": {
  "com.ludiars.tela": {
    "code": "Tl",
    "repository": "https://github.com/LUDIARS/Tela.git",
    "path": "unity/com.ludiars.tela",
    "rev": "4142f77c0775e1c3ad93733526085c56523ea9dc",
    "track": { "branch": "main" }
  }
}
```

- `track.branch` 省略時は `main`。`upm.<name>.rev` は link と同じ 40 桁 SHA (PKG-02 を C# / C++ 読み手でも適用)。
- `schema` は 1 のまま (追加キーのみ。既存の読み手の挙動が変わらないことを `test_packages_m0_invariance` で保つ)。
  `spec/data/ergo-packages.md` に `track` / `upm` の節を足す。

**取得 (fetch) の方法と置き場所**: 新しい保管庫は作らない。`fetch` は `cmake/ErgoDependencies.cmake` の
`_ergo_fc_declare` と同じ URL / SHA / `GIT_SHALLOW` で、同じ `FETCHCONTENT_BASE_DIR` に populate する
(実装は `cmake -P cmake/packages/ErgoPackagePrefetch.cmake` を CLI が起動する。取得エンジンを二重に持たない)。

**オフライン時**: `outdated --remote` / `watch` は `ls-remote` 失敗を **`unreachable` として報告に残し**、終了 3 に
しない (他の項目を続ける)。構成は従来どおり: `source` / `sibling` / 事前 `fetch` 済みキャッシュのどれかがあれば通る。
**兄弟チェックアウト時**: `outdated` は pin と兄弟 HEAD の関係 (S2 の drift) と、兄弟の `refs/remotes/origin/<track>`
も読んで「兄弟 checkout 自体が古い」を区別して出す (`sibling_behind`)。兄弟の作業ツリーは変更しない。

**既存 S2 経路・STRICT・N7 との整合**:
- CLI はホストの構成を変えない。変えるのは Ergo のマニフェストだけ → N7 どおり「使いたい人が Ergo に PR」。
  `update` の出力に `git checkout -b feat/pkg-<name>-<tag>` とコミットメッセージの雛形を出す (実行はしない)。
- `update --to <sha>` でタグに乗らない SHA を指すと、`expect_tag` を消すか据え置くかを尋ねず **据え置き + 警告**
  (PKG-08 が構成時に判定する)。STRICT の CI (`ci-packages`) が N4 を守る。
- `fetch` は `ERGO_PACKAGES_FETCH=ON` の経路 5 と同じ内容を先に置くだけで、経路の順序や判定 (PKG-04) は変えない。

### 4.2 更新の検知 (依頼 5-2)

**Praeforma の確認結果** (LUDIARS/Praeforma の手元 checkout、2026-10-14 時点):
- `server/src/lib/project-git-activity.ts`: リポジトリ置き場所 (`PRAEFORMA_GIT_REPOS_ROOT`) 直下の各リポの
  `.git/logs/HEAD` の **末尾 64 KiB を読むだけ**。「git は起動しない」「checkout 行は数えない」。用途は一覧の並べ替え。
- `spec/feature/praeforma.md` §4: 正本は Postgres、git に置くのは `spec/schema/` / `migrations/` / export snapshot。
- `spec/feature/local-mode.md`: 同じ reflog 読みの仕様。ブランチ操作の記述なし。
- `server/src` に `checkout` / `push` / `branch` を実行するコードは無い (grep: `git` を扱うのは上記 1 ファイルと
  `astra-cli.ts` の呼び出し名のみ)。
- Pf のタスク文書にあるブランチ名 (`feat/data-schema-logical-storage-20260908` 等) はすべて **Concordia の委託
  セッションが作った** もの。

よって「Pf 由来の更新」は **Concordia の run の属性** (`project` が Pf のプロジェクト、`template` が Pf 系) であり、
ブランチの形では区別できない。報告には `origin: "concordia"` と run の `call_name` / `project` を載せ、「Pf」の
札はその値から表示側が付ける。

**何を更新とみなすか** (種別 `kind`):

| kind | 判定 | 対象 SHA / id | 根拠の取得元 |
|---|---|---|---|
| `tag` | 前回状態に無いタグが `ls-remote --tags` に現れた (`^{}` の剥がし済み commit で比較) | タグ名 + commit | `git ls-remote --tags --refs <repository>` |
| `track` | `refs/heads/<track.branch>` の先頭 SHA が `rev` と違い、かつ前回通知した先頭と違う | 先頭 SHA (手元に履歴があれば `behind N`) | `git ls-remote --heads`、補助に兄弟 / キャッシュの `rev-list --count` |
| `review` | Revisor の open な local PR で `repository` がパッケージの `repository` と一致 | local PR id + `headSha` | `GET /v1/local-prs?view=summary&state=open` |
| `run` | Concordia の running な委託 run で `target_repo` の basename が `REPO_NAME` と一致 | run id | `GET /v1/delegation/runs?status=running` |

`review` と `run` は **「来そうな更新」**で、表示上は `tag` / `track` と区別する (審査中 / 作業中)。
同じ変更が `run` → `review` → `track` → `tag` と進むのは正常で、それぞれ別の通知になる (抑止しない。種別が違う)。

**取得元と認証**: `ls-remote` は呼び出し環境の git をそのまま使う (資格情報の保管・入力は CLI の責務外。
失敗は `unreachable`)。Revisor / Concordia は loopback のみ受け付け (`127.0.0.1` 以外の URL は終了 2)。
トークンが要る場合は環境変数 `ERGO_PKG_REVISOR_TOKEN` だけを読む (引数で受けない、ログに書かない)。
URL の既定は Excubitor の ProcessMap から配送側 (Excubitor) が渡す (Ergo 側に数字を埋めない。`port-source-rule`)。

**頻度**: 日次 1 回 (Excubitor の既存ジョブと同じ時刻)。手動は `ergo_pkg watch` をいつでも。構成時は
ネットワークに出ない。

**前回状態の保存場所**: `--state <dir>` または `ERGO_PKG_STATE_DIR`、既定は
Windows `%LOCALAPPDATA%/ergo/package-watch/`、他は `$XDG_STATE_HOME/ergo/package-watch/`
(無ければ `~/.local/state/...`)。ファイルは `<host_id>.json` (schema 1): パッケージごとに
`seen_tags` / `last_notified_track_sha` / `notified_reviews` / `notified_runs` / `checked_at`。
リポ内・ビルドディレクトリ内には置かない (ビルドを消しても重複通知しない)。

**重複通知の抑止**: 通知キー = (`host_id`, package, kind, 対象)。状態にあるキーは `changes` に入れず
`pending` (未解消の既知更新) に入れる。日次の報告は `changes` が空でも書く (「新規なし、未解消 N」)。
`review` / `run` は終局 (closed / merged / completed) すると `pending` から消える。

**報告 `ergo-packages.watch.json`** (schema 1、`spec/data/ergo-packages-watch.md` に正本):

```json
{
  "schema": 1,
  "generated_at": "2026-10-14T09:00:12Z",
  "host": { "id": "Ln", "manifest_rev": "<Ergo の HEAD SHA>", "mode": "M1" },
  "packages": {
    "pictor": {
      "code": "Pc", "repository": "https://github.com/LUDIARS/Pictor.git",
      "rev": "625d83d6...", "expect_tag": "v2.2.0", "route": "provided", "drift": "none",
      "remote": { "track": "main", "head": "9b1c...", "behind": 7, "reachable": true },
      "changes": [
        { "kind": "tag", "name": "v2.3.0", "commit": "9b1c...", "first_seen": "2026-10-14T09:00:12Z",
          "action": "ergo_pkg update pictor --to v2.3.0" },
        { "kind": "review", "id": "a1b2...", "head_ref": "feat/webgpu-rhi", "head_sha": "77aa...",
          "origin": "concordia", "run_call_name": "impl-from-design", "action": null }
      ],
      "pending": []
    }
  },
  "upm": { "com.ludiars.tela": { "...": "同じ形。lock_hash は Unity 側が照合" } },
  "unreachable": []
}
```

パスは一切含まない。`action` は **人が打つコマンド 1 行** (実行はしない)。

### 4.3 通知 (依頼 5-3)

**通知先と形式** (優先順):

| 先 | 形式 | 誰に | 担い手 |
|---|---|---|---|
| **Discord** (Excubitor の package-audit 専用 webhook) | 1 ホスト 1 メッセージ (2,000 字分割は Excubitor 側)。見出し `[<host_id>] LUDIARS パッケージ更新 N 件`、1 件 1 行 | webhook の channel を見る開発者 (ホストの担当) | Excubitor の日次ジョブが `ergo_pkg watch` を実行し、`changes` を描く |
| CLI 標準出力 | 同じ内容の表 | 手で `watch` を打った人 | `ergo_pkg` |
| 構成時の 1 行 | `-- ergo packages: 2 update(s) known for host Ln (see ergo_pkg watch)` | その場で `cmake` した人 | `ErgoPackageReport.cmake` が **状態ファイルがあれば読む** (無ければ何も出ない。ネットワークには出ない) |
| Tela (P4) | 札と色 | 図を開いた人 | `ergo_package_viz --view host` が watch 報告を読む (§4.4) |
| Unity Editor 窓 | 一覧 + バッジ | Unity の開発者 | `com.ludiars.ergo.packages` が同じ報告を読む (§4.5) |
| Concordia chitchat | 任意 (N-W2) | 協調中のセッション | 初版では送らない |

**通知の内容** (1 件 1 行、Discord の例):

```
[Ln] pictor (Pc) v2.2.0 → 新タグ v2.3.0 (9b1c7f2)。main は pin より 7 進み。
     → Ergo で `ergo_pkg update pictor --to v2.3.0` → PR。
[Ln] pictor (Pc) 審査中: Revisor local PR a1b2c3 feat/webgpu-rhi (Concordia impl-from-design 由来)
[Ln] tela (Tl) 取得元に到達できず (ls-remote 失敗)。前回確認 2026-10-13。
```

載せるもの: ホスト id、パッケージ名とコード、今の pin (タグ / SHA7)、何が変わったか、次の操作。
載せないもの: 絶対パス、webhook URL、トークン、個人名、非公開リポ名 (非公開は `ERGO_PACKAGE_MANIFEST_EXTRA`
のパッケージで、報告の `code` と `name` だけを出し `repository` は伏せる `private: true` フラグ)。

**定期実行の担い手とその理由**: Excubitor (§3.4)。理由を 3 つに絞る: (1) 常駐を増やさない (Ergo は C++
ライブラリ群で、Excubitor catalog に Ergo の常駐は無い)、(2) Excubitor は「catalog の作業ディレクトリを回って
更新を集め、専用 webhook に出す」日次ジョブ・redaction・分割を既に持つ、(3) Revisor / Concordia は loopback に
いるので、同じ機械の Excubitor からしか (c)(d) は取れない (CI では取れない)。
Excubitor 側の追加は 1 ステップ: catalog の各サービスの作業ディレクトリに **`ergo-packages.resolved.json` を
持つビルドディレクトリ** (`ERGO_PACKAGES_BUILD_DIR` を catalog の env で宣言) があれば `ergo_pkg watch` を
一回実行し、報告の `changes` を既存の Discord 描画に流す。`ergo_pkg` の実行ファイルは Ergo のビルド成果
(catalog の `exec` と同じ指し方)。

### 4.4 Ludellus-Native の状態確認 (依頼 5-4)

**表示するもの** (1 ホスト 1 図、P4):

| 要素 | 内容 | 出所 |
|---|---|---|
| 中心ノード | ホスト (`host_id` = `Ln`、`CMAKE_PROJECT_NAME`、mode `M0/M1/M2`) | resolved.json `host` |
| パッケージノード | 依存する link / tool (Ludellus-Native なら `pictor`、将来 `tela`) と Ergo 自身 | resolved.json `packages` |
| 札 1 行目 | `pictor (Pc) v2.2.0 625d83d` + 経路 (`provided`) | resolved.json |
| 札 2 行目 | drift (`none/ahead/behind/diverged/unknown`)、dirty | resolved.json findings (PKG-05/07) |
| 札 3 行目 | 遅れ: `main +7` / 新タグ `v2.3.0` / 審査中 1 / 作業中 0 / 到達不可 | watch 報告 |
| 札 4 行目 | 最後のビルド・テスト: `build ok 10-14 08:52` / `ctest 12/12` | `<build>/ergo-packages.status.json` (下記) |
| 辺 | ホスト → パッケージ (実リンク、File API から。S3 の帰属を再利用)、パッケージ間 `requires` | File API codemodel |
| 色 | 更新なし / 既知の未解消 / 新規 / 到達不可 の 4 段 (Tela の group で表す。8 グループ上限内) | 合成 |

**ビルド・テストの状態の取り方**: ネットワークも CI も使わず、**ホストのビルドディレクトリから読む**。
`ergo_pkg status --build <dir>` が `CMakeCache.txt` の生成時刻、`Testing/Temporary/LastTest.log` (ctest の
既定出力) または `--output-junit` の XML を読んで `ergo-packages.status.json` (schema 1、`build_ok` /
`configured_at` / `tests {passed, failed, total, at}`) を書く。CI の結果 (GitHub Actions) は初版では読まない
(トークンと API が絡む。N-W3)。

**Tela での画面構成**: `ergo_package_viz build --view host --resolved <build> --watch <report> --status <status>
--out host.tela`。配置は `ergo_graph_layout` (左にホスト、右に依存、更に右に `requires` 先)。P1 / P2 とは別ファイル、
`ergo_package_graphs` ターゲットが P1 / P2 / P4 をまとめて書く。上限超過時の畳みは P1 と同じ `fit_view`。
P1 (全パッケージ版一覧) は「Ergo が何を固定しているか」、P4 は「このホストが何を使い、何が来ているか」。
P2 (ターゲットのリンク図) は変えない。

**Ludellus-Native の移行 (S5 の具体化)**。現状: `SUIKA_ERGO_DIR` (Ergo を `add_subdirectory`、`ERGO_BUILD_*` を
FORCE で絞る) → その後 `SUIKA_PICTOR_DIR` が存在すれば Pictor を `add_subdirectory` (`PICTOR_BUILD_*` FORCE)。
Ergo の **後** に Pictor を作るので `host` 経路には乗らない。

| 段 | Ludellus-Native の変更 (1 PR ずつ、Ludellus-Native 側) | Ergo 側 | 確認 |
|---|---|---|---|
| L0 (今) | なし。M0 `host-unmanaged` として resolved.json に記録されるだけ | なし | 既存ビルド不変 |
| L1 照合 | `add_subdirectory(Ergo)` の前に `set(ERGO_PACKAGES "pictor")`、`set(ERGO_PACKAGE_PROVIDED_PICTOR ON)`、`set(ERGO_PACKAGES_HOST_ID "Ln")`。Pictor の取り込みはそのまま | なし (`provided` 経路は S2 で実装済み) | 構成ログ `mode=M1 (pictor=provided)`。Pictor checkout の HEAD と pin の drift が出る。`SUIKA_PICTOR_DIR` 無しのとき `pictor` が無いので PKG-10 → **`ERGO_PACKAGE_PROVIDED_PICTOR` は `SUIKA_PICTOR_DIR` が存在するときだけ ON にする** |
| L2 委譲 | Pictor の `add_subdirectory` と `PICTOR_BUILD_*` FORCE を消す。`SUIKA_PICTOR_DIR` が指定されていれば `ERGO_PACKAGE_SOURCE_DIR_PICTOR` に写す (1 リリース分の別名、`DEPRECATION` 警告)。無指定なら `ERGO_PACKAGES` から `pictor` を外す (描画なしビルドを保つ)。`SUIKA_ENABLE_RENDER` は `TARGET pictor` で判定 | なし | `mode=M2 (pictor=source)`。`suika_*` ターゲット集合と compile definitions が L1 と同じ (ホスト側 ctest) |
| L3 | `SUIKA_ERGO_DIR` は **残す** (Ergo 自身はパッケージではない)。`-DERGO_PACKAGES_SIBLING_ROOT` を開発機の `CMakeUserPresets.json` に書く手順を `spec/setup/native-build.md` に足す | なし | 手順の文書 |

msquic (`cmake/FetchMsQuic.cmake`) と OpenCV は第三者で対象外のまま。

### 4.5 Unity パッケージ (依頼 5-5)

**形式**: UPM、`unity/com.ludiars.ergo.packages/` に `package.json` (`"name": "com.ludiars.ergo.packages"`,
`"unity": "2022.3"`, version は Ergo のリリースに合わせず独立 `0.1.0` から)、`Editor/` + `Ergo.Packages.Editor.asmdef`
(Editor 専用、ランタイム asmdef なし)、`Tests/Editor/` + `Ergo.Packages.Editor.Tests.asmdef`、`Samples~/` に
見本 JSON。導入は `manifest.json` に
`"com.ludiars.ergo.packages": "https://github.com/LUDIARS/Ergo.git?path=unity/com.ludiars.ergo.packages#<sha>"`。
依存: `com.unity.modules.jsonserialize` のみ (Newtonsoft に依存しない。schema 1 の JSON は `JsonUtility` で
読める平たい形に `[Serializable]` DTO を用意。`changes` の `kind` 差は 1 DTO にフィールドを全部持たせ null 許容)。

**置き場所の理由**: §3.6。Tela / Praeforma と同じ「所有リポに置く」慣習。形式 (JSON) の変更と読み手の変更を
同一 PR にする横断変更ルールが守れる。

**Unity 側で持つ機能** (初版):

| 機能 | 内容 | 読むもの |
|---|---|---|
| エディタ窓 `LUDIARS/Packages` | 2 タブ。**Native**: Ergo のマニフェストと watch 報告の一覧 (パッケージ / pin / 新タグ / 審査中 / 作業中)。**Unity**: マニフェスト `upm` 節と `Packages/packages-lock.json` の照合 (`hash` が `rev` と違えば「遅れ」、`packages-lock.json` に無ければ「未導入」) | `ergo-packages.json` (場所は窓の設定: Ergo checkout またはこのパッケージに同梱の **コピー** を既定)、`ergo-packages.watch.json` (設定したパス。既定は状態ディレクトリと同じ規則で `%LOCALAPPDATA%/ergo/package-watch/<host>.json` の隣に Excubitor が置く最新報告 `latest.watch.json`) |
| 通知 | 窓を開いたとき・ドメインリロード後に報告を読み直し、`changes` の新規分を Console に 1 行ずつ (`Debug.Log`、パスなし) + 窓のバッジ。既読は `SessionState` (Praeforma の下書きと同じ保持範囲) | watch 報告 |
| 操作 | 「更新コマンドをコピー」(`action` の文字列をクリップボードへ)、「報告を再読込」。**git も CLI も実行しない** | — |
| 任意 | 設定で Revisor loopback の読み取り (`/v1/local-prs?view=summary&state=open`) を直接見る。既定 OFF。loopback 以外は拒否 (Praeforma の `unity-editor-instructions.md` と同じ規律) | Revisor |

**ネイティブ側との共有のしかた**: 共有するのは **3 つの JSON の形式** (`ergo-packages.json` / `ergo-packages.watch.json` /
`ergo-packages.status.json`) と、その **見本ファイル** (`tests/package_watch/fixtures/*.json`)。C++ のテストと
Unity の EditMode テストが **同じ見本** を読む (Unity 側は `Samples~/fixtures` へのコピーを PR で揃え、
`scripts/check-unity-fixtures.*` が差分 0 を確認する)。C# から C++ を呼ばない。

**Unity 版の検証方法**:
- EditMode テスト (Unity Test Framework): 見本 3 種を読んで DTO が一致、`packages-lock.json` の照合 (一致 / 遅れ /
  未導入 / hash 無し)、`changes` の既読管理、loopback 以外の URL を拒否。
- 構造検査 (CI、Unity 不要): `package.json` の必須キー、asmdef が Editor 専用、`Runtime/` が無い、
  見本の差分 0。Node か PowerShell の小スクリプトで `.github/workflows/ci.yml` に 1 ジョブ。
- 手元確認: UnityFoundation (Unity 2022.3) に git URL で導入して窓が開く。これは **人の許可のもとで** 行い、
  未実施なら未実施と書く (Praeforma と同じ: コンパイルは end-to-end の代わりにならない)。

### 4.6 既存段階 (S4〜S7) との関係 (依頼 5-6)

S4 (観測 P3: `scan`、MUSA 図) は並行実装中の前提。衝突を避ける分け方:

| 領域 | S4 が触る | W が触る | 重なり |
|---|---|---|---|
| `include|src|tests/package_model/` | `scan*.{h,cpp}` (package.json / .gitmodules / ported_from) | **触らない** (watch 報告は別モジュールで読む) | なし |
| `include|src|tests/package_viz/` | `musa_view.*` (P3) | `host_view.*` (P4)、`fit.cpp` は触らない | `CMakeLists.txt` の source 列挙のみ (行追加、同じ場所に足すと衝突 → **W は末尾、S4 は P2 の直後**と決める) |
| `ergo-packages.json` | `observe` の `relations` 追加 | `track` / `upm` 追加 | キーが違う。JSON の末尾カンマだけ注意 |
| `cmake/packages/ErgoPackageReport.cmake` | observe の出力 | `host.id` の出力、状態ファイルの 1 行表示 | 関数を分ける (`_ergo_pkg_report_host` / `_ergo_pkg_report_observe`) |
| `spec/data/ergo-packages.md` | `observe` 節 | `track` / `upm` 節 + 新規 `ergo-packages-watch.md` / `ergo-packages-status.md` | 節が違う |
| `module_list.md` / `.yaml`、`README.md`、`spec/domains/observability.domain.json`、`.anatomia/layers.json` | 行追加 | 行追加 | 行の衝突は手で解ける (どちらが後でも) |
| 新規 | — | `tools/package_cli/`、`include|src|tests/package_watch/`、`unity/`、`cmake/packages/ErgoPackagePrefetch.cmake` | なし |

段階表の組み直し (S4 以降):

| 段階 | 内容 | 依存 |
|---|---|---|
| S4 観測 P3 | 変更なし (並行) | S3 |
| **W1 CLI の読み取り系** | `ergo_pkg list / verify / fetch`、`ErgoPackagePrefetch.cmake`、`ERGO_PACKAGES_HOST_ID` → resolved.json `host` | S3 |
| **W2 更新検知と報告** | `ergo_package_watch` モジュール (ls-remote / Revisor / Concordia の読み手、状態、重複抑止、報告ライター)、`ergo_pkg outdated / watch`、`track` キー、`spec/data/ergo-packages-watch.md` | W1 |
| **W3 配送と構成時の 1 行** | Excubitor 側 PR (日次ジョブに 1 ステップ)、`ErgoPackageReport.cmake` の状態ファイル読み (Ergo 側)、`spec/setup/packages.md` に watch の手順 | W2 |
| **W4 ホスト状態図 P4 と Ludellus-Native L1〜L3** | `ergo_pkg status`、`host_view.*`、`--view host`、`ergo_package_graphs` に P4。Ludellus-Native 側 L1 / L2 / L3 の PR (旧 S5 の最初のホスト) | W2、S3 |
| **W5 マニフェスト編集系と Unity** | `ergo_pkg update / add / remove` (整形保持ライター)、`upm` 節、`unity/com.ludiars.ergo.packages/`、構造検査 CI | W2 |
| S5 ホスト移行 (他ホスト) | AdventureCube 型 / FetchContent 型。Ludellus-Native は W4 で先行 | W4 |
| S6 互換の既定変更 | 変更なし | S5 |
| S7 任意 | `revisor version show` 注釈、`tools/ergo` 版一覧 (watch 報告を読むだけに縮小)、CI 結果の取り込み (N-W3) | — |

### 4.7 公開リポとしての注意 (依頼 5-7)

- **絶対パスを出力に入れない**: 報告・状態・通知・構成ログの 1 行・Tela の札・Unity の Console はすべて
  `host_id` / パッケージ名 / コード / URL / SHA / タグ / PR id / run id だけ。CLI がエラーで経路を示すときは
  **ホストのビルドディレクトリからの相対** にする。`test_package_watch` に「報告の全文字列に `:/` `:\` `/home/`
  `/Users/` が無い」検査を入れる (S3 の秘匿語照合と並ぶ機械検査)。
- **秘匿語**: 非公開パッケージ (`ERGO_PACKAGE_MANIFEST_EXTRA` 由来) は報告で `private: true`、`repository` を
  伏せ、`name` と `code` だけ。Discord の本文も同じ。S3 で使った秘匿語辞書の照合を W の各 PR の提出条件にする。
- **トークン**: `ERGO_PKG_REVISOR_TOKEN` 等は環境変数だけ。引数・設定ファイル・報告・ログに書かない。
  webhook URL は Excubitor の暗号化設定にしかない (Ergo は知らない)。
- **置き場所**: 状態と報告は利用者ディレクトリ、`.tela` / resolved / status はビルドディレクトリ。いずれも
  追跡しない。見本 JSON は追跡する固定ファイルで、開発機の値を入れない (`host.id` は `Example`)。
- **Unity パッケージ**: `package.json` の URL は `https://github.com/LUDIARS/Ergo` のみ。
- **ネットワーク先の固定**: `ls-remote` はマニフェストの `repository` だけ。HTTP は loopback だけ。
  それ以外の URL は終了 2。

---

## 5. 実装の分割と受入条件 (依頼 5-8)

どの段階も単独で main に入り、M0 の既定を変えない。テストは ctest 名で裏づける (Augur の契約は C++ / CMake に
使えない、S3 と同じ)。実機・実ホスト・Unity Editor の確認は人の許可に従い、未確認は未確認と書く。

| 段階 | 成果物 | 受入 (機械で確かめる) | 手元確認 |
|---|---|---|---|
| **W1** | `tools/package_cli/` (`list` / `verify` / `fetch`)、`cmake/packages/ErgoPackagePrefetch.cmake`、`ERGO_PACKAGES_HOST_ID` → resolved.json `host.id`、`spec/data/ergo-packages.md` 更新 | C-W1-1 `list` が見本マニフェスト + 見本 resolved.json から決定的な表 / JSON を出す (`test_package_cli`)。C-W1-2 `verify --strict` が見本 resolved.json の PKG-05 で終了 1、findings 無しで 0。C-W1-3 `fetch` が偽リポ (テスト内 `git init`) を `FETCHCONTENT_BASE_DIR` と同じ場所に取り、直後の `-DERGO_PACKAGES_FETCH=ON` 構成がネットワーク無しで経路 5 に当たる (`test_packages_prefetch`、`cmake -P`)。C-W1-4 `host.id` 未設定時は `CMAKE_PROJECT_NAME`、M0 の結果不変 (`test_packages_m0_invariance` 継続) | なし |
| **W2** | `include|src|tests/package_watch/` (`remote_probe` / `revisor_reader` / `concordia_reader` / `watch_state` / `dedup` / `report_writer`、1 責務 1 ファイル)、`ergo_pkg outdated / watch`、`track` キー、`spec/data/ergo-packages-watch.md`、`spec/module/package_watch.md` | C-W2-1 `ls-remote` の出力見本 (文字列) から tag / track を解析し、`^{}` を剥がす (`test_package_watch` RemoteProbe*)。C-W2-2 Revisor / Concordia の応答見本 (JSON) から `review` / `run` を抽出、`repository` 不一致は無視 (ReaderFilter*)。C-W2-3 同じ入力を 2 回通すと 2 回目の `changes` が空で `pending` に移る、終局で `pending` から消える (Dedup*)。C-W2-4 報告が schema 1 で決定的、`:/` `:\` を含む文字列が無い、`private` パッケージの `repository` が無い (ReportNoPaths*)。C-W2-5 loopback 以外の URL で終了 2、`ls-remote` 失敗は `unreachable` で続行 (Safety*)。C-W2-6 `track` キー追加で CMake の読み手の出力不変 (`test_packages_manifest`) | 実リポ 1 つで `outdated --remote` を打つ (ネットワーク使用は人の許可) |
| **W3** | Ergo: `ErgoPackageReport.cmake` の状態ファイル読みと 1 行表示、`spec/setup/packages.md` watch 節。Excubitor: 日次ジョブの 1 ステップ (`ERGO_PACKAGES_BUILD_DIR` を持つ catalog サービスで `ergo_pkg watch` 実行 → 既存 Discord 描画) | Ergo: C-W3-1 状態ファイルが無い / 壊れている / 別ホストのときは何も出さず構成を止めない (`test_packages_report_watch_hint`)。Excubitor: C-W3-2 報告見本から Discord 本文を作り、パス・URL 以外の秘匿が無い (既存の redaction テストに追加)、`changes` 空で「新規なし」を 1 通 | Excubitor のジョブを 1 回手で回し、channel に 1 通 (人の許可) |
| **W4** | `ergo_pkg status`、`spec/data/ergo-packages-status.md`、`package_viz/host_view.*`、`--view host`、`ergo_package_graphs` に P4。Ludellus-Native 側 L1 / L2 / L3 (別リポ PR) | C-W4-1 `status` が `LastTest.log` 見本 / junit 見本から `tests {passed,failed,total}` を読み、無ければ `unknown` (`test_package_cli` Status*)。C-W4-2 `build_host_view` がホスト中心・依存右・`requires` 更に右、札 4 行、色 4 段、`validate_tela_graph` 通過、決定的 (`test_package_viz` HostView*)。C-W4-3 watch / status が無くても P4 が出る (札は `—`)。Ludellus-Native: C-W4-4 L1 で `mode=M1 (pictor=provided)`、L2 で `mode=M2 (pictor=source)`、L1→L2 で `suika_*` ターゲット集合と compile definitions 不変 (ホスト側 `cmake -P` 比較テスト) | Ludellus-Native の実ビルド (人の許可)。`tela_view` で P4 を開く |
| **W5** | `ergo_pkg update / add / remove` (`manifest_writer`: 整形保持)、`upm` 節、`unity/com.ludiars.ergo.packages/` (package.json / asmdef / Editor 窓 / DTO / EditMode テスト / Samples~ 見本)、`scripts/check-unity-package.*`、CI ジョブ | C-W5-1 `update --to <tag>` が `rev` と `expect_tag` を書き、他の行が 1 バイトも変わらない (見本と diff、`test_package_cli` Update*)。C-W5-2 `--to <sha>` で `expect_tag` 据え置き + 警告。C-W5-3 `add --kind link` で結線ファイルが無ければ終了 2、ファイルは変わらない。C-W5-4 `upm` 節追加で CMake の読み手の出力不変。C-W5-5 (Unity 不要の CI) package.json 必須キー、Editor 専用 asmdef、`Runtime/` 無し、見本の差分 0。C-W5-6 (EditMode) 見本 3 種の読み取り、`packages-lock.json` 照合 4 状態、既読管理、loopback 以外拒否 | UnityFoundation に git URL で導入し窓が開く (人の許可、未実施なら未実施と書く) |

共通の提出条件: 秘匿語辞書の照合 0 件、`spec/domains/observability.domain.json` と `.anatomia/layers.json` を
同じ PR で更新 (`tools/package_cli/`、`package_watch/`、`unity/` を足す PR)、`module_list.md` / `.yaml` /
`README.md` に行を足す。

---

## 6. 未決事項 (推奨つき)

人間 (neco) にしか決められないもの。いずれも W1〜W2 の着手を妨げない。

| ID | 論点 | 推奨 |
|---|---|---|
| N-W1 | 配送の担い手を Excubitor にする (Excubitor 側の PR が要る) か、Ergo 内に小さな送信 (webhook POST) を持つか | **Excubitor**。常駐・秘匿情報・redaction を増やさない。Excubitor の PR が遅れる間は `ergo_pkg watch` の標準出力で運用できる |
| N-W2 | Concordia の chitchat / 親セッションへも通知するか | 初版は送らない。報告ファイルを Concordia が読む形なら後から足せる (S7) |
| N-W3 | ビルド・テスト状態に CI (GitHub Actions) の結果を含めるか | 初版はローカルのビルドディレクトリだけ。CI はトークンと API が絡むので S7 |
| N-W4 | Unity 側で `git ls-remote` を直接呼び、Unity 単体で更新検知するか | 呼ばない。Unity は報告の読み手。Unity だけの開発機で検知が要るなら、Excubitor のジョブが同じ機械で報告を置く |
| N-W5 | 通知の頻度 (日次 1 回 / 変化があったときだけ 1 時間ごと) | 日次 1 回 (Excubitor の既存ジョブに合わせる)。緊急は手動 `watch` |
| N-W6 | `ERGO_PACKAGES_HOST_ID` をプロジェクトコード (`Ln`) にするかリポ名 (`Ludellus-Native`) にするか | プロジェクトコード (短く、`PROJECT-CODES.md` が正本)。報告には両方 (`id` / `name`) を載せられる |
| N-W7 | `update` が `expect_tag` の無い SHA を許すか (N4 との関係) | 非 STRICT では許す (据え置き + 警告)。STRICT は従来どおり構成時に止める |
| N-W8 | Unity パッケージの版番号を Ergo のタグに揃えるか | 独立 (`0.x`)。UPM の `version` は semver 必須で、Ergo の SHA 運用と合わない。導入は git URL の `#<sha>` で固定する |
| N-W9 | Ludellus-Native の L2 で `SUIKA_PICTOR_DIR` の別名を何リリース残すか | 1 リリース (次の Ludellus-Native のタグまで)。`DEPRECATION` 警告付き |
| N-W10 | Revisor の `GET /v1/local-prs` 読み取りに token が要るか | 要るなら `ERGO_PKG_REVISOR_TOKEN` 環境変数で受ける (設計は両対応)。実装時に Revisor の `auth.mjs` で確認する |

設計側で推奨どおり進めるもの: U-W1 報告 / 状態 / status はすべて JSON schema 1 で `spec/data/` に正本、
U-W2 HTTP は `ergo_http` を使い新規 third-party を入れない、U-W3 `ls-remote` は呼び出し環境の git を使う
(libgit2 を入れない)、U-W4 P4 は P1 の配置を再利用し新しい配置を書かない、U-W5 Unity は Newtonsoft を使わず
`JsonUtility` で読める平たい DTO にする。
