# パッケージマネージャ・更新の定期確認と通知・Ludellus-Native の状態確認・Unity パッケージ (Opus 案)

- 日付: 2026-10-14
- 対象: Ergo (Eg)。Unity 向けパッケージの置き場所を含む
- 依頼: `spec/tasks/2026-10-14-package-manager-watch-design-request.md` (コミット 7843ae8)
- 前提の正本: `spec/feature/package-hub.md` (以下「ハブ設計」。§n はその節)
- 種別: 設計の素案 (Opus 側)。実装・ビルド・実ホストでの確認は含まない。もう一方の案は見ていない。

---

## 0. 結論

1. **Ergo は常駐サービスを持たない。** Ergo が足すのは「1 回走って JSON を書いて終わる」CLI `ergo_pkg` と、
   それを支える純粋な C++ ライブラリ 2 つ (`ergo_package_manager` = 宣言の読み書き、`ergo_package_watch` = 更新の判定) だけ。
   定期実行は **Excubitor の既存 update ドメイン** (Task Scheduler / cron で 1 回起動、常駐なし、Discord webhook で配送) に任せる。
   Excubitor には `src/update/checker.ts` (git behind/ahead) と `package-update-audit` (日次の npm 更新監査) という同型の先例がある。
2. **ロックファイルは作らない。** ハブ設計どおり `rev` (40 桁 SHA) がロックを兼ねる。足すのはホスト側の宣言
   (`ergo-packages.json` の `host` 節) で、そこに **Ergo 自身の pin** と使うパッケージを書く。共有パッケージ (Pictor など) の pin は
   これまでどおり Ergo のマニフェストが持ち (N7)、`ergo_pkg update pictor` は「Ergo 側を上げる変更」を作る。
3. **Pf はブランチを作らない。** Praeforma は Concordia の `POST /v1/delegation/invoke` を `triggered_by: "praeforma"` 付きで呼ぶだけで
   (`Praeforma/server/src/lib/cc-client.ts` の `invokeDelegation`)、ブランチ・コミット・PR は委託先の AI セッションが作る。
   したがって「Pf 由来」と「AI 由来」は **どちらも Concordia の委託 run** で、`triggered_by` の値で分ける。審査中の更新は
   Revisor の local PR (`headRef` / `headSha` / `sessionId`) から取り、run の `child_session_id` / `spawn_branch` と突き合わせて由来を付ける。
4. **Ludellus-Native は最初の実ホストとして S5 の先頭で移す** (`SUIKA_ERGO_DIR` → ホスト宣言 + ブートストラップ)。状態確認は
   Tela の新しい図 **P4 ホスト状態** (`--view status --host <code>`) で、版・遅れ・来ている更新 (由来つき)・ビルドとテストの結果を 1 枚に出す。
5. **Unity パッケージは Ergo リポ内 `unity/com.ludiars.ergo/`** に置く (Tela の `unity/com.ludiars.tela` と同じ置き方)。
   Unity 側は **判定をしない**: 共有するのは報告ファイルの形式 (`ergo-watch-report.json`) と UPM の pin 規則だけ。C# は報告を読んで
   エディタ窓に出し、更新の適用は UPM の `Packages/manifest.json` の `#<sha>` を書き換えて `Client.Resolve()` するだけ。
   判定は `ergo_pkg` が UPM の `manifest.json` / `packages-lock.json` を入力に取って行う (Unity プロジェクトも同じ見張りに乗る)。

---

## 1. 解釈 (親の解釈との違い)

| # | 親の解釈 | この案の解釈 | 根拠 |
|---|---|---|---|
| 1 | `ergo pkg add/remove/update/outdated/fetch/lock`。lock ファイルの要否は論点 | `lock` は作らない。`add/remove/update/outdated/fetch` に `init/list/doctor/watch/status` を足す。**誰の宣言を書き換えるか**はパッケージの持ち主で決まる (共有パッケージは Ergo、Ergo の pin と非公開パッケージはホスト) | ハブ設計 §2「マニフェストがロックを兼ねる」、N7。ロックを別に持つと pin の正本が 2 つになる |
| 2 | Pf がブランチを作る・更新する | Pf は**委託を起こすだけ**。ブランチを作るのは委託先セッション。Pf 由来の判定は run の `triggered_by == "praeforma"` | `Praeforma/server/src/routes/cc.ts` (`/send` → `invokeDelegation`)、`cc-client.ts` (`triggered_by: 'praeforma'`)。Pf の git 読み取りは `project-git-activity.ts` の reflog 読みだけ (書き込み無し) |
| 2 | 更新 = 新タグ / main の進み / 審査中ブランチ | 加えて **「pin が main に無い」** (squash merge で feature の SHA が main に残らない) を更新ではなく警告として出す | ハブ設計の pin は main 上の SHA を前提にしており、squash 運用 (Ergo CLAUDE.md) では feature の SHA を pin すると追跡不能になる |
| 2 | 定期実行の担い手は論点 | Ergo には持たせない。Excubitor の update ドメインのジョブ | 下記 §3。Excubitor `spec/feature/package-update-audit.md` が「追加の常駐を作らない、Task Scheduler で CLI を 1 回起動」を既に決めている |
| 3 | Ludellus-Native は M0 のまま、描画を Pictor 経由にする段階で M2 (ハブ設計 §6) | **既に `SUIKA_PICTOR_DIR` で Pictor を任意に `add_subdirectory` し、`PICTOR_BUILD_*` を FORCE している**ので M1 (照合) に入れる。順は L1 (Ergo の pin) → L2 (M1) → L3 (M2) | `Ludellus-Native/native/CMakeLists.txt` L54-96 (Ergo)、L172-187 (Pictor) |
| 4 | Unity から通知とパッケージマネージャを使う | Unity では **UPM がパッケージマネージャそのもの**。Ergo の Unity パッケージは UPM の上に「LUDIARS パッケージの版一覧・来ている更新・pin の書き換え」を足す。CMake 系の解決は持ち込まない | UPM は git URL + `packages-lock.json` の `hash` で既に SHA を固定している |
| 4 | 置き場所は Ergo `unity/` / Uf / 新規リポ | Ergo `unity/com.ludiars.ergo/` | 報告形式・pin 規則を変える PR とパッケージを同じ PR で直せる (Ergo CLAUDE.md「横断変更は 1 PR」)。Uf は Unity プロジェクト (Unity 6000.0.59f2) で UPM パッケージの配布元ではない (`UnityFoundation/unity/`、`dist/*.unitypackage`) |

---

## 2. 比較した案

### 2.1 定期実行の担い手

| 案 | 内容 | 採否 | 理由 |
|---|---|---|---|
| A. Ergo に常駐を持たせる | `ergo_pkg serve` が定期にポーリングして通知 | 不採用 | Ergo は C++ ライブラリ群。プロセスの起動・再起動・ポートは Excubitor の領分 (port-source-rule)。常駐が 1 本増えるだけで得るものが無い |
| B. **Excubitor のジョブ** | Task Scheduler / cron が `ergo_pkg watch` を 1 回起動、結果を Excubitor の Discord 配送に流す | **採用** | 先例 (`package-update-audit`) と同じ形。見張る checkout の一覧 (catalog) を Excubitor が既に持つ |
| C. CI (GitHub Actions) | 定期 workflow | 不採用 (補助としても初版は持たない) | Revisor の local PR と Concordia の run は loopback にしか無く、CI からは見えない。審査中の更新が取れない |
| D. エディタ / CMake 構成時 | 起動時に確かめる | **補助として採用** | 定期ではないが「今の状態」を手元で見る入口として `ergo_pkg outdated` と Unity 窓の「今すぐ確認」を置く。構成時にはネットワークに出ない (ハブ設計の既定 OFF を守る) |

### 2.2 ロックファイル

| 案 | 採否 | 理由 |
|---|---|---|
| A. `ergo-packages.lock.json` を別に持つ (宣言は範囲、ロックは SHA) | 不採用 | 宣言を範囲 (`^2.2`) にすると STRICT のタグ要求 (N4) と pin 照合 (PKG-05) の基準が 2 つになる。LUDIARS のタグは major/minor だけで範囲指定の利得が小さい |
| B. **`rev` がロックを兼ねる** (ハブ設計のまま) | **採用** | `update` は「タグかブランチ名 → SHA に解決して `rev` と `expect_tag` を書く」操作にする。人が読む版は `expect_tag` と図の版札で足りる |

### 2.3 Unity との共有のしかた

| 案 | 採否 | 理由 |
|---|---|---|
| A. C# で判定まで再実装 | 不採用 | git の祖先判定・Revisor / Concordia の突合を 2 言語で持つと必ずずれる |
| B. Unity から毎回ネイティブ CLI を呼ぶ | 一部採用 (任意) | `ergo_pkg` の配布 (各 OS のバイナリ) が要る。CLI のパスが設定されていれば「今すぐ確認」で呼ぶ |
| C. **報告ファイルを読む** | **採用** | 見張り (Excubitor のジョブ) がホストごとに `<host>/.ergo/watch-report.json` を書き、Unity はそれを読むだけ。CLI が無くても動く |

### 2.4 Unity パッケージの置き場所

| 案 | 採否 | 理由 |
|---|---|---|
| A. **Ergo `unity/com.ludiars.ergo/`** | **採用** | 報告形式の変更と同じ PR で直せる。Tela が同じ置き方 (`Tela/unity/com.ludiars.tela`)。導入は `https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo#<sha>` |
| B. UnityFoundation | 不採用 | Uf は Unity プロジェクトで、UPM パッケージ群の配布元ではない。Uf の利用者以外が入れにくい |
| C. 新規リポ | 不採用 | 形式の正本 (Ergo の `spec/data`) と離れ、横断変更が 2 PR になる |

---

## 3. 推奨案の詳細

### 3.1 パッケージマネージャの範囲 (依頼 5-1)

#### 宣言: ホスト側 `ergo-packages.json` の `host` 節

Ergo のマニフェストと **同じファイル名・同じ schema 番号** を使い、ホストのリポ直下に置く。ホストのファイルは `packages` に
非公開パッケージだけ (= 今の `ERGO_PACKAGE_MANIFEST_EXTRA` の中身) を持ち、`host` 節で Ergo の pin と選択を持つ。

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

- `use` は `ERGO_PACKAGES` の既定値、`optional` は「ソースが見つからなければ使わない」(Ludellus-Native の Pictor 任意)。
- `features` は `ERGO_PACKAGE_FEATURES_<NAME>` の既定値。どれもキャッシュ変数で上書きでき、上書きは resolved.json に残る (ハブ設計どおり)。
- `legacy_vars` は移行中だけ使う別名表 (§3.4)。S6 で警告、その後削除。
- `host` 節を持つファイルは Ergo のマニフェストとして読まない (Ergo のマニフェストは `host` を持たない)。ハブの `ERGO_PACKAGE_MANIFEST_EXTRA`
  は「ホストの `ergo-packages.json` の `packages` 節」を読む形に寄せ、既存の同名エラー (PKG-15) をそのまま使う。

Ergo 自身を取る仕組み (鶏と卵): ホストは Ergo を取る前に Ergo の CMake を使えない。`ergo_pkg init` が
`cmake/ergo-bootstrap.cmake` (30 行程度、`string(JSON)` で `host.ergo` を読み、`ERGO_SOURCE_DIR` → 兄弟 → 取得 (明示 ON のときだけ) の順で
Ergo を見つけて `add_subdirectory`) をホストに書き込む。ファイル先頭に生成元の Ergo SHA を入れ、`ergo_pkg doctor` が古さを検出する。
解決の順・取得の既定 OFF・照合の規則はハブ設計 §3 の 6 経路と同じ (Ergo 自身に PKG-02/04/05/07 を当てる)。

#### コマンド体系 (`ergo_pkg`、C++、`tools/package_manager/`)

| コマンド | 何をするか | 書き換えるもの | ネットワーク |
|---|---|---|---|
| `init [--code <c>]` | ホストに `ergo-packages.json` (host 節) と `cmake/ergo-bootstrap.cmake` を作り、`.gitignore` に `.ergo/` を足す | ホスト | 無し |
| `list [--json]` | 宣言・resolved.json (あれば)・UPM 宣言を合わせた一覧 | 無し | 無し |
| `doctor [--json]` | 宣言の検査 (ハブの PKG 規則を CLI から実行) + ブートストラップの古さ + 開発機パスの混入検査 (§3.7) | 無し | 無し |
| `add <name> [--optional] [--features ...]` | `host.use` に足す。Ergo マニフェストに無い名前は `--repository/--rev` を要求し host の `packages` (非公開) に足す | ホスト | 任意 (`--resolve` でタグ → SHA) |
| `remove <name>` | `host.use` / host `packages` から外す。`modules.*.needs` を壊す場合は拒否 (PKG-14 と同じ判定) | ホスト | 無し |
| `update <name> --to <tag\|main\|sha>` | 対象を SHA に解決し `rev` と `expect_tag` を書く。`name` が `ergo` か非公開ならホスト、共有パッケージなら **Ergo の checkout** (`--ergo <dir>`) のマニフェスト | 持ち主 | 要 (ミラー) |
| `outdated [--json]` | 各 pin に対する新タグ・main の進み・審査中の更新 (§3.2) を今すぐ判定して表示。通知はしない、前回状態も書かない | 無し | 要 (無ければ「未確認」) |
| `fetch [--cache <dir>]` | pin を `<cache>/src/<name>-<sha12>` に展開し、ホストの `CMakeUserPresets.json` (追跡しない) に `ERGO_PACKAGE_SOURCE_DIR_<NAME>` を書く | 追跡外のみ | 要 |
| `watch --host <dir>... --state <file> --out <dir>` | 定期実行の本体。判定 → 前回状態と差分 → 新規の出来事だけを `events.json` に、ホストごとの `<host>/.ergo/watch-report.json` を書く | 状態・報告のみ | 要 |
| `status --host <dir> --out <file.tela>` | P4 ホスト状態図を書く (§3.4) | 出力のみ | 無し |

- `update` は **pin を書くだけ**で、コミット・ブランチ作成・PR はしない (Ergo の運用ではホストの人が feat ブランチで PR を出す)。
  完了時に「次にすること」(どのリポで PR を出すか、STRICT で通るか) を表示する。
- `--to main` / `--to <sha>` で tag の無い pin を書くと、N4 (STRICT ではタグ要求) により STRICT 構成で通らないことを警告する。
  `--to` に feature ブランチを渡すと拒否する (§1 の squash 問題。main の祖先でない SHA は `--allow-off-main` が無ければ書かない)。
- 終了コード: 0 成功 / 1 判定の結果に要対応 (`outdated --fail-on tag` 等、CI 用) / 2 引数・入力の誤り / 3 書けない・取れない。
  `ergo_package_viz` と同じ割り当てにする。

#### 取得とミラー

- `update` / `outdated` / `watch` / `fetch` は **ツリーレスの bare ミラー** `<cache>/mirrors/<Repo>.git` (`git clone --bare --filter=tree:0`)
  を使う。タグと main の祖先判定・距離 (`rev-list --count`) は blob も tree も要らない。`<cache>` は `--cache` > 環境変数
  `ERGO_PKG_CACHE_DIR` > `ERGO_DEPENDENCY_CACHE_DIR` の順 (Windows のパス長対策はハブ設計 §8 と同じ手順)。
- 先に `git ls-remote --tags --heads origin` で参照の指紋を取り、前回と同じならミラーを fetch しない (毎回の通信は 1 往復)。
- **兄弟チェックアウト** (`--sibling-root` / `ERGO_PACKAGES_SIBLING_ROOT`): ミラーの代わりに `<root>/<Repo>` の `refs/remotes/origin/*` と
  `refs/tags/*` を読む (fetch はしない)。そのとき判定は「最後に fetch した時点の」値で、報告に `as_of: "sibling-last-fetch"` と
  その refs の更新時刻を載せる。`.worktrees` は見ない (ハブ設計どおり)。
- **オフライン**: 取れなかったパッケージは `observed: false` と理由を載せ、「更新なし」とは書かない。前回状態は書き換えない
  (オフラインの回で「解消した」と誤判定しない)。

#### 既存との整合

| 既存 | 整合 |
|---|---|
| S2 の 6 経路 | 変えない。`fetch` は経路 2 (ローカルソース) を用意するだけ、`init` のブートストラップは Ergo 自身に同じ順を当てるだけ |
| STRICT / N4 | `update` が STRICT で落ちる pin を書く前に警告。`doctor --strict` は構成時と同じ判定 |
| N7 (pin 更新はホストの人) | `update pictor` は Ergo のマニフェストを書き換えるだけで PR は人が出す。自動 PR は作らない (§6 未決 W4) |
| 取得の既定 OFF | 構成時 (CMake) は今までどおりネットワークに出ない。ネットワークに出るのは明示的に打った `ergo_pkg` コマンドと見張りのジョブだけ |

### 3.2 更新の検知 (依頼 5-2)

#### Pf の実際の挙動 (確かめた結果)

- Pf のサーバは Concordia へ `POST /v1/delegation/invoke` を送る (`call_name` = 設定の既定テンプレート、`args` に `project` /
  `anatomia_repo` / `anatomia_domain` / `target`、`triggered_by: "praeforma"`)。返った `run_id` を Pf の DB (`cc_links`、`cc_kind = delegation_run`) に残し、
  `/sync` で run の状態を引き直す (`Praeforma/server/src/routes/cc.ts`、`server/src/lib/cc-client.ts`)。
- Pf 自身は git に書かない。git を読むのは一覧の並び替え用に `.git/logs/HEAD` の末尾を読むだけ (`project-git-activity.ts`)。
- よってブランチは委託先セッションが作る。Concordia の run には `spawn_branch` / `spawn_worktree_path` / `child_session_id` /
  `triggered_by` が記録され (`GET /v1/delegation/runs`、`/v1/delegation/runs/:id`)、委託先は Revisor へ local PR を出す
  (local PR の記録は `repository` / `headRef` / `baseRef` / `headSha` / `baseSha` / `sessionId` / 状態 open|merged|closed。
  `Revisor/src/local-pr-service.mjs`、一覧は `GET /v1/local-prs?view=summary&state=open`)。
- 委託ブランチの名前は委託ごとに違う (このセッション自身も `design/package-manager-opus`)。**ブランチ名の接頭辞で Pf / AI を見分けることはできない**。

#### 何を「更新」とみなすか

1 つの依存 (= ホスト H がパッケージ P を pin S で使っている、または Ergo のマニフェストが P を S で持つ) ごとに次を判定する。

| 種類 `kind` | 条件 | 主体 `subject` (重複判定の鍵) |
|---|---|---|
| `tag` | `v*` タグ T が S の子孫、かつ `expect_tag` より新しい (semver 比較)。複数あれば最新 1 つ | タグ名 |
| `main` | `origin/main` の HEAD M ≠ S かつ S が M の祖先。距離 n を添える | M の SHA |
| `review` | P のリポに base = main の **open** local PR がある | local PR の id + `headSha` |
| `merged` | 前回 `review` だった local PR が merged になった (= 次の `main` の中身の説明) | local PR の id |
| `off-main` (警告) | S が `origin/main` の祖先でない (squash で消えた feature SHA を pin している等) | S |
| `pin-behind-ergo` (Ergo の pin 専用) | ホストの Ergo pin が、Ergo の main で上がった共有パッケージの pin を含まない | Ergo の main SHA |

由来 `origin` は `review` / `merged` に付ける (タグと main の進みは由来を問わない。中身の説明は `merged` の列で足りる)。

| 由来 | 判定 |
|---|---|
| `pf` | local PR の `sessionId` が Concordia run の `child_session_id` と一致 (無ければ `headRef == spawn_branch` かつ同リポ) し、その run の `triggered_by == "praeforma"` |
| `ai` | 同じ突合で run が見つかり、`triggered_by` が `praeforma` 以外 (Discord スレッド・daily-goal-run・web-spawn など。値はそのまま `trigger` に写す) |
| `human` | Concordia に到達でき、突合する run が無い |
| `unknown` | Concordia に到達できない / 認可されない |

#### 取得元と認証

| 取得元 | 取るもの | 認証 |
|---|---|---|
| git (ls-remote / ミラー fetch) | タグ・main の HEAD・祖先と距離 | 公開リポは不要。非公開リポは利用者の git credential helper に任せ、`ergo_pkg` はトークンを受け取らない・書かない |
| Revisor local PR API | open / merged の local PR | loopback。URL は引数 `--revisor-url` か環境変数 `ERGO_WATCH_REVISOR_URL` で受ける (ポートは Excubitor catalog が正本、Ergo に書かない) |
| Concordia 委託 run | `triggered_by` / `spawn_branch` / `child_session_id` | loopback。`--concordia-url` / `ERGO_WATCH_CONCORDIA_URL`。`GET /v1/delegation/runs?limit=…` を 1 回読む |
| GitHub の PR (任意、後段) | 公開 PR | 初版は扱わない (W5)。トークンが要り、LUDIARS の主経路は local PR |

Revisor / Concordia が無い環境 (CI・他の開発機) では `review` を出さず、報告に `sources.revisor: "unavailable"` と書く。

#### 頻度

- 見張りのジョブは **15 分ごと** (Task Scheduler)。各回は ls-remote の指紋比較が主で、変化があったリポだけミラーを fetch する。
- Revisor / Concordia の読みは各回 1 リクエストずつ (一覧 API)。
- 通知の配送は「新しい出来事があった回だけ」。加えて **日次の要約** (朝 1 回、未対応の出来事の一覧) を出す (§3.3)。

#### 前回状態と重複の抑止

- 状態ファイル: `--state <file>` で受ける。Excubitor のジョブは自分のデータディレクトリ配下 (例 `data/ergo-watch/state.json`) を渡す。
  **ホストのリポにもErgo のリポにも置かない** (公開リポ・開発機のパス)。ホストの `.ergo/watch-report.json` は表示用の写しで、状態の正本ではない。
- 状態の中身: ホストごとに `(dep, kind, subject)` の既知集合と、最初に見た時刻・最後に通知した時刻。参照の指紋 (リポごと)。
- 重複の規則:
  1. 同じ `(host, dep, kind, subject)` は 1 回だけ通知する。
  2. `main` は主体が SHA なので進むたびに新しくなる → **同じ `(host, dep)` の `main` は 6 時間に 1 回まで**に畳み、畳んだ間の距離を合算する。
  3. pin が上がって条件を満たさなくなった出来事は「解消」にして状態から消す (再発は新規)。
  4. `review` → `merged` は同じ local PR の続きとして 1 本の通知の更新にする (Discord では新規投稿だが本文に前回の参照を書く)。
- 判定は純粋関数 `diff(prev_state, observations) → (events, next_state)` にし、時刻は引数で渡す (テストで決定的にする)。

### 3.3 通知 (依頼 5-3)

#### 担い手

- `ergo_pkg watch` は **判定と出来事の JSON を書くまで**。配送はしない。
- 配送は Excubitor の update ドメインが既存の Discord 配送 (`package-update-audit` の `discord-report.ts` と同じ webhook 方式) で行う。
  Excubitor 側に足すのは「catalog の `ergo_watch: true` の checkout を `--host` に並べて `ergo_pkg watch` を起動し、`events.json` を整形して送る」ジョブ 1 本。
  これは Excubitor の PR (Ergo 外) で、Ergo の PR には含めない。
- Concordia へも出したい場合は Excubitor のジョブが Concordia の通知経路へ転送する (Ergo は Concordia に書き込まない)。

理由: Ergo は C++ ライブラリ群で、プロセス管理・定期起動・Webhook の秘密値は Excubitor がすでに扱っている。Ergo に持たせると
常駐 1 本と秘密値の置き場が増える。Ergo の責務は「同じ入力から同じ出来事を出す」判定に閉じる。

#### 通知先と誰に届くか

| 通知先 | 形式 | 誰に |
|---|---|---|
| Discord (Excubitor の update 用 webhook) | 1 出来事 1 埋め込み、日次要約 1 本 | 開発者全体 (チャンネル)。非公開ホストは非公開チャンネルの webhook (ホストの catalog 設定で選ぶ) |
| CLI | `ergo_pkg outdated` の表 / `--json` | 手元で打った人 |
| エディタ (Unity) | 窓の一覧 + 未読数のバッジ、新着時に `EditorWindow.ShowNotification` | その Unity プロジェクトを開いている人 |
| Tela | P4 ホスト状態図 | 見たい人が開く |

#### 通知の内容 (events.json の 1 件と Discord の本文)

```json
{
  "host": "Ln", "dep": "pictor", "owner": "ergo", "kind": "tag",
  "from": { "rev7": "625d83d", "tag": "v2.2.0" }, "to": { "rev7": "9a1b2c3", "tag": "v2.3.0" },
  "distance": 41, "origin": null, "review": null,
  "strict_ok": true,
  "action": { "repo": "Eg", "command": "ergo_pkg update pictor --to v2.3.0", "then": "Ergo に feat ブランチで PR" },
  "first_seen": "2026-10-14T03:00:00Z"
}
```

Discord の本文 (日本語、1 行目で要件が分かる):

> **Ln の依存 Pictor に新しいタグ v2.3.0** (今の pin は v2.2.0 / 625d83d、41 コミット先)
> 持ち主: Ergo のマニフェスト → Ergo で `ergo_pkg update pictor --to v2.3.0` を打ち、feat ブランチで PR。STRICT で通る pin です。

`review` の例:

> **Ergo の依存 Tela に審査中の更新** (Revisor local PR #128、由来: Pf の委託、ブランチ head 4f0e9a1)
> マージ後に `main` の更新として再通知します。今の操作は不要です。

- 書くもの: ホストのコード、依存名、持ち主、種類、前後の版 (タグと SHA7)、距離、由来、local PR の番号、次の操作、STRICT で通るか。
- **書かないもの**: 絶対パス、ホストのディレクトリ名、PR のタイトルと本文、コミットメッセージ、トークン、Revisor / Concordia の URL (§3.7)。
  ホストは `host.code` (略称) でだけ表す。

### 3.4 Ludellus-Native の状態確認 (依頼 5-4)

#### 何を表示するか

| 項目 | 取り方 |
|---|---|
| 依存ごとの pin・版札・経路・drift・dirty・選んだ機能 | `<build>/ergo-packages.resolved.json` (ハブが構成時に書く) |
| Ergo 自身の pin と遅れ | ホストの `host.ergo` + 見張りの報告 |
| 依存ごとの「来ている更新」(tag / main / review、由来つき) | `<host>/.ergo/watch-report.json` (見張りが書く写し) か `ergo_pkg outdated --json` |
| ハブの検査結果 (PKG-xx) | resolved.json の `findings` |
| ビルドとテストの結果 | `ctest --output-junit <build>/ctest-junit.xml` の JUnit XML (件数・失敗名)。無ければ「未実行」。ビルドの成否は `<build>` の File API 返答の有無と時刻で「構成済み」まで (ビルドの成否は取らない、§6 W6) |
| Revisor の審査結果 (ホスト自身の open local PR) | Revisor の local PR 一覧 (ホストのリポ分) |

#### Tela の画面構成 (P4 ホスト状態 `status`)

既存の P1 (版一覧) と P2 (リンク図) は「構成の形」を見る図、P4 は「1 ホストの今の状態」を見る図。P1 の部品 (グループ・版札・印) を使い回し、
`package_viz` に `status_view` を 1 ファイル足す。

| グループ (≤ 8) | ノード | 札 |
|---|---|---|
| host | ホスト 1 ノード | コード / ホストの HEAD SHA7 / モード (M0〜M2) / テスト `12/12` か `3 失敗` か `未実行` |
| Ergo | Ergo 1 ノード | 版札 / pin SHA7 / 遅れ (`main +18`、`新タグ v0.10.0`) |
| 依存 (link) | Pictor など | 版札 / pin / drift / 機能 / 印 (`★drift` `dirty` `off-main`) |
| 更新: タグ | 新タグ 1 ノード / 依存 | `v2.3.0 (41 先)` |
| 更新: main | main の進み 1 ノード / 依存 | `+41 (うち merged local PR 3)` |
| 更新: 審査中 | open local PR 1 ノード | `#128 Pf` / `#131 AI` / `#133 人` |
| 検査 | 落ちた PKG-xx | ID と対象 |

辺: host → 依存 (実線、P1 と同じ向き)、依存 → 更新ノード (破線、「来ている」)、依存 → 検査 (点線)。上限超過時はハブ設計 §5 の畳み
(審査中が多いときは由来ごとに 1 ノードに畳んで件数を札に) を使う。

```sh
ergo_pkg status --host <Ludellus-Native> --build native/build --out status.tela
tela_view --font <file.ttf> --graph status.tela
```

#### Ludellus-Native の ergo-packages.json 化 (S5 の先頭)

現状: `SUIKA_ERGO_DIR` (既定 `native/external/ergo`) を必須で `add_subdirectory`、`ERGO_BUILD_*` を FORCE でモジュール選択、
`SUIKA_PICTOR_DIR` があれば Pictor を `add_subdirectory` して `PICTOR_BUILD_DEMO/TESTS`・`PICTOR_ENABLE_RIVE` を FORCE。Ergo の SHA は固定していない。CI は無い。

| 段 | 変更 (Ludellus-Native の PR) | 受入 |
|---|---|---|
| L1 Ergo の pin | `ergo_pkg init --code Ln` で `ergo-packages.json` (host 節に Ergo の SHA、`use` は空) と `cmake/ergo-bootstrap.cmake` を足す。`native/CMakeLists.txt` の Ergo 取り込みをブートストラップ呼び出しに置き換え、`SUIKA_ERGO_DIR` は `legacy_vars` で `ERGO_SOURCE_DIR` の別名として受ける (渡されれば使い、`DEPRECATION` を出す)。M0 のまま | 同じ `SUIKA_ERGO_DIR` を渡した構成で、ターゲット集合と `ctest -N` の一覧が変わらない。resolved.json に Ergo の drift が出る |
| L2 照合 (M1) | `host.use` / `optional` に `pictor`。Pictor の取り込みはホストのまま (`SUIKA_PICTOR_DIR`) | ハブがホスト供給経路で受け PKG-05 を判定。PKG-15 (`PICTOR_BUILD_*` の FORCE) が記録される |
| L3 委譲 (M2) | Pictor の `add_subdirectory` と FORCE を消し、`SUIKA_PICTOR_DIR` は `legacy_vars` で `ERGO_PACKAGE_SOURCE_DIR_PICTOR` の別名に。`PICTOR_ENABLE_RIVE` は host `features` / cache 既定で渡す | 描画ターゲットの有無が L2 と同じ。PKG-15 が消える |

- `ERGO_BUILD_*` によるモジュール選択はホストに残す (ハブの範囲外)。ただし FORCE は「未設定なら既定」に直すよう L1 で推奨する。
- `spec/setup/native-build.md` と `native/CMakeLists.txt` のエラー文にある開発機の絶対パス例は L1 で `<ergo>` などの記号に置き換える (§3.7)。
- 見張りへの登録は Excubitor catalog の Ludellus-Native に `ergo_watch: true` を足す (Excubitor 側)。

### 3.5 Unity パッケージ (依頼 5-5)

#### 形式と置き場所

```
unity/com.ludiars.ergo/
  package.json                      name: com.ludiars.ergo、unity: "2022.3" (Uf は 6000.0 で使えることを検証対象に含める)
  README.md  CHANGELOG.md
  Editor/Core/Ergo.Packages.Core.asmdef      noEngineReferences: true (UnityEngine に依存しない純 C#)
    UpmManifestReader.cs            Packages/manifest.json の git 依存 → (名前, リポ, path, rev)
    PackagesLockReader.cs           packages-lock.json の hash (解決済み SHA)
    LudiarsPackageCatalog.cs        github.com/LUDIARS/<Repo> を略称に写す (表は報告ファイルから受ける。C# に名簿を持たない)
    WatchReportReader.cs            <project>/.ergo/watch-report.json → 出来事一覧
    PinPlanner.cs                   「v2.3.0 にする」→ 新しい git URL (#<sha>) を作る。ブランチ名・短縮 SHA を拒否
    ManifestJsonEditor.cs           manifest.json の該当 1 値だけを書き換える (他の行・並び・改行を保つ)
  Editor/Ergo.Packages.Editor.asmdef         Core に依存、UnityEditor を使う
    ErgoPackagesWindow.cs           Window > LUDIARS > Ergo Packages
    UpdateNotifier.cs               報告ファイルの更新時刻を見て未読数を更新、新着で ShowNotification
    UpmApplier.cs                   ManifestJsonEditor で書いて UnityEditor.PackageManager.Client.Resolve()
    NativeCliRunner.cs              (任意) EditorPrefs に ergo_pkg のパスがあれば「今すぐ確認」で outdated --json を起動
  Tests/Editor/Ergo.Packages.Tests.asmdef    EditMode テスト (Core だけを対象)
  Tests/Editor/Fixtures/            Ergo 本体の tests/package_watch/fixtures/ の写し (§3.5 検証)
```

導入: `Packages/manifest.json` に `"com.ludiars.ergo": "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo#<40 桁 SHA>"`。
パッケージ自身も SHA で固定する (自分の更新も一覧に出る)。

#### Unity 側で持つ機能

| 機能 | 中身 |
|---|---|
| パッケージ一覧 | UPM 宣言のうち LUDIARS のリポを指すもの: 名前 / 略称 / 宣言の rev / lock の hash / ブランチ名指定の警告 (`#main` は SHA でないので STRICT 相当の警告) |
| 来ている更新 | 報告ファイルの出来事 (tag / main / review、由来つき)。報告の時刻と `observed` を出し、古い・未観測を「更新なし」と表示しない |
| 更新の適用 | 出来事の「この版にする」→ PinPlanner → 差分を確認ダイアログで見せる → manifest.json を書く → `Client.Resolve()`。コミットはしない |
| 通知 | 窓のタブに未読数、新着時の通知 1 回。ポップアップを繰り返さない (状態は報告の出来事 id で既読管理、EditorPrefs) |

Unity の非 git 依存 (レジストリ・`file:`) は一覧に出すが更新判定の対象外 (理由を札に書く)。

#### ネイティブ側との共有のしかた

- 共有するのは **形式** (`spec/data/ergo-watch-report.md`、`schema: 1`) と **pin 規則** (40 桁 SHA、タグは注釈) だけ。
- `ergo_pkg watch` は Unity プロジェクトを「UPM ホスト」として扱う入力アダプタ (`upm_host_reader`) を持つ。`ergo-packages.json` が無く
  `Packages/manifest.json` があるディレクトリを UPM ホストとみなし、`host.code` は `.ergo/host.json` (`{"code": "..."}`、`init --upm` が作る、追跡する) から読む。
- 判定ロジックは C++ にしか無い。C# は報告を読む・pin を書くだけ。

#### Unity 版の検証方法

| 検証 | 機械で確かめる条件 |
|---|---|
| 形式の一致 | Ergo の `tests/package_watch/fixtures/*.json` と `unity/com.ludiars.ergo/Tests/Editor/Fixtures/*.json` がバイト一致 (`cmake -P tests/package_watch/check_unity_fixtures.cmake`。Unity 無しで回る) |
| Core の単体 | Core asmdef は `noEngineReferences` なので Unity 無しでも `dotnet build` で型検査できる (`unity/com.ludiars.ergo/Tests/Core.csproj` を用意し CI で `dotnet test`)。Unity 上では同じテストを EditMode で回す |
| コンパイル | Pf と同じ方式のコンパイル確認スクリプト (`scripts/compile-unity.ps1 -EditorData <Unity Editor/Data>`) を Ergo にも置く。Editor 起動は人の許可に従う |
| 手元確認 | Uf の Unity プロジェクトに入れて窓が開くこと・報告の表示・pin 書き換え後の Resolve (未確認として PR に書く) |

### 3.6 既存段階 (S4〜S7) との関係 (依頼 5-6)

S4 (観測 P3) は並行で実装中なので、**S4 が触るファイルに触らない**段階を先に置き、P4 (status_view) だけ S4 の後にする。

S4 が触る (と想定する) もの: `package_model` の観測スキャン (`scan` 系の新ファイル)、`package_viz` の `usage` 図、`tools/package_viz` の `scan` サブコマンド。
この案の新段階が触るもの: 新モジュール `ergo_package_manager` / `ergo_package_watch`、新ツール `tools/package_manager/`、`unity/`、
`spec/data/ergo-watch-report.md`、`cmake/packages/ErgoPackageManifest.cmake` (host 節と EXTRA の寄せ。S4 は触らない想定)。

| 段階 | 内容 | 先行 | S4 との衝突 |
|---|---|---|---|
| S4 | 観測 P3 (既定どおり) | S3 | — |
| **S4a** 宣言と読み取り | host 節のスキーマ、`ergo_package_manager` (読む・検査)、`ergo_pkg init/list/doctor`、ブートストラップ | S3 | 無し (新モジュール・新ツール) |
| **S4b** 書き換えと取得 | `add/remove/update/fetch`、ミラー、`ManifestEdit` (書式を保つ書き換え) | S4a | 無し |
| **S4c** 検知 | `ergo_package_watch` (観測 → 判定 → 差分)、git ソース、`outdated` / `watch`、報告と出来事の形式 | S4b | 無し |
| **S4d** 由来 | Revisor / Concordia の読み取りアダプタ、`review` / `merged` / 由来の判定 | S4c | 無し |
| **S4e** 状態図 P4 | `package_viz/status_view`、`ergo_pkg status`、JUnit 読み | S4 と S4c | `package_viz` の登録 1 行 (S4 の後に入れる) |
| **S4f** Unity | `unity/com.ludiars.ergo` (一覧・報告表示・pin 適用)、UPM ホスト入力 | S4c | 無し |
| (Ergo 外) 配送 | Excubitor のジョブと Discord 整形 | S4c | — |
| S5 ホスト移行 | **Ludellus-Native を先頭** (L1→L2→L3)、他ホストはハブ設計どおり | S4a (L1)、S2 (L2/L3) | — |
| S6 互換の既定変更 | M0 警告に加えて `legacy_vars` の警告 | S5 | — |
| S7 任意 | ハブ設計の S7 に「GitHub PR の取得 (W5)」「自動 PR (W4)」を足す | — | — |

モジュールの向き: `package_viz → package_model`、`ergo_pkg (tools) → package_manager / package_watch / package_viz`。
`package_watch` は `package_manager` の宣言の型を読む (下層に寄せる: 宣言の型は `package_manager`、判定は `package_watch`、どちらも `package_viz` を import しない)。
IO (git プロセス・HTTP) は `tools/package_manager/` 側のアダプタに置き、ライブラリは `IRefSource` / `IReviewSource` / `IRunSource` の
interface だけを持つ (DIP。テストは偽ソース)。HTTP は既存 `ergo_http` の `IHttpClient` を tools 側で使う。
ドメインは `package_manager` / `package_watch` を `foundation` (宣言と取得) と `observability` (判定) に分け、同じ PR で `spec/domains/*.domain.json` と
`.anatomia/layers.json` を更新する。`unity/` は新しいトップレベルなので同じ PR で membership を足す (Ergo CLAUDE.md)。

### 3.7 公開リポとしての注意 (依頼 5-7)

| 置き場所 | 規則 |
|---|---|
| Ergo / ホストの追跡ファイル (マニフェスト、ブートストラップ、spec、テストの見本) | 絶対パスを書かない。パスはリポ相対か記号 (`<ergo>` `<cache>` `<build>`)。リポは公開 URL か略称だけ |
| 非公開パッケージ・非公開ホストの名前 | 非公開ホストの `ergo-packages.json` の `packages` にだけ書く。Ergo のマニフェストと Ergo のテスト見本には出さない |
| 前回状態・ミラー・キャッシュ | リポの外 (Excubitor のデータディレクトリ、`<cache>`)。ホストの `.ergo/` は `init` が `.gitignore` に足す |
| 報告 (`watch-report.json`) と出来事 (`events.json`) | パスを持たない (ホストは略称、依存は名前)。local PR は番号と headSha7 だけ (タイトル・本文・ブランチ名は持たない。ブランチ名は委託のタスク名を含み得る) |
| 通知の本文 | 上と同じ。加えて URL・ポート・トークン・セッション id を書かない |
| 認証 | `ergo_pkg` はトークンを引数・環境変数・ファイルのどれでも受け取らない。git は利用者の credential helper、Revisor / Concordia は loopback 前提 |
| ポート | Ergo に書かない。URL は見張りのジョブ (Excubitor catalog が正本) が引数で渡す |

機械検査: `ergo_pkg doctor` と Ergo 自身のテストに **漏れ検査** を入れる。対象 = 追跡ファイルのうち `ergo-packages.json` / `cmake/ergo-bootstrap.cmake` /
`spec/data` / `tests/package_watch/fixtures` / `unity/**`、および `watch` が書く報告と出来事。拒む形 = ドライブ文字始まり (`[A-Za-z]:[\\/]`)、
`/Users/` `/home/`、`\\` 始まりの UNC、`token` / `secret` / `Authorization` をキーに持つ JSON 値。報告・出来事は書く前に同じ検査を通し、
当たれば書かずに終了コード 3。

---

## 4. 実装の分割と受入条件 (依頼 5-8)

どの段階も単独で main に入り、既存ホストの構成は変わらない (CMake 構成時の挙動は S4a のブートストラップを入れたホスト以外不変)。
テストのネットワークは使わない: git はテスト内で初期化した偽リポ、Revisor / Concordia は JSON 見本を返す偽ソース。

| 段階 | 受入条件 (機械で確かめる) |
|---|---|
| **S4a** | (1) host 節の読み取り: 見本 3 種 (host 節のみ / host + 非公開 packages / 不正) を読み、不正は理由つきで拒否。(2) `host` 節を持つファイルを Ergo マニフェストとして読むと構成エラー。(3) ホストの非公開 `packages` と Ergo のマニフェストの同名は PKG-15 エラー (既存規則の再利用を `cmake -P` で確認)。(4) `ergo_pkg init` を空ディレクトリで 2 回走らせ、2 回目が無変更 (冪等)・`.gitignore` に `.ergo/` が 1 行だけ。(5) 偽 Ergo リポ + 偽ホストで、ブートストラップが `ERGO_SOURCE_DIR` → 兄弟の順で解決し、取得 OFF では通信しない (取得経路を選ぶと構成エラー)。(6) `doctor` がブートストラップの生成元 SHA の古さと漏れ検査 (§3.7) を検出する (真偽両方) |
| **S4b** | (1) `update <name> --to <tag>` が偽リポのタグを 40 桁 SHA に解決し `rev` と `expect_tag` だけを書き換え、他の行・キー順・改行がバイト単位で変わらない。(2) 共有パッケージは `--ergo <dir>` のマニフェストを、`ergo` と非公開はホストを書く (取り違えたら終了 2)。(3) `--to` に feature ブランチ / main の祖先でない SHA を渡すと拒否、`--allow-off-main` で通る。(4) `--to main` で STRICT 警告が出る。(5) `remove` が `modules.*.needs` を壊す場合に拒否。(6) `fetch` が `<cache>/src/<name>-<sha12>` を作り、`CMakeUserPresets.json` に `ERGO_PACKAGE_SOURCE_DIR_<NAME>` を書き、その構成でハブが経路 2 を選ぶ (resolved.json の `route`) |
| **S4c** | (1) 偽リポで tag / main / off-main を判定 (距離・semver 比較・複数タグの最新 1 つ)。(2) `diff(prev, obs, now)` の決定性: 同じ入力で同じバイト列。(3) 重複抑止: 同じ主体は 2 回目で出来事 0、`main` は 6 時間内の畳み込み (距離の合算)、pin を上げると解消して状態から消える。(4) オフライン (ソースが失敗) で `observed:false`、前回状態が書き換わらない。(5) 兄弟チェックアウトでは fetch せず `as_of` が付く。(6) ls-remote の指紋が同じならミラーを fetch しない (偽ソースの呼び出し回数で確認)。(7) 報告と出来事が漏れ検査を通る、通らない入力 (絶対パスを含む見本) で書かずに終了 3。(8) 形式見本 `tests/package_watch/fixtures/` を同梱 |
| **S4d** | (1) local PR 見本 + run 見本の突合で `pf` / `ai` (trigger 写し) / `human` / `unknown` を判定 (sessionId 一致、無ければ headRef == spawn_branch)。(2) open → merged の遷移で `merged` が 1 回、`review` の続きとして参照が付く。(3) Revisor / Concordia が無い (偽ソースが失敗) とき `review` を出さず `sources.* = unavailable`。(4) 出来事に PR タイトル・本文・ブランチ名が含まれない (見本にわざと入れて出力に出ないことを確認) |
| **S4e** | (1) resolved.json + 報告 + JUnit 見本から P4 を作り `validate_tela_graph` を通る。(2) 審査中 20 件の見本で由来ごとの畳み込みが起き上限内に収まる。(3) JUnit が無いとき札が `未実行`。(4) 同じ入力で同じ `.tela` (決定性)。(5) S4 の `usage` 図のテストが無変更で通る |
| **S4f** | (1) `check_unity_fixtures.cmake` でフィクスチャの写しがバイト一致。(2) Core の EditMode / `dotnet test`: manifest.json の git 依存の読み取り、`#main` の警告、PinPlanner がブランチ名と短縮 SHA を拒否、ManifestJsonEditor が 1 値だけ変えて他をバイト一致で保つ、報告の `observed:false` を「更新なし」にしない。(3) `ergo_pkg watch` が UPM ホスト見本 (manifest.json + packages-lock.json + `.ergo/host.json`) から出来事を出す。(4) Unity 実機 (窓・Resolve) は手元確認として未確認と記す |
| **配送** (Excubitor) | Excubitor 側の受入 (Excubitor の PR で定める)。Ergo 側は `events.json` の形式見本を正本として渡す |
| **S5 L1〜L3** | §3.4 の表のとおり (Ludellus-Native の PR)。Ergo 側の受入は無い |

完了の確認に使う契約 (実装時に `augur.contracts.json` へ写す想定、id は段階ごと):
`C-1 resolveHostManifest(host): host 節と非公開 packages を読み、Ergo マニフェストとの同名を拒否する` /
`C-2 updatePin(name,to): 持ち主のマニフェストの rev と expect_tag だけを書き換える` /
`C-3 diffWatch(prev,obs,now): 同じ主体を 2 度出さず、オフラインで前回状態を変えない` /
`C-4 classifyOrigin(pr,runs): triggered_by=praeforma を pf、他の run を ai、run 無しを human、到達不能を unknown にする` /
`C-5 writeReport(report): 絶対パス・秘匿キーを含む報告を書かない` /
`C-6 unityFixtures(): Unity パッケージの見本が Ergo の見本とバイト一致する`。

---

## 5. 未決事項 (推奨つき)

| ID | 論点 | 推奨 |
|---|---|---|
| W1 | Unity パッケージ名の接頭辞 (`com.ludiars.*` は Tela、`jp.ludiars.*` は Pf で揃っていない) | `com.ludiars.ergo` (Tela と同じ下層ライブラリ側に揃える)。LUDIARS 全体の統一は別件で決める |
| W2 | 見張りの頻度と `main` の畳み時間 | 15 分ごと・`main` は 6 時間に 1 回・日次要約は朝 1 回。運用して多ければ `main` を日次要約だけにする |
| W3 | 通知チャンネルの分け方 | 公開ホストは 1 チャンネル、非公開ホストは別 webhook (catalog の設定)。ホストごとのメンションは初版では付けない |
| W4 | `update` の後の PR を自動で作るか (N7 の自動化) | 初版は作らない。`update` は pin を書いて次の操作を示すだけ。自動化するなら Concordia の委託として起こす (Ergo から PR を作らない) |
| W5 | GitHub の公開 PR も「審査中」に入れるか | 入れない (初版)。LUDIARS の主経路は Revisor の local PR。入れるならトークンの置き場を Excubitor 側に決めてから |
| W6 | ビルドの成否を P4 に出すか | 初版は「構成済み」と ctest の JUnit まで。ビルドの成否は Revisor の審査結果 (ホストの local PR) を札に貼る形で後から |
| W7 | `ergo_pkg` バイナリの配布 (Unity の「今すぐ確認」と CMake を持たない利用者向け) | 初版は配布しない (Excubitor のジョブは Ergo の checkout でビルドしたものを使う)。要望が出たら Revisor のリリース経路で添付 |
| W8 | ホストの宣言を Ergo と同じファイル名にするか | 同じ `ergo-packages.json` + `host` 節 (ハブの `MANIFEST_EXTRA` を寄せられる、ファイルが 1 つで済む)。混同は「`host` 節の有無で役割が決まり、取り違えは構成エラー」で防ぐ |
| W9 | Pf 由来の判定を `triggered_by` に頼ってよいか | 頼る。ただし Pf 側で値を固定文字列の契約にしてもらう (Pf の `cc-client.ts` の `'praeforma'` を定数化し spec に書く、Pf 側の PR) |
