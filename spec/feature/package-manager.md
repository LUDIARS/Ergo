# パッケージマネージャ・更新の定期確認と通知 (統合設計)

- 日付: 2026-10-14
- 状態: 統合設計 (確定)。実装は §7 の段階 W1〜W7 で別に委託する
- 依頼: `spec/tasks/2026-10-14-package-manager-watch-design-request.md`
- 素案: `spec/plan/2026-10-14-package-manager-watch-fable.md` (Fable)、`spec/plan/2026-10-14-package-manager-watch-opus.md` (Opus)
- 前提: `spec/feature/package-hub.md` (以下「ハブ設計」)。S1〜S3 で決めたこと (マニフェストの `rev` がロックを兼ねる、40 桁 SHA、
  取り込み経路 6 通り、STRICT、PKG-xx、P1/P2) は変えない。本書はハブ設計の続きで、§7 の段階表を差し替える

## 人間の指示 (原文)

> Ergoはパッケージマネージャも兼ね、またPfやAIによるブランチ更新を定期的に確認して更新を通知するようにします。
> 現在作成しているLudellus-Nativeの管理や状態を確認できるようにしましょう。
>
> この通知部分とパッケージマネージャについては、Unity用のパッケージも用意します。

## 0. 2 案の比較と採否

両案は骨格で一致した。(1) Ergo に常駐は持たせず、1 回で終わる CLI `ergo_pkg` を足す。(2) 別の lock ファイルは作らない。
(3) 定期実行と Discord への配送は Excubitor の update ドメインに任せる。(4) Praeforma はブランチを作らない
(Concordia に委託を出すだけで、ブランチは委託先のセッションが作る)。(5) Ludellus-Native は新しい図 P4「ホスト状態」で見せる。
(6) Unity パッケージは Ergo の `unity/` に置き、C# は判定をしない。

| 論点 | Fable 案 | Opus 案 | 採用 | 理由 |
|---|---|---|---|---|
| ホスト側の宣言 | 作らない。変数 (`ERGO_PACKAGES` など) と `ERGO_PACKAGES_HOST_ID` だけ | ホストの `ergo-packages.json` に `host` 節 (Ergo 自身の pin・use・optional・features・旧変数の別名)。`ergo_pkg init` がブートストラップを書く | **Opus** | 「パッケージマネージャを兼ねる」には、ホストが何を使い Ergo のどの版に乗っているかを宣言する場所が要る。Ergo 自身の pin が無いと「Ln の Ergo が遅れている」を検知できない。ハブ設計 S1 の「ホスト側の別マニフェストは作らない」はこの指示で改める (§2.1)。変数での上書きは残す |
| 更新の種類 | tag / track / review / run | tag / main / review / merged / off-main (警告) / pin-behind-ergo | **和** | run (委託が進行中) は「来そうな更新」として指示の「AI によるブランチ更新」に直接当たる。merged・off-main・pin-behind-ergo は squash 運用と Ergo 経由の pin で必要。追跡ブランチは Fable の `track.branch` で可変にする |
| Pf / AI の区別 | 区別できないとして `origin: concordia` と run の属性を載せる | Revisor local PR の `sessionId` / `headRef` を Concordia run と突き合わせ、`triggered_by` で pf / ai / human / unknown | **Opus** | 指示が「Pf や AI による」と名指ししている。Pf の委託は `triggered_by: "praeforma"` で識別できる (Pf の `cc-client.ts`)。値は Pf 側で定数の契約にしてもらう (W9) |
| 頻度 | 日次 1 回 | 15 分ごと (ls-remote の指紋比較)、main は 6 時間に 1 回へ畳む、日次要約 | **Opus** | 「定期的に確認」の対象は AI の委託で、1 日に何本も動く。指紋比較なら 1 回の通信は 1 往復で軽い。日次要約も出す |
| 前回状態の置き場所 | 利用者ディレクトリ (`%LOCALAPPDATA%` / `$XDG_STATE_HOME`) | `--state` で受け、Excubitor のデータディレクトリ | **両方** | 定期ジョブは Excubitor が `--state` で自分のデータディレクトリを渡す。手で打つときの既定は利用者ディレクトリ。どちらもリポとビルドディレクトリには置かない |
| 取得 (fetch) | `cmake -P` の事前取得で FetchContent と同じ置き場所へ (取得エンジンを二重に持たない) | `<cache>/src/<name>-<sha12>` に展開し、追跡しない `CMakeUserPresets.json` に経路 2 の変数を書く | **Fable** | 取得の正本はハブの FetchContent 経路で、別の置き場所を作ると 2 系統になる |
| 遅れの距離 | 手元に履歴があるときだけ `rev-list --count` | ツリーレスの bare ミラー (`--filter=tree:0`) で常に距離を出す | **Opus (任意)** | 距離は通知の主要な情報。ミラーは blob も tree も持たないので軽い。ミラーが無ければ距離を空欄にする |
| Unity パッケージ名 | `com.ludiars.ergo.packages` | `com.ludiars.ergo` | **Fable** | 中身はパッケージ管理と通知だけ。`com.ludiars.ergo` は将来 Ergo の実行時部品を Unity に出すときのために空けておく |
| Unity の役割 | 読むだけ (更新コマンドをコピー) | 読む + UPM の `#<sha>` を書き換えて `Client.Resolve()` | **Opus** | 指示が「パッケージマネージャについても Unity 用を」。Unity では UPM がパッケージマネージャなので、その pin を書き換えられないと管理にならない。コミットはしない |
| Unity の宣言 | Ergo のマニフェストに top-level `upm` 節 | Unity プロジェクトを「UPM ホスト」として `Packages/manifest.json` と `.ergo/host.json` を読む | **Opus** | Unity プロジェクトの pin の持ち主はそのプロジェクト。Ergo のマニフェストに Unity の pin を持つと持ち主が 2 つになる |
| ビルドとテストの状態 | `CMakeCache.txt` の時刻 + `LastTest.log` か JUnit | JUnit + File API 返答の有無 | **和** | JUnit を優先し、無ければ ctest の既定出力 `LastTest.log` を読む |
| モジュール | `package_watch` だけ (編集は CLI 側) | `package_manager` (宣言の読み書き) + `package_watch` (判定) | **Opus** | 書式を保つ書き換えは単体テストが要るので、ライブラリに置く |
| 段階 | W1〜W5 | S4a〜S4f | **W1〜W7** (§7) | S4 と名前を分ける。Ludellus-Native は Fable の 2 段 (provided → source) に Opus の Ergo pin の段を前に足して 3 段 |

## 1. 解釈

- **パッケージマネージャ** = ホストがパッケージを宣言・追加・削除・更新 (pin の書き換え)・事前取得・照会できる CLI。
  取り込みそのものはハブの構成時 6 経路のまま。CLI はコミット・ブランチ作成・PR をしない (N7: pin の更新はホストの人が PR を出す)。
- **Pf による更新**: Praeforma のサーバは git に書かない (`project-git-activity.ts` は `.git/logs/HEAD` を読むだけ)。
  Pf は Concordia の `POST /v1/delegation/invoke` を `triggered_by: "praeforma"` で呼ぶ。ブランチは委託先セッションが作り、
  Revisor の local PR として審査に出る。よって「Pf 由来」は「`triggered_by` が praeforma の委託 run に属する変更」と定義する。
- **AI による更新**: それ以外の `triggered_by` の委託 run に属する変更。run に結び付かない local PR は人による更新とする。

## 2. パッケージマネージャ

### 2.1 ホストの宣言 (`host` 節)

ホストのリポ直下に、Ergo のマニフェストと同じ名前・同じ schema 番号の `ergo-packages.json` を置く。
`host` 節を持つファイルはホストの宣言で、Ergo のマニフェストとしては読まない (取り違えは構成エラー)。

```json
{
  "schema": 1,
  "host": {
    "code": "Ln",
    "ergo": { "repository": "https://github.com/LUDIARS/Ergo.git", "rev": "<40 桁 SHA>", "expect_tag": "v0.9.0" },
    "use": ["pictor"],
    "optional": ["pictor"],
    "features": { "pictor": "+vulkan" },
    "legacy_vars": { "SUIKA_ERGO_DIR": "ergo.source_dir", "SUIKA_PICTOR_DIR": "pictor.source_dir" }
  },
  "packages": {}
}
```

- `use` は `ERGO_PACKAGES` の既定値、`optional` は「ソースが見つからなければ使わない」、`features` は
  `ERGO_PACKAGE_FEATURES_<NAME>` の既定値。どれもキャッシュ変数で上書きでき、上書きは resolved.json に残る。
- `packages` は非公開パッケージだけを持つ (今の `ERGO_PACKAGE_MANIFEST_EXTRA` の中身)。Ergo のマニフェストとの同名は PKG-15。
- `legacy_vars` は移行中だけの別名表。S6 で警告、その後削除。
- `host.code` はプロジェクトコード (`LUDIARS/PROJECT-CODES.md` が正本)。resolved.json の `host.id` に写す。宣言の無いホストは
  `ERGO_PACKAGES_HOST_ID` → `CMAKE_PROJECT_NAME` の順。
- 共有パッケージ (Pictor など) の pin はこれまでどおり Ergo のマニフェストが持つ。ホストは Ergo の pin を上げることで
  共有パッケージの pin も受け取る。

**Ergo 自身を取る仕組み**: `ergo_pkg init` がホストに `cmake/ergo-bootstrap.cmake` (30 行程度) を書く。`string(JSON)` で
`host.ergo` を読み、`ERGO_SOURCE_DIR` → 兄弟チェックアウト → 取得 (明示的に ON のときだけ) の順で Ergo を見つけて
`add_subdirectory` する。先頭に生成元の Ergo SHA を入れ、`ergo_pkg doctor` が古さを検出する。Ergo 自身にも PKG-02/04/05/07 を当てる。

### 2.2 CLI `ergo_pkg`

ターゲット `ergo_pkg_cli`、置き場所 `tools/package_cli/` (`main.cpp` は配線だけ、コマンドごとに 1 ファイル)、
`ERGO_BUILD_PACKAGE_CLI` で切り替える。終了コードは `ergo_package_viz` と同じ: 0 成功 / 1 要対応の検出あり /
2 引数・入力の誤り / 3 書けない・取れない。

| コマンド | 何をするか | 書き換えるもの | ネットワーク |
|---|---|---|---|
| `init [--code <c>] [--upm]` | ホストの宣言とブートストラップを作り `.gitignore` に `.ergo/` を足す。`--upm` は Unity プロジェクト用に `.ergo/host.json` だけ作る。2 回目は無変更 | ホスト | 無し |
| `list [--json]` | 宣言・resolved.json・UPM 宣言を合わせた一覧 | 無し | 無し |
| `doctor [--strict] [--json]` | 宣言の検査 (PKG 規則を CLI から)、ブートストラップの古さ、漏れ検査 (§6)。`--strict` は構成時の STRICT と同じ判定 | 無し | 無し |
| `add <name> [--optional] [--features ...]` | `host.use` に足す。Ergo のマニフェストに無い名前は `--repository/--rev` を要求して非公開 `packages` へ。`link` の結線ファイルが無ければ終了 2 (生成しない) | ホスト | 任意 |
| `remove <name>` | `host.use` / 非公開 `packages` から外す。`modules.*.needs` を壊すなら拒否 | ホスト | 無し |
| `update <name> --to <tag\|main\|sha>` | 対象を 40 桁 SHA に解決して `rev` と `expect_tag` を書く。`ergo` と非公開はホスト、共有パッケージは `--ergo <dir>` の Ergo マニフェスト。取り違えは終了 2 | 持ち主 | 要 |
| `fetch [<name>...]` | 各 `repository@rev` を FetchContent と同じ置き場所へ事前取得 (`cmake/packages/ErgoPackagePrefetch.cmake` を起動)。オフラインでも経路 5 が当たるようにする | キャッシュのみ | 要 |
| `outdated [--json] [--fail-on <kind>]` | §3 の判定を今すぐ行って表示。前回状態は読み書きしない | 無し | 要 (無ければ未確認) |
| `watch --host <dir>... [--state <dir>] [--out <dir>] [--revisor-url <u>] [--concordia-url <u>]` | 定期実行の本体。判定 → 前回状態と差分 → 新しい出来事を `events.json` に、ホストごとの報告を `<host>/.ergo/watch-report.json` に | 状態と報告のみ | 要 |
| `status --host <dir> --build <dir> [--out <file>]` | ビルドとテストの状態を `ergo-packages.status.json` に | 出力のみ | 無し |

- `update` は書式を保って書き戻す (キー順・改行・字下げを変えない。変えるのは該当の値だけ)。終わりに次の操作
  (どのリポで feat ブランチを切って PR を出すか、STRICT で通るか) を表示し、実行はしない。
- `--to main` / `--to <sha>` でタグの無い pin を書くときは STRICT で通らないと警告する (非 STRICT では許す)。
  main の祖先でない SHA (squash で消える feature の SHA) は `--allow-off-main` が無ければ書かない。
- 構成時 (CMake) はこれまでどおりネットワークに出ない。出るのは明示的に打った `ergo_pkg` と定期ジョブだけ。

### 2.3 取得元とオフライン

- タグと追跡ブランチの先頭は `git ls-remote --tags --heads <repository>`。前回の指紋と同じなら以降を省く。
- 距離 (`behind N`) と祖先判定は、任意のツリーレス bare ミラー `<cache>/mirrors/<Repo>.git`
  (`git clone --bare --filter=tree:0`)。ミラーが無ければ距離は空欄。`<cache>` は `--cache` > `ERGO_PKG_CACHE_DIR` >
  `ERGO_DEPENDENCY_CACHE_DIR`。
- 兄弟チェックアウト (`ERGO_PACKAGES_SIBLING_ROOT`) があれば、その `refs/remotes/origin/*` と `refs/tags/*` を読む (fetch しない)。
  判定は「最後に fetch した時点の値」で、報告に `as_of: "sibling-last-fetch"` を載せる。兄弟チェックアウト自体が古いことは
  `sibling_behind` で区別する。`.worktrees` は見ない。
- 取れなかったパッケージは `observed: false` と理由を載せ、「更新なし」とは書かない。その回は前回状態を書き換えない。
- git の資格情報は利用者の git に任せる。`ergo_pkg` はトークンを引数・ファイルで受け取らない。

## 3. 更新の検知

依存 1 つ (ホスト H がパッケージ P を pin S で使う、または Ergo のマニフェストが P を S で持つ) ごとに判定する。

| `kind` | 条件 | 主体 (重複判定の鍵) | 取得元 |
|---|---|---|---|
| `tag` | 前回に無い `v*` タグが現れ、S の子孫で `expect_tag` より新しい。複数なら最新 1 つ | タグ名 | ls-remote |
| `track` | 追跡ブランチ (`track.branch`、既定 `main`) の先頭 M が S と違い、S が M の祖先。距離を添える | M の SHA | ls-remote + ミラー |
| `run` | P のリポを対象にした Concordia の委託 run が進行中 | run id | Concordia `GET /v1/delegation/runs` |
| `review` | P のリポで base が main の open な Revisor local PR がある | local PR id + headSha | Revisor `GET /v1/local-prs?view=summary&state=open` |
| `merged` | 前回 `review` だった local PR が merged になった | local PR id | 同上 |
| `off-main` (警告) | S が追跡ブランチの祖先でない | S | ミラー |
| `pin-behind-ergo` | ホストの Ergo pin が、Ergo の main で上がった共有パッケージの pin を含まない | Ergo の main SHA | Ergo のマニフェスト |

同じ変更が `run` → `review` → `merged` → `track` → `tag` と進むのは正常で、種類ごとに別の出来事にする。

**由来** (`run` / `review` / `merged` に付ける):

| 由来 | 判定 |
|---|---|
| `pf` | local PR の `sessionId` が run の `child_session_id` と一致 (無ければ `headRef == spawn_branch` かつ同じリポ) し、run の `triggered_by == "praeforma"` |
| `ai` | 同じ突合で run が見つかり、`triggered_by` が praeforma 以外 (値を `trigger` に写す) |
| `human` | Concordia に届き、突合する run が無い |
| `unknown` | Concordia に届かない・認可されない |

Revisor / Concordia の URL は引数か環境変数 (`ERGO_WATCH_REVISOR_URL` / `ERGO_WATCH_CONCORDIA_URL`) で受け、loopback 以外は
終了 2。ポートの数字は Ergo に書かない (Excubitor の catalog が正本で、ジョブが渡す)。読み取りの認可は要らない (W4 で確認:
Revisor の `GET /v1/local-prs` と Concordia の `GET /v1/delegation/runs` は loopback から認可なしで読める) ので、トークンは受け取らない。
届かないときは `run` / `review` を出さず `sources.* = "unavailable"`。

**頻度**: 定期ジョブは 15 分ごと。1 回の通信はリポごとの ls-remote 1 往復と、Revisor / Concordia の一覧各 1 回。

**前回状態と重複の抑止**:

- 状態ファイルはホストごとに `(依存, kind, 主体)` の既知集合・最初に見た時刻・最後に通知した時刻・リポごとの指紋を持つ。
- 同じ `(host, 依存, kind, 主体)` は 1 回だけ通知する。既知の未解消は `pending` に残し、日次要約に出す。
- `track` は進むたびに主体が変わるので、同じ `(host, 依存)` は 6 時間に 1 回へ畳み、その間の距離を合算する。
- pin が上がって条件を満たさなくなったもの、終局した `run` / `review` は状態から消す (再発は新規)。
- 判定は純粋関数 `diff(prev_state, observations, now) → (events, next_state)` にし、時刻は引数で渡す。

## 4. 通知

`ergo_pkg watch` は判定と JSON の書き出しまで。配送は Excubitor の update ドメインのジョブが行う
(`package-update-audit` と同じ形: Task Scheduler / cron で 1 回起動、Discord webhook、秘匿の redaction、2,000 字の分割)。
Excubitor に足すのは「catalog で `ergo_watch: true` の checkout を `--host` に並べて `ergo_pkg watch` を起動し、
`events.json` を整形して送る」ジョブ 1 本で、Excubitor の PR として出す (Ergo の PR には含めない)。

| 通知先 | 形式 | 誰に |
|---|---|---|
| Discord (Excubitor の update 用 webhook) | 1 出来事 1 件 + 日次要約 1 本。非公開ホストは別 webhook (catalog の設定) | 開発者 |
| CLI | `outdated` / `watch` の表、`--json` | 手で打った人 |
| 構成時の 1 行 | 状態ファイルがあれば `-- ergo packages: 2 update(s) known for host Ln` を出す。無ければ何も出さない (ネットワークに出ない) | `cmake` した人 |
| Tela | P4 ホスト状態図 (§5) | 図を開いた人 |
| Unity | 窓の一覧と未読数、新着で 1 回だけ通知 (§6.5) | Unity の開発者 |

本文の例:

> **Ln の依存 Pictor に新しいタグ v2.3.0** (今の pin は v2.2.0 / 625d83d、41 コミット先)
> 持ち主: Ergo のマニフェスト → Ergo で `ergo_pkg update pictor --to v2.3.0` を打ち、feat ブランチで PR。STRICT で通ります。

> **Ergo の依存 Tela に審査中の更新** (Revisor local PR #128、由来: Pf の委託、head 4f0e9a1)。マージ後に main の更新として知らせます。

載せるもの: ホストのコード、依存名とコード、持ち主、種類、前後の版 (タグと SHA7)、距離、由来、local PR の番号、次の操作、STRICT で通るか。
載せないもの: §6。

## 5. Ludellus-Native の状態確認

### 5.1 P4 ホスト状態図 (`--view host`)

P1 (版一覧) と P2 (リンク図) は構成の形を見る図、P4 は 1 ホストの今の状態を見る図。配置は `ergo_graph_layout` を再利用し、
`package_viz/host_view.*` を 1 組足す。

| グループ (8 以内) | ノード | 札 |
|---|---|---|
| host | ホスト 1 | コード / HEAD SHA7 / モード (M0〜M2) / テスト `12/12`・`3 失敗`・`未実行` |
| Ergo | Ergo 1 | 版 / pin SHA7 / 遅れ (`main +18`、`新タグ v0.10.0`) |
| 依存 | Pictor など | 版 / pin / 経路 / drift / 機能 / 印 (`★drift` `dirty` `off-main`) |
| 更新: タグ・main | 依存ごと | `v2.3.0 (41 先)` / `+41 (うち merged 3)` |
| 更新: 審査中・作業中 | local PR / run | `#128 Pf` / `#131 AI` / `#133 人` / `run AI` |
| 検査 | 落ちた PKG-xx | ID と対象 |

辺: host → 依存 (実線)、依存 → 更新 (破線)、依存 → 検査 (点線)。上限を超えたら由来ごとに 1 ノードへ畳んで件数を札に出す。
入力: `<build>/ergo-packages.resolved.json`、`<host>/.ergo/watch-report.json`、`ergo-packages.status.json`。どれかが無くても図は出し、札は `—`。

`ergo-packages.status.json` は `ergo_pkg status` が書く: 構成時刻 (`CMakeCache.txt`)、テスト結果 (`--output-junit` の XML を優先、
無ければ `Testing/Temporary/LastTest.log`)、無ければ `unknown`。ビルドの成否と CI の結果は初版では取らない。

### 5.2 Ludellus-Native の移行 (Ludellus-Native 側の PR、1 段 1 PR)

現状: `SUIKA_ERGO_DIR` (既定 `native/external/ergo`) を必須で `add_subdirectory` し、`ERGO_BUILD_*` を FORCE。その後
`SUIKA_PICTOR_DIR` があれば Pictor を `add_subdirectory` して `PICTOR_BUILD_*` を FORCE。Ergo の SHA は固定していない。

| 段 | 変更 | 確認 |
|---|---|---|
| L1 Ergo の pin | `ergo_pkg init --code Ln` で宣言 (Ergo の SHA、`use` は空) とブートストラップを足す。Ergo の取り込みをブートストラップに置き換え、`SUIKA_ERGO_DIR` は `legacy_vars` で `ERGO_SOURCE_DIR` の別名 (渡されれば使い、DEPRECATION を出す)。M0 のまま | 同じ `SUIKA_ERGO_DIR` でターゲット集合と `ctest -N` が変わらない。resolved.json に Ergo の drift が出る |
| L2 照合 (M1) | `use` / `optional` に `pictor`。Pictor の取り込みはホストのまま (Ergo より後に作るので `provided` 経路)。`SUIKA_PICTOR_DIR` が無いときは使わない | `mode=M1 (pictor=provided)`。PKG-05 と PKG-15 (`PICTOR_BUILD_*` の FORCE) が記録される |
| L3 委譲 (M2) | Pictor の `add_subdirectory` と FORCE を消し、`SUIKA_PICTOR_DIR` は `ERGO_PACKAGE_SOURCE_DIR_PICTOR` の別名に。`PICTOR_ENABLE_RIVE` は `features` / キャッシュ既定で渡す | `mode=M2 (pictor=source)`。描画ターゲットの有無と compile definitions が L2 と同じ。PKG-15 が消える |

- `ERGO_BUILD_*` によるモジュール選択はホストに残す。FORCE は「未設定なら既定」に直すよう L1 で推奨する。
- `spec/setup/native-build.md` と CMake のエラー文にある開発機の絶対パス例は L1 で記号 (`<ergo>`) に置き換える。
- 定期ジョブへの登録は Excubitor catalog の Ludellus-Native に `ergo_watch: true` を足す (Excubitor 側)。

## 6. Unity パッケージ

### 6.1 形式と置き場所

`unity/com.ludiars.ergo.packages/` (UPM、Editor 専用)。Tela の `unity/com.ludiars.tela` と同じ「所有リポに置く」形で、
報告の形式を変える PR と同じ PR で直せる。導入は
`"com.ludiars.ergo.packages": "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#<40 桁 SHA>"`。
版は Ergo のタグと独立の `0.x`。依存は `com.unity.modules.jsonserialize` だけ (`JsonUtility` で読める平たい DTO)。

```
unity/com.ludiars.ergo.packages/
  package.json  README.md  CHANGELOG.md
  Editor/Core/Ergo.Packages.Core.asmdef      noEngineReferences: true (純 C#)
    UpmManifestReader.cs   PackagesLockReader.cs   WatchReportReader.cs
    PinPlanner.cs          (ブランチ名と短縮 SHA を拒否し、#<40 桁 SHA> の URL を作る)
    ManifestJsonEditor.cs  (manifest.json の該当 1 値だけを書き換え、他はバイト単位で保つ)
  Editor/Ergo.Packages.Editor.asmdef
    ErgoPackagesWindow.cs  (Window > LUDIARS > Ergo Packages)
    UpdateNotifier.cs      UpmApplier.cs (書き換え → Client.Resolve())
  Tests/Editor/            EditMode テスト + Fixtures (Ergo 本体の見本の写し)
```

### 6.2 Unity 側の機能

| 機能 | 中身 |
|---|---|
| パッケージ一覧 | `Packages/manifest.json` のうち LUDIARS のリポを指すもの: 名前 / コード / 宣言の rev / `packages-lock.json` の hash / `#main` などブランチ指定の警告 |
| 来ている更新 | 報告 (`<project>/.ergo/watch-report.json`) の出来事 (由来つき)。報告の時刻と `observed` を出し、古い・未観測を「更新なし」と出さない |
| 更新の適用 | 出来事の「この版にする」→ 差分を確認ダイアログで見せる → manifest.json を書く → `Client.Resolve()`。コミットはしない |
| 通知 | 窓のタブに未読数、新着で `ShowNotification` を 1 回。既読は出来事 id で管理 |

レジストリ・`file:` の依存は一覧に出すが判定の対象外 (理由を表示する)。

### 6.3 ネイティブ側との共有

- 共有するのは **形式** (`spec/data/ergo-watch-report.md`、schema 1) と **pin の規則** (40 桁 SHA、タグは注釈) だけ。判定は C++ にしか無い。
- `ergo_pkg watch` は Unity プロジェクトを「UPM ホスト」として読む: `ergo-packages.json` が無く `Packages/manifest.json` がある
  ディレクトリで、`host.code` は `.ergo/host.json` (`init --upm` が作り、追跡する) から読む。
- リポとコードの対応は C# に持たず、報告に載せて渡す。

### 6.4 検証

| 検証 | 機械で確かめる条件 |
|---|---|
| 形式の一致 | Ergo の `tests/package_watch/fixtures/*.json` と Unity 側 Fixtures がバイト一致 (`cmake -P`、Unity 不要) |
| Core の単体 | Core は `noEngineReferences` なので `dotnet test` で回せる。Unity 上では同じテストを EditMode で回す |
| 構造 | `package.json` の必須キー、asmdef が Editor 専用、`Runtime/` が無い (CI で 1 ジョブ) |
| 手元確認 | UnityFoundation に git URL で入れ、窓が開く・報告が出る・pin の書き換え後に Resolve される。人の許可のもとで行い、未実施なら未実施と書く |

## 7. 段階表 (ハブ設計 §7 の S4 以降を差し替える)

S4 (観測 P3) は並行で実装中。W は S4 が触るファイル (`package_model` の scan、`package_viz` の usage 図) に触らない。
`package_viz` の CMake 列挙への行追加だけは重なるので、W6 は S4 のマージ後に入れる。

| 段階 | 内容 | 先行 |
|---|---|---|
| S4 観測 P3 | ハブ設計どおり (並行) | S3 |
| **W1** 宣言と読み取り | `host` 節のスキーマ、`ergo_package_manager` (読む・検査)、`ergo_pkg init / list / doctor`、ブートストラップ、resolved.json の `host.id`、`fetch` (事前取得) | S3 |
| **W2** 書き換え | `add / remove / update` (書式を保つ書き換え)、持ち主の判定、off-main の拒否 | W1 |
| **W3** 検知 | `ergo_package_watch` (ls-remote・ミラー・兄弟の読み手、状態、重複抑止、報告と出来事の書き手)、`outdated / watch`、`track` キー、`spec/data/ergo-watch-report.md` | W1 |
| **W4** 由来 | Revisor / Concordia の読み手、`run / review / merged`、pf / ai / human / unknown | W3 |
| **W5** 配送 | Excubitor のジョブ (Excubitor の PR)、構成時の 1 行 (Ergo) | W3 |
| **W6** P4 と Ludellus-Native | `ergo_pkg status`、`host_view`、`--view host`、`ergo_package_graphs` に P4。Ludellus-Native の L1〜L3 (Ludellus-Native の PR) | W4、S4 |
| **W7** Unity | `unity/com.ludiars.ergo.packages`、UPM ホストの読み手、CI の構造検査 | W3 |
| S5 ホスト移行 (他ホスト) | ハブ設計どおり。Ludellus-Native は W6 で先行 | W6 |
| S6 互換の既定変更 | M0 警告に加えて `legacy_vars` の警告 | S5 |
| S7 任意 | ハブ設計の S7 に GitHub の公開 PR の取得・自動 PR・CI 結果の取り込みを足す | — |

モジュールの向き: `ergo_package_watch → ergo_package_manager → (ergo_common)`、`ergo_package_viz → ergo_package_model`、
`tools/package_cli → package_manager / package_watch / package_viz`。git プロセスと HTTP は `tools/package_cli/` のアダプタに置き、
ライブラリは `IRefSource` / `IReviewSource` / `IRunSource` だけを持つ (テストは偽ソース)。HTTP は既存の `ergo_http`。
新しいトップレベル (`unity/`) と新モジュールは同じ PR で `spec/domains` と `.anatomia/layers.json` を更新する。

実装状況: W1 は `spec/tasks/2026-10-14-package-manager-w1.md`、W2 は `spec/tasks/2026-10-15-package-manager-w2.md` (モジュール `spec/module/package_manager.md`)。
W3 は `spec/tasks/2026-10-15-package-manager-w3.md` (モジュール `spec/module/package_watch.md`、形式 `spec/data/ergo-watch-report.md`)。
W4 は `spec/tasks/2026-10-15-package-manager-w4.md` (同じモジュールと形式に由来を足した)。

### 受入条件 (機械で確かめる。テストはネットワークを使わない: git はテスト内の偽リポ、Revisor / Concordia は JSON 見本)

| 段階 | 受入 |
|---|---|
| W1 | `host` 節の見本 3 種 (host のみ / 非公開 packages あり / 不正) の読み取りと拒否理由。`host` 節のファイルを Ergo マニフェストとして読むと構成エラー。非公開名と Ergo マニフェストの同名で PKG-15。`init` を 2 回走らせて 2 回目が無変更・`.gitignore` に `.ergo/` が 1 行。偽 Ergo + 偽ホストでブートストラップが `ERGO_SOURCE_DIR` → 兄弟の順に解決し、取得 OFF で通信しない。`doctor` がブートストラップの古さと漏れ (§8) を真偽両方で検出。`fetch` 後の `ERGO_PACKAGES_FETCH=ON` 構成がネットワーク無しで経路 5 に当たる。M0 の結果不変 (`test_packages_m0_invariance` 継続) |
| W2 | `update --to <tag>` が `rev` と `expect_tag` だけを書き、他がバイト単位で同じ。共有パッケージは `--ergo` 側、`ergo` と非公開はホスト側 (取り違えで終了 2)。feature ブランチ / main の祖先でない SHA を拒否、`--allow-off-main` で通る。`--to main` で STRICT 警告。`remove` が `needs` を壊すと拒否。`add --kind link` で結線ファイルが無ければ終了 2 でファイル不変 |
| W3 | tag / track / off-main の判定 (距離・semver・複数タグの最新 1 つ)。`diff` の決定性 (同じ入力で同じバイト列)。同じ主体の 2 回目で出来事 0、`track` の 6 時間畳み込み、pin を上げると解消。取得失敗で `observed:false`、前回状態不変。兄弟では fetch せず `as_of` が付く。指紋が同じならミラーを fetch しない (偽ソースの呼び出し回数)。報告と出来事が漏れ検査を通り、通らない入力では書かずに終了 3。`track` キーの追加で CMake の読み手の出力不変 |
| W4 | local PR 見本 + run 見本の突合で pf / ai (trigger 写し) / human / unknown。open → merged で `merged` が 1 回。Revisor / Concordia が落ちていると `run`/`review` を出さず `unavailable`。loopback 以外の URL で終了 2。出来事に PR のタイトル・本文・ブランチ名が入らない (見本にわざと入れて確かめる) |
| W5 | Ergo: 状態ファイルが無い / 壊れている / 別ホストのとき何も出さず構成を止めない。Excubitor: 出来事の見本から Discord 本文を作り秘匿が無い、出来事 0 件で「新規なし」(Excubitor の PR で定める) |
| W6 | resolved + 報告 + JUnit 見本から P4 を作り `validate_tela_graph` を通る。審査中 20 件で由来ごとに畳まれ上限内。入力が欠けても図が出る (札 `—`)。決定的。S4 の usage 図のテストが無変更で通る。Ludellus-Native: L1 でターゲット集合不変、L2 で `mode=M1`、L3 で `mode=M2` と compile definitions 不変 |
| W7 | Fixtures のバイト一致。Core: manifest.json の git 依存の読み取り、`#main` の警告、PinPlanner がブランチ名と短縮 SHA を拒否、ManifestJsonEditor が 1 値だけ変える、`observed:false` を「更新なし」にしない。`watch` が UPM ホスト見本から出来事を出す。Unity 実機は手元確認 (未実施なら未実施と書く) |

## 8. 公開リポとしての注意

| 対象 | 規則 |
|---|---|
| 追跡するファイル (マニフェスト、ブートストラップ、spec、見本、`unity/**`) | 絶対パスを書かない。パスはリポ相対か記号 (`<ergo>` `<cache>` `<build>`)。リポは公開 URL か略称だけ。見本の `host.code` は `Example` |
| 非公開パッケージ・非公開ホスト | 非公開ホストの宣言にだけ書く。報告では `private: true` にし `repository` を伏せる |
| 前回状態・ミラー・キャッシュ | リポの外 (Excubitor のデータディレクトリ、利用者ディレクトリ、`<cache>`)。ホストの `.ergo/` は `init` が `.gitignore` に足す |
| 報告・出来事・通知の本文 | パス・URL・ポート・トークン・セッション id・PR のタイトルと本文・ブランチ名 (委託のタスク名を含み得る) を載せない。local PR は番号と headSha7 だけ |
| 認証 | トークンを引数・設定ファイル・報告・ログに書かない。webhook は Excubitor の暗号化設定にだけある |

機械検査: `doctor` と Ergo のテストに漏れ検査を入れる。拒む形 = ドライブ文字始まり、`/Users/` `/home/`、`\\` 始まりの UNC、
`token` / `secret` / `Authorization` をキーに持つ値。報告と出来事は書く前に同じ検査を通し、当たれば書かずに終了 3。
各 PR の提出前に秘匿語辞書で照合して 0 件を確かめる。

## 9. 未決事項 (推奨で進める。該当段階の前に確認する)

| ID | 論点 | 推奨 |
|---|---|---|
| W-1 | 配送を Excubitor に任せる (Excubitor の PR が要る) | 任せる。Excubitor の PR が入るまでは `watch` の標準出力と報告ファイルで使える |
| W-2 | 頻度 | 15 分ごと、`track` は 6 時間に 1 回へ畳む、日次要約を朝 1 回。多すぎれば `track` を日次要約だけにする |
| W-3 | 通知チャンネル | 公開ホストは 1 チャンネル、非公開ホストは別 webhook。メンションは付けない |
| W-4 | `update` の後に PR を自動で作るか | 作らない。自動化するなら Concordia の委託として起こす (S7) |
| W-5 | GitHub の公開 PR も審査中に入れるか | 入れない (主経路は Revisor の local PR) |
| W-6 | ビルドの成否・CI の結果を P4 に出すか | 初版は構成時刻とテスト結果まで |
| W-7 | `ergo_pkg` の配布 | 初版は配布しない (定期ジョブは Ergo の checkout でビルドしたものを使う) |
| W-8 | Unity パッケージの版を Ergo のタグに揃えるか | 揃えない (`0.x`、導入は `#<sha>`) |
| W-9 | Pf 由来の判定を `triggered_by` に頼る | 頼る。Pf の `'praeforma'` を定数にして spec に書いてもらう (Pf 側の PR) |
| W-10 | `SUIKA_*` の別名を残す期間 | Ludellus-Native の次のタグまで (DEPRECATION 警告つき) |
