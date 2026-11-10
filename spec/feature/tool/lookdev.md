# lookdev — ゲームのライティングを遊びながら調整する

Ergo Studio の `lookdev` プラグインとそのタブ。
ゲームは自分の窓のまま動かして操作し、Studio のタブでライティングの値を変える。
変えた結果は、その場の画面と設定ファイルを添えて Discord / Slack に投稿できる。
Ergo はゲームの名前も、設定の中身も知らない。
ゲームが下の取り決めに沿って変数を出せば、どのゲームでも使える。

## 構成

| 部分 | 場所 | 役割 |
|---|---|---|
| 変数の中継 | `variable` プラグイン (`ergo_bind`) | ゲームの変数と、その値の変更 |
| `lookdev` プラグイン | `tools/studio/plugins/lookdev/` | プリセットの保存と読み出し、撮影フォルダ、Concordia への投稿 |
| ルックデヴのタブ | `tools/studio/views/lookdev_view.*` | 変数をグループ別にスライダー・切り替え・数値欄で出す。撮影と投稿、プリセットのボタン |

## ゲーム側の取り決め (ergo_bind の変数)

| 変数 | 種類 | 向き | 意味 |
|---|---|---|---|
| `look.<項目>` | float / int32 / bool / vec3 / color | 双方向 | 調整する値。カテゴリは `look/<グループ>` で、タブにはグループごとに並ぶ。`min`・`max`・`step` はスライダーの範囲になる |
| `look.apply` | string | Studio → ゲーム | `名前=値` の行 (プリセット) を書くと、ゲームがまとめて反映して空に戻す |
| `look.preset` | string (読み取り専用) | ゲーム → Studio | 今の設定をプリセットの文字列にしたもの。保存ボタンはこれをそのまま保存する |
| `look.capture_request` | string | Studio → ゲーム | フォルダの絶対パスを書くと、ゲームが次のフレームをそこへ PNG で保存し、同じ名前の `.look` に設定を書き、空に戻す |
| `look.capture_path` | string (読み取り専用) | ゲーム → Studio | 最後に保存した PNG のパス。これが変わったら撮影は終わっている |

`look/Capture` カテゴリの変数は、調整の一覧に出さない。
vec3 は RGB として、color は RGB と補正値 (4 つ目) として、成分ごとのスライダーで出す。

## 撮影と投稿

1. タブの「撮影して投稿」を押す。
2. Studio が `POST /lookdev/api/capture-dir` で新しいフォルダを作る。フォルダは `<一時フォルダ>/ergo-studio-lookdev/<UTC 時刻>` で、パスは長い名前に直す。
3. そのパスを `look.capture_request` に書く。
4. `look.capture_path` が変わったら、`POST /lookdev/api/post` で投稿する。中身は PNG、その `.look`、コメント、接続したときから変えた項目。
5. プラグインは Concordia の `POST /v1/chat` に送る。チャンネルは `報告`、添付は `attachment_paths`。Concordia はこれを Discord と Slack に流す。

投稿できる画像は、撮影フォルダの中の `.png` に限る。
リクエストから、任意のファイルを添付させることはできない。
Concordia の URL は、`studio` プラグインと同じく Excubitor のトポロジーから引く。ポートを書かない。

## プリセット

プリセットは `<ERGO_STUDIO_HOME>/lookdev/presets/<名前>.look` に保存する。中身は `名前=値` の行。
名前に使える文字は、英数字・`-`・`_`・`.`・空白と非 ASCII の文字。区切り文字は拒む。
読み込むときは、中身を `look.apply` に書く。
同じファイルは、ゲームの起動引数 (Amicii では `--look-file`) でも読める。

## 状態とエラー

- 変数を出しているゲームがいなければ、タブは「ゲームが接続していません」と、起動のしかたを出す。
- `/lookdev/api/health` は、プリセットと撮影のフォルダを返す。Studio のホームか一時フォルダが分からないときは `ok: false` を返す。
- 投稿に失敗したときは、Concordia の答えをそのまま通知行に出す。投稿はやり直さない。
