# Ergo Studio (`studio` プラグイン) {#SPEC-ergo-studio}

依頼 (2026-10-28): Ergo に GUI を用意し、各 LUDIARS の実装と連携しつつ、デスクトップ上でゲームを AI と
構築するインタフェースにする。Actio: `actio:684d8cda-6996-43ee-b3be-3c7482a21b97`。

方針 (人間の回答): **リソースは Curare、ソースは git / Revisor** で版管理・同期する。Curare は
「Git を前提にしない」アセットの版・提出・保持の正本で、ソースコードは扱わない。

## 置き場所

`tools/studio/plugins/studio/` の組み込みプラグイン (C++ ネイティブアプリ `ergo_studio.exe` の
ウィンドウでも、`--headless` のサーバだけでも同じ)。新しいツールはプラグインとして足すという
Ergo の方針 (CLAUDE.md) に従い、タブ (画面) は 1 プラグインの中で分ける。取得したプラグインの
台帳 (`plugin_ledger.cpp`) もこのプラグインが持ち、有効な pack は `tools/studio/main.cpp` が
`ERGO_PLUGIN_DIR` と同じ扱いで外部ローダに渡す。旧 `tools/ergo` (Node / Electron /
TypeScript) 版は 2026-11-06 に削除し、同じ API をネイティブで作り直した。

| 層 | ファイル | 責務 |
|---|---|---|
| 設定 | `studio_config.*` | 環境変数を一度だけ読む (`tools/studio/studio_settings.cpp` と並び、環境変数を読む 2 か所のうちの 1 つ) |
| 接続 | `endpoints.*` / `service_call.*` | 接続先の解決、サービス呼び出しと失敗分類 (HTTP は `src/studio/platform/` の WinHTTP クライアント) |
| クライアント | `{concordia,actio,breviarium,curare,revisor,git}_client.*` | 各サービスの API / CLI |
| 判定 (純関数) | `delegation_request.*` / `mutation_guard.*` / `resource_sync.*` / `source_sync.*` / `plugin_source.*` / `connection_map.*` | 指示の組み立て、変更系の防御、取得・早送り・取得元の判定、連携図 |
| 手順 | `resource_download.*` / `plugin_installer.*` / `plugin_ledger.*` | 複数サービスにまたがる流れと I/O、取得した pack の台帳 |
| API | `studio_plugin.*` / `studio_routes_{projects,tasks,sources}.cpp` | `/studio/api/*` |
| 画面 | `IPluginView` (アプリのウィンドウのタブ) | タブ: プロジェクト / タスク / ソース / リソース / プラグイン / 連携 |

## 不変条件

### ST-INV-01 接続先 {#SPEC-ergo-studio-endpoints}

ポート番号はコードに持たない。Excubitor は `EXCUBITOR_URL` か
`<LUDIARS_ROOT>/Excubitor/excubitor.catalog.yaml` の `code: excubitor`、他のサービスは Excubitor の
`GET /api/v1/topology` の `<CODE>_URL`。Curare だけは遠隔の Cu Server を `ERGO_CURARE_URL` で明示できる。
解決できなければ「未接続」として扱い、別のポートやホストを試さない。


### ST-INV-02 サーバ側から呼ぶ {#SPEC-ergo-studio-server-side}

画面は `/studio/api/*` とだけ話す。各サービスへは Ergo のサーバから
呼ぶ (Actio の local mode が Origin 付きの呼び出しを拒むため、トークンを画面に渡さないため)。


### ST-INV-03 変更系の防御 {#SPEC-ergo-studio-guard}

`/studio/api/*` の GET 以外は、Host が loopback、Origin を付けるなら
同じ host:port、本文が JSON のときだけ受け付ける (`mutation_guard.cpp`。サーバ全体の
`Host` / `Origin` 検査は `src/studio/net/request_guard.cpp`)。別サイトから localhost へ
POST して AI への指示やプラグイン導入を起こさせない。


### ST-INV-04 指示の作業場所 {#SPEC-ergo-studio-instruct}

AI への指示 (Cc `POST /v1/delegation/invoke`) の `cwd` と `target_repo` は
Cc に登録されたプロジェクトの `repo_path` に固定する。画面から任意のパスを渡させない。送る直前に Cc の
登録とテンプレートを引き直して検証し、テンプレートの `input_schema` に無い入力は送らない。


### ST-INV-05 タスクとの結びつけ {#SPEC-ergo-studio-tasks}

Actio のタスクから実装させるときは `taskflow_reference = actio:<id>` を
付け、Cc に新しいタスクを作らせない。タスク本文はテンプレートの `task` 入力に入れる (無ければ追加指示の先頭)。


### ST-INV-06 ソースの同期 {#SPEC-ergo-studio-sources}

git は `fetch` と「main の早送り」だけ。早送りは main にいて、変更が無く、
手元だけのコミットが無く、遅れているときだけ (共有の作業ツリーを他の人の作業ごと動かさない)。判定は
実行直前の状態で行う。push・PR 提出・マージはしない (Revisor の local PR と人間の判断が持つ)。
この制限は studio プラグインの「ソース」タブ (Cc に登録された全プロジェクトの自動同期) に掛かる。
人が選んで操作する git は ST-INV-10 の `git` プラグインが持つ。


### ST-INV-10 ワークスペースの git {#SPEC-ergo-studio-workspace-git}

`git` プラグインは、ワークスペースのプロフィール (`game-ops.md`) に書かれたリポジトリだけを扱う。
画面はリポジトリを一覧の番号で指し、パスを渡さない。操作は人がボタンで選んだときだけ行う
(状態・履歴・ブランチの作成と切り替え・全変更のコミット・pull (`--ff-only`)・push)。
Revisor のワークフロー (local PR・審査・マージ) とは別物で、その経路では使わない。
push は通常の `git push` で、そのリポジトリに入っている Revisor の push guard などのフックはそのまま効く。


### ST-INV-07 リソースの取得 {#SPEC-ergo-studio-resources}

Curare の取得計画 (`POST /api/v1/acquisition/plan`、`overwrite: false`) に従い、
手元に同名で別内容があれば上書きせず「衝突」にする。取得先は Cc プロジェクトの repo 内の相対パスだけ。
一時ファイルに書き、内容ハッシュ (`sha256:<hex>`) が計画と一致したときだけ置き換える。


### ST-INV-08 プラグインの取得 {#SPEC-ergo-studio-plugins}

取得元は許可リスト (`ERGO_PLUGIN_OWNERS`、既定 `LUDIARS`) の GitHub owner
だけ。rev を 40 桁の sha に解決してから subpath だけを取得し、中身 (コミット・プラグイン名・依存の有無・
置き換え) を見せて承認を取る。導入するのはプレビューで取得した物そのもの (再取得しない)。pack は
C ABI の共有ライブラリなので、取得時に CMake で構成・ビルドする (`plugin_installer.cpp`。
旧版の `npm install` は無い)。台帳に sha を固定して記録し、読み込みは次回起動時。


### ST-INV-09 秘密 {#SPEC-ergo-studio-secrets}

Actio / Curare のトークンは環境変数で渡し、保存・画面表示・ログ出力をしない。
サービスの失敗本文は要約 (300 文字) だけを画面に出す。


## 画面

連携タブの図と表は `connection_map.cpp` が組み立てる (下の「連携の可視化」)。

| タブ | 内容 | 使う API |
|---|---|---|
| プロジェクト | Cc への登録、Br の状態要約、AI への指示 (テンプレート選択 + `input_schema` の入力欄 + 追加指示)、Studio から出した委託の状態 | Cc `/v1/project-codes(/admin)`・`/v1/delegation/{templates,invoke,runs}`、Br `/api/projects/:code/{overview,refresh}` |
| タスク | 選択プロジェクトの Actio タスクの一覧・作成・状態変更、タスクを AI に実装させる | Actio `/api/tasks`、Cc `/v1/delegation/invoke` |
| ソース | 全登録プロジェクトの git 状態、選択プロジェクトの Revisor PR、fetch / main の早送り | `git`、Revisor CLI `pr list --json` |
| リソース | Curare のプロジェクト → フォルダ → アセット → 版を選び、Cc プロジェクトの repo 内へ取得 | Curare `/api/v1/{projects,assets,acquisition/plan}` |
| プラグイン | 取得元の指定 → 中身の確認 → 承認・導入、有効 / 無効 / 削除 | GitHub (`git ls-remote` / sparse fetch) |
| 連携 | Studio と各サービスの生死・接続先・認証・最終成功時刻の図と表、topology の全サービス | 各サービスの読み取り API |

## 連携の可視化 {#SPEC-ergo-studio-connections}

Studio を中心に Excubitor / Cc / Actio / Br / Curare / Revisor / Cernere を置き、各サービスの状態 (接続・応答異常・停止・未接続・認証が必要)、接続先、認証の方式、この Ergo プロセスが覚えている最終成功時刻を出す。辺は Studio → 各サービスの用途と、サービス同士の既知の連携 (Cc ⇄ Actio のプロジェクト / タスク同期、Cc → Revisor の local PR、Br → Actio のスプリント集計、Cu / Actio → Cr の認可) で、両端が動いているときだけ有効として描く。確認は読み取りだけで、Revisor は `repo list` の成否、Curare はトークンがあるときだけ `/api/v1/me` を見る。

## 起動 (Excubitor) {#SPEC-ergo-studio-launch}

デスクトップアプリは Excubitor の catalog code `ergo-studio` で起動する。定義はリポ直下の
`excubitor.catalog.yaml` (Excubitor の per-repo catalog) にある。

- `runtime: app` / `app_kind: native`: `build/studio/ergo_studio.exe` を本体フォルダで実行する。
  死活はプロセスの生存で見る。
- `build_command`: `cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON` →
  `cmake --build build --config Release --target ergo_studio`。Excubitor の deploy (update) で
  Pictor (pin) 込みで構成・ビルドし直す (Vulkan SDK が要る)。
- GUI なので自動起動・自動 respawn はしない。
- 起動は Excubitor 経由・本体フォルダだけ。worktree からは起動しない (Excubitor も worktree の
  catalog を読まない)。Excubitor が `LUDIARS_ROOT` / `EXCUBITOR_URL` を注入する (下表)。

## 設定 (環境変数)

| 変数 | 既定 | 内容 |
|---|---|---|
| `LUDIARS_ROOT` | — | catalog と Revisor CLI の場所 (Excubitor が注入する) |
| `EXCUBITOR_URL` | catalog から | Excubitor の URL (Excubitor が注入する) |
| `ERGO_ACTIO_TOKEN` | 無し = local mode | Actio の Cernere Bearer トークン。無ければ Actio の `ACTIO_LOCAL_MODE` を前提にする |
| `ERGO_CURARE_URL` | topology の `CURARE_URL` | 遠隔の Cu Server |
| `ERGO_CURARE_TOKEN` | 無し | Curare の Cernere Bearer トークン (無ければリソース画面は「認証が必要」) |
| `ERGO_PLUGIN_OWNERS` | `LUDIARS` | プラグイン取得を許す GitHub owner (カンマ区切り) |
| `ERGO_STUDIO_HOME` | `<home>/.ergo-studio` | 台帳・取得したプラグインの置き場 |
| `ERGO_STUDIO_TIMEOUT_MS` | 8000 | サービス呼び出し 1 回の上限 |

## 検証

- ctest (`tests/studio/`、mini-gtest): `test_studio_studio_domain.cpp` が純関数 (指示の組み立て、
  変更系の防御、接続先、取得の判定、取得元と台帳、git 状態・早送り・連携図・設定・失敗の分類)、
  `test_studio_studio_api.cpp` が `/studio/api/*` (HTTP は `fake_http.h` の偽クライアント)。
  旧 `tools/ergo/tests/studio/*.test.ts` (node:test) は 2026-11-06 に削除した。
- 実施していないこと (2026-10-28 時点): 各サービスとの実接続、Curare の
  取得・プラグイン取得の実行、ウィンドウでの表示。

## 未決・今後

- Curare はこの拠点の catalog に無い (遠隔の Cu Server)。`ERGO_CURARE_URL` / `ERGO_CURARE_TOKEN` の
  渡し方 (Excubitor Vault からの注入など) は運用で決める。
- 実行中のサーバへのプラグインの差し込み (再起動なし) はしていない。
- Cc の委託 run の詳細 (セッションの会話) は Cc / Discord 側で見る。Studio は状態だけを出す。
