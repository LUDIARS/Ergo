# Ergo Studio: プロジェクトランチャーとリモート開発サーバ (Portus C++)

依頼 (2026-11-23, neco):
1. 「ErgoStudio だけど、最初のランチャーはプロジェクトの選択と前回起動したプロジェクトのキャッシュ登録だけにする。UnityHub や Godot を参考にする」
2. 「で、プロジェクトを選択して『リモート開発サーバ起動』で、アセット確認用の Web サービスやエフェクト生成やゲーム画面の遠隔操作などを Web から出来るようにする。これは昔作っていた tool を C++ (Portus) で再実装するもの」
3. 「Cc と連携してアセット生成は Astra (Codex の Cloud) で AI ドリブンで行えるようにする。エフェクトやモデルの仕様はそれぞれのプロジェクトで定義される」

前提: Ergo #4112 (fd1536e, 段 3) で Studio の UI は Tela の宣言 1 つになり、窓 (Tela GPU) とブラウザ (`/shell/`、Tela::Web) が
同じ宣言を描く。HTTP / WS は Ergo 自前の `src/studio/net`。旧 tool は `tools/ergo` (Node + Electron、2026-11-06 削除、
最後の版は 6ecf079^)。プラグイン: studio / particle / vfx-suggest / vfx-breed / scene_editor / ui_layout / variable / log /
pictor_perf / profile / render_pipeline / rive / visus。Portus `cpp/` は C++17 の libportus (表駆動 HTTP/1.1、WS、log、json、net、
poll 単一ループ + ワーカー、外部依存は OpenSSL のみ)。

## 全体像 (推奨案)

```
ergo_studio.exe (ネイティブ窓, Tela)          ergo_devserver.exe (新規, Portus C++)              Concordia -> Astra (Codex Cloud)
 [ランチャー]                    起動 (子プロセス)  [プロジェクト 1 つ分の Web サービス]   生成依頼     [アセット生成]
  最近のプロジェクト / 一覧      ---------------->   HTTP/WS = Portus                  --------->   プロジェクトの仕様に従い
  開く / 外す / 追加             --project <json>    UI = Tela::Web                    <---------   モデル・エフェクトを作る
  [リモート開発サーバ起動]       <-- URL・稼働 ---   アセット確認 / エフェクト生成 /    成果・状態
                                                     遠隔プレイ (Odeum) / 調整 / ログ
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

## B. リモート開発サーバ (ergo_devserver, 新規)

- 1 プロジェクトにつき 1 プロセス。`ergo_devserver --project <profile.json> --host 127.0.0.1 --port <N>`。Studio が子プロセスとして起動・停止し、
  落ちたら一覧に出す。別の拠点 (Mac / AWS) では Excubitor のサービスとして同じ exe を起動できる。
- HTTP / WS は Portus C++ (`portus/http.hpp` `ws.hpp` `log.hpp`)。表に書いた経路だけ届く・本文の上限・ログの秘匿マスク。
  公開の決まり (Cloudflare Access のヘッダーと Origin の `request_guard`、公開名では loopback 専用の操作を出さない) は引き継ぐ。
- 画面は Tela::Web (Studio と同じ宣言の仕組み・`web/tela.js`)。HTML のページは作らない (2026-11-23「Ergo だけ別の仕組みは嫌」)。
- 機能 (旧 tool からの移植。今の `tools/studio/plugins/*` を devserver 側に載せる):
  アセット確認 (assets) / ゲーム画面の遠隔操作 (play + Odeum `odeum_remote_play`) / エフェクト (vfx_suggest・vfx_breed・particle) /
  調整 (tuning・variable) / ログ (log) / サービス (services) / git
- プラグインは開いたプロジェクトのプロフィールだけを見る (`IProfileSource`。`load_workspace_profiles()` を直接呼ばない)。

## C. AI ドリブンのアセット生成 (Concordia → Astra)

- 仕様の正本は各プロジェクト。プロフィールに生成の定義を足す:
  `"generation": {"model": {"spec": "spec/assets/models", "output": "games/x/models"}, "effect": {"spec": "spec/assets/effects", "output": "games/x/vfx"}}`
  (spec は各プロジェクトの Markdown / JSON。Ergo は中身を解釈せず、種類・一覧・依頼文への添付だけを行う)。
- devserver の「生成」画面: 種類 (モデル / エフェクト) と仕様 (一覧から選ぶ) と追加の指示 → Concordia に依頼。
  Concordia が Astra (Codex Cloud) の委託テンプレートで起動し、結果 (成果物の置き場所・状態) を devserver が Concordia から読む。
  Ergo は Codex Cloud を直接呼ばない (鍵を持たない、委託の記録を Concordia に一本化)。
- 成果物は対象プロジェクトのブランチ / PR (Revisor) で戻る。devserver はアセット確認画面でその版を見られるようにする。
- 既存の経路: Castra の `officina-cloud-production` スキル (Codex Cloud への制作投入・Drive 納品)、Cc の委託テンプレート (astra)。
  どちらに乗せるかは Concordia 側の口で決める (要調査: Cc に「アセット生成依頼」の API があるか、無ければ Cc に足す)。

## D. 段階

1. ランチャー (A) と `IProfileSource`。Studio 単体で完結。
2. `ergo_devserver` の骨組み: Portus の HTTP/WS 上に Tela::Web と plugin host。Studio から起動・停止・URL 表示。
3. 機能の載せ替え: assets → play → vfx / particle → tuning / variable / log / services / git。
4. 生成 (C): プロフィールの `generation`、Cc の依頼 API、Astra テンプレート。
5. 撤去: Studio 窓のタブ群、Ergo 自前の `src/studio/net` (Portus に置き換え後)、web プラグインの HTML (Actio 80ae489f)。

## 決めてほしい点 (推奨を先に)

1. devserver を別プロセスにする (推奨: 遠隔拠点で単体起動でき、Studio の窓が軽い) か、Studio の中で動かすか。
2. Studio の窓から今のタブを消し「ランチャーだけ」にする (推奨: 依頼の文面どおり) か、窓でもタブを残すか。

## テスト

- ランチャー: キャッシュの parse/serialize 往復・並び・上限・重複・壊れたファイル・消えたプロフィール、起動直後はタブが無い宣言。
- devserver: Portus のルート表 (表に無い経路は 404)、本文上限、公開名での loopback 専用操作の拒否、`--project` のプロフィールだけが見えること、
  Studio からの起動・停止。
- 生成: プロフィールの `generation` の解釈 (欠け・不正パス・root の外は拒否)、Cc への依頼本文 (偽の Cc で確認)。
- 既存プラグインのテストは `IProfileSource` の固定実装で渡す形に直す。

## やらないこと

- 新規プロジェクトの作成 (テンプレート生成)。前回プロジェクトの自動オープン。
- Ergo から Codex Cloud を直接呼ぶこと。
