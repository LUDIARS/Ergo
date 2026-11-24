# Ergo Studio: プロジェクトランチャーとリモート開発サーバ (Studio の Web サーバ化, Portus C++)

依頼 (2026-11-23, neco):
1. 「ErgoStudio だけど、最初のランチャーはプロジェクトの選択と前回起動したプロジェクトのキャッシュ登録だけにする。UnityHub や Godot を参考にする」
2. 「で、プロジェクトを選択して『リモート開発サーバ起動』で、アセット確認用の Web サービスやエフェクト生成やゲーム画面の遠隔操作などを Web から出来るようにする。これは昔作っていた tool を C++ (Portus) で再実装するもの」
3. 「Cc と連携してアセット生成は Astra (Codex の Cloud) で AI ドリブンで行えるようにする。エフェクトやモデルの仕様はそれぞれのプロジェクトで定義される」
4. 「開発サーバだけど Studio が Web サーバ化して CF Tunnel や Tailscale でアクセス出来るようになる感じね」(2026-11-24) → 別プログラムにしない (§B)

前提: Ergo #4112 (fd1536e, 段 3) で Studio の UI は Tela の宣言 1 つになり、窓 (Tela GPU) とブラウザ (`/shell/`、Tela::Web) が
同じ宣言を描く。HTTP / WS は Ergo 自前の `src/studio/net`。旧 tool は `tools/ergo` (Node + Electron、2026-11-06 削除、
最後の版は 6ecf079^)。プラグイン: studio / particle / vfx-suggest / vfx-breed / scene_editor / ui_layout / variable / log /
pictor_perf / profile / render_pipeline / rive / visus。Portus `cpp/` は C++17 の libportus (表駆動 HTTP/1.1、WS、log、json、net、
poll 単一ループ + ワーカー、外部依存は OpenSSL のみ)。

## 全体像 (決定 2026-11-24)

```
ergo_studio.exe (1 つのプログラム)                                         Concordia -> Astra (Codex Cloud)
 [窓 = ランチャー, Tela GPU]                                               生成依頼   [アセット生成]
  最近のプロジェクト / 開く / 外す / 追加                                  --------->  プロジェクトの仕様に従い
  [リモート開発サーバ起動] ─┐                                               <---------  モデル・エフェクトを作る
                            v
 [Web サーバ = 開いたプロジェクトの開発サーバ, Portus HTTP/WS + Tela::Web]
  手元の口  127.0.0.1:<N>  ── 手元のブラウザ (全操作)
  遠隔の口  127.0.0.1:<M>  ── cloudflared (CF Tunnel + Access) ── 外のブラウザ
                           └─ tailscale serve (*.ts.net)       ── tailnet の端末
  アセット確認 / エフェクト生成 / 遠隔プレイ (Odeum) / 調整 / ログ
```

## A. ランチャー (ergo_studio)

| 項目 | 決定 | 参考 |
|---|---|---|
| プロジェクト | ワークスペースプロフィール 1 つ (`WorkspaceProfile`、例 amicii)。識別子はプロフィールファイルの絶対パス | Godot は project.godot、Unity Hub はフォルダ |
| 起動直後 | ランチャーだけ。前回のプロジェクトを自動で開かない | 両者の既定 |
| 一覧 | 最近起動した順 → 見つかったプロフィール (profiles/ と ERGO_PLUGIN_DIR の *.studio.json) で最近に無いもの。行: 名前・root・最後に起動した時刻・サーバの状態 (稼働中なら URL) | Godot Project Manager |
| 操作 | 「リモート開発サーバ起動」/「停止」/「ブラウザで開く」、「一覧から外す」、「パスを入れて追加」 | Unity Hub の Add / Remove from list |
| キャッシュ | `<studio home>/recent_projects.json` `{"version":1,"projects":[{"source","name","root","last_opened"}]}`。起動したら先頭へ、上限 30、一時ファイル→置き換え。壊れていたら空として扱い原本を上書きしない | Godot projects.cfg、Unity Hub projects-v1.json |
| 今のタブ | ランチャーからは消す (Studio の窓はランチャーだけ) | |

## B. リモート開発サーバ (Studio 自身が Web サーバになる)

neco (2026-11-24): 開発サーバは別プログラム (旧案の `ergo_devserver`) にせず、Studio が Web サーバになり、Cloudflare Tunnel や
Tailscale から開けるようにする。

- 「リモート開発サーバ起動」で、Studio が開いたプロジェクトの Web サーバを始める。開けるプロジェクトは同時に 1 つ
  (別のプロジェクトを起動すると、今の方を止めてから切り替える)。「停止」で Web サーバを止め、窓はランチャーに戻る。
- 窓はランチャーだけ。機能の画面 (タブ) は Web 側にだけ出す。窓とブラウザは同じ Tela の宣言の仕組みのまま
  (段 3 の `/shell/`、HTML のページは作らない)。
- 拠点 (Mac / AWS) では `ergo_studio --headless --project <profile.json>` を Excubitor のサービスとして起動する
  (窓を出さず、起動直後から Web サーバ)。
- HTTP / WS は Portus C++ (`portus/http.hpp` `ws.hpp` `log.hpp`) に移す。表に書いた経路だけ届く・本文の上限・ログの秘匿マスク。
  Ergo 自前の `src/studio/net` は移し終えたら消す (段 5)。

### 口を 2 つに分ける

cloudflared も `tailscale serve` も Studio には 127.0.0.1 から接続してくるので、相手のアドレスでは手元か遠隔かを
区別できない。**待ち受けの口で分ける**。

| 口 | 待ち受け | 届く相手 | できること |
|---|---|---|---|
| 手元 | `127.0.0.1:<N>` | この PC のブラウザ | 全操作 (プロジェクトの追加・削除、プラグインの導入、変更系) |
| 遠隔 | `127.0.0.1:<M>` (既定は無効) | cloudflared / `tailscale serve` の転送先にだけ使う | 閲覧と、プロフィールで遠隔に許した操作だけ。loopback 専用の操作は出さない・断る |

- 遠隔の口に届く要求は、Host で入口を確かめる:
  - Cloudflare: Host が設定した公開名 (`ERGO_STUDIO_PUBLIC_HOSTS`) で、Cloudflare Access のヘッダーがあること (今の `request_guard`)。
  - Tailscale: Host が設定した tailnet 名 (`ERGO_STUDIO_TAILNET_HOSTS`、`*.ts.net`) で、`tailscale serve` が付ける
    `Tailscale-User-Login` があること。tailnet の外 (Funnel) からの要求はこのヘッダーが無いので断る。
  - どちらにも当たらない Host・Origin の食い違いは断る。手元の口に公開名・tailnet 名の Host が来ても断る。
- 誰がアクセスしたか (Access のメール / Tailscale のログイン名) をログと操作の記録に残す。
- 公開の設定 (Tunnel の経路と Access、`tailscale serve` の設定) は Studio の外で行う。LUDIARS では
  Excubitor のサービスとして登録し、Tunnel と Access は `cf-access-publish` の手順、`tailscale serve` は人が設定する。
  Studio は自分で公開の設定を変えない。
- 機能 (旧 tool からの移植。今の `tools/studio/plugins/*` を Web 側に載せる):
  アセット確認 (assets) / ゲーム画面の遠隔操作 (play + Odeum `odeum_remote_play`) / エフェクト (vfx_suggest・vfx_breed・particle) /
  調整 (tuning・variable) / ログ (log) / サービス (services) / git
- プラグインは開いたプロジェクトのプロフィールだけを見る (`IProfileSource`。`load_workspace_profiles()` を直接呼ばない)。

## C. AI ドリブンのアセット生成 (Concordia → Astra)

- 仕様の正本は各プロジェクト。プロフィールに生成の定義を足す:
  `"generation": {"model": {"spec": "spec/assets/models", "output": "games/x/models"}, "effect": {"spec": "spec/assets/effects", "output": "games/x/vfx"}}`
  (spec は各プロジェクトの Markdown / JSON。Ergo は中身を解釈せず、種類・一覧・依頼文への添付だけを行う)。
- Web 側の「生成」画面: 種類 (モデル / エフェクト) と仕様 (一覧から選ぶ) と追加の指示 → Concordia に依頼。
  Concordia が Astra (Codex Cloud) の委託テンプレートで起動し、結果 (成果物の置き場所・状態) を Studio が Concordia から読む。
  Ergo は Codex Cloud を直接呼ばない (鍵を持たない、委託の記録を Concordia に一本化)。
- 成果物は対象プロジェクトのブランチ / PR (Revisor) で戻る。Studio はアセット確認画面でその版を見られるようにする。
- 既存の経路: Castra の `officina-cloud-production` スキル (Codex Cloud への制作投入・Drive 納品)、Cc の委託テンプレート (astra)。
  どちらに乗せるかは Concordia 側の口で決める (要調査: Cc に「アセット生成依頼」の API があるか、無ければ Cc に足す)。

## D. 段階

1. ランチャー (A) と `IProfileSource`。Studio 単体で完結。
2. Studio の Web サーバ化: Portus の HTTP/WS に移し、手元と遠隔の 2 つの口、Host とヘッダーでの入口の確認、ランチャーからの起動・停止・URL 表示、`--headless --project`。
3. 機能の載せ替え: assets → play → vfx / particle → tuning / variable / log / services / git。
4. 生成 (C): プロフィールの `generation`、Cc の依頼 API、Astra テンプレート。
5. 撤去: Studio 窓のタブ群、Ergo 自前の `src/studio/net` (Portus に置き換え後)、web プラグインの HTML (Actio 80ae489f)。

## 決まったこと

- 開発サーバは別プログラムにせず Studio が Web サーバになる (neco 2026-11-24)。旧案の `ergo_devserver` は作らない。
- Studio の窓はランチャーだけにし、機能の画面は Web 側に出す (依頼 1 の文面どおり)。

## テスト

- ランチャー: キャッシュの parse/serialize 往復・並び・上限・重複・壊れたファイル・消えたプロフィール、起動直後はタブが無い宣言。
- Web サーバ: Portus のルート表 (表に無い経路は 404)、本文上限、`--project` のプロフィールだけが見えること、ランチャーからの起動・停止と切り替え。
- 口と入口: 遠隔の口では loopback 専用の操作を断る / 公開名で Access のヘッダーが無ければ断る / tailnet 名で `Tailscale-User-Login` が無ければ断る /
  知らない Host・Origin の食い違いは断る / 手元の口に公開名・tailnet 名が来たら断る / 遠隔の口は既定で無効。
- 生成: プロフィールの `generation` の解釈 (欠け・不正パス・root の外は拒否)、Cc への依頼本文 (偽の Cc で確認)。
- 既存プラグインのテストは `IProfileSource` の固定実装で渡す形に直す。

## やらないこと

- 新規プロジェクトの作成 (テンプレート生成)。前回プロジェクトの自動オープン。
- Ergo から Codex Cloud を直接呼ぶこと。
