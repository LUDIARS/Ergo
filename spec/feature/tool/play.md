# 遠隔プレイ (`play` プラグイン) {#SPEC-ergo-play}

依頼 (2026-11-23, neco): 「Web から操作したりアセット確認する実装は Ergo が持つよ。Portus と Custos 参考」。
画面の送り方は WebRTC (同日、人間の選択)。遠隔プレイそのもの (撮影・WebRTC 配信・入力・ICE) は Odeum が持ち
(「Custos 廃止して Odeum にした。初期案どおり Odeum 組み込みで」)、Ergo Studio は Odeum の `odeum_remote_play`
(Odeum `spec/feature/remote-play-library.md`) を組み込む。ゲームのビルド・実行・停止、プロフィール、ページは Ergo が持つ。

参考にしたもの (コードは持ち込まず、決め事を写す):

| 参考 | 写したもの |
|---|---|
| Custos (`spec/feature/remote-play-and-play-log.md`) | アプリの定義 (build / run / 窓 / 操作ボタン)、正規化座標のポインタ、ffmpeg gdigrab → H.264 → WebRTC |
| Odeum `native/runner` (Custos の C++ 版) | libdatachannel の使い方 (send-only の H.264 track、RTP ヘッダーの付け替え、他の m 行の拒否)、Cloudflare TURN の取得、ICE の候補の絞り方 |
| Portus `cpp/` | 外から来る入力には上限を置く、表に書いた経路だけ届く、ログに秘匿値を出さない |

## 構成

| 部分 | 場所 | 役割 |
|---|---|---|
| プロフィール・メッセージ | `tools/studio/plugins/play/` の `play_target` / `play_message` / `target_log` | 目標の定義の読み取り、WS メッセージ、ログ |
| ビルド・実行 | `target_runner` + `include/ergo/studio/platform/live_process.h` (`src/studio/platform/win_live_process.cpp`) | 目標ごとに子プロセス 1 つ (ビルドかゲーム、job object で子孫ごと止める)、状態、ビルドの時間切れ |
| 遠隔プレイの口 | `remote_play.h` (`IRemotePlay`) | 映像・入力・ICE。`remote_play_odeum.cpp` が Odeum の `odeum_remote_play` につなぎ (TURN の資格情報の取得もここ)、`remote_play_unavailable.cpp` はすべて断る |
| 遠隔プレイの本体 | Odeum `native/remote_play` (`Odeum::remote_play`) | 窓の撮影 (ffmpeg)、WebRTC (libdatachannel)、窓への入力 (`SendInput`)、キー名、ICE |
| プラグイン | `play_plugin` | REST (目標・ビルド・実行・停止・ログ・ICE) と WS `/play/ws` (シグナリングと入力) |
| ページ | `tools/studio/plugins/web/page/` のゲームタブ | `<video>`・ビルド / 実行 / 停止・ログ・操作ボタン・キーボードとマウス |

構成で `-DERGO_ODEUM_SOURCE_DIR=<Odeum の checkout>` を渡すと、Odeum の `native/` を取り込んで遠隔プレイを持つ
(Odeum の依存は Odeum の CMake が取る。オフラインなら `FETCHCONTENT_SOURCE_DIR_*` を渡す)。
Odeum を取り込まない構成 (既定) でも、ビルド・実行・停止・ログは動く。offer には
`{"type":"error","error":"webrtc_unavailable"}`、入力には `input_unsupported` を返す。

## プロフィールの `play`

ゲームのリポのプロフィール (`*.studio.json`) に書く。作業フォルダは常にプロフィールの `root`。

```json
"play": [{
  "name": "amicii",
  "label": "Amicii クライアント",
  "build": {"cmd": "node", "args": ["scripts/studio-sample-build.mjs"], "timeout_sec": 3600},
  "run":   {"cmd": "node", "args": ["scripts/launch-client.mjs", "--config", "tools/runner/client-launch.studio.json"]},
  "window": "Amicii",
  "fps": 30,
  "bitrate": 4000000,
  "buttons": [{"label": "決定", "key": "Enter"}, {"label": "戻る", "key": "Escape"}]
}]
```

- `name` は `[a-z0-9-]` 1〜40 文字、重複不可。`build` は省略できる。`run` と `window` は必須。
- `fps` は 5〜60 (既定 30)、`bitrate` は 250k〜20M bps (既定 4M)。`buttons` は 24 個まで、`key` はキー名の表にあるものだけ。
- 読めない項目があればそのプロフィールごとエラーにする (他のプロフィールは読む)。

## API (`play` プラグイン)

| Method | Path | 内容 |
|---|---|---|
| GET | `/play/api/health` | `{ok, rtc}` (`rtc` は WebRTC を作ったか) |
| GET | `/play/api/targets` | 目標の一覧 `{name, label, window, fps, buttons, state, exit_code, viewers}` |
| POST | `/play/api/targets/<name>/build` | ビルドを始める (実行中・ビルド中は 409) |
| POST | `/play/api/targets/<name>/run` | 実行を始める (同上) |
| POST | `/play/api/targets/<name>/stop` | ビルドか実行を止める (子プロセスごと) |
| GET | `/play/api/targets/<name>/logs?since=<n>` | 標準出力・標準エラーの行 (`{next, lines:[{n, stream, text}]}`、1 目標 2000 行まで) |
| GET | `/play/api/ice` | ブラウザに渡す ICE サーバー (STUN と、鍵があれば TURN) |
| WS | `/play/ws` | シグナリングと入力 (下記) |

`state` は `idle` / `building` / `running` / `failed`。

### WS `/play/ws`

1 接続 = 1 視聴。テキストの JSON 1 個ずつ、1 メッセージ 64 KiB まで。

| 向き | type | 中身 |
|---|---|---|
| → | `offer` | `{target, sdp}`。H.264 (packetization-mode=1) の video を含むこと |
| ← | `answer` | `{sdp}` (ICE の候補を集め終えるか 5 秒で返す) |
| → | `key` | `{key, down}` (キー名の表の名前) |
| → | `pointer` | `{x, y, button, phase}`。`x` `y` は 0〜1 (映像の左上原点)、`button` 0/1/2、`phase` `down`/`up`/`move` |
| → | `wheel` | `{x, y, delta}` (`delta` は -10〜10 のノッチ) |
| ← | `state` | `{target, state, exit_code}` (変わったとき) |
| ← | `error` | `{error, message}` |

WS が閉じたら、その視聴の WebRTC 接続も閉じる。目標ごとに視聴は 4 つまで。
入力は「その目標の視聴を開いている接続」からだけ受ける。

## 画面と入力

- 画面: `ffmpeg -f gdigrab -framerate <fps> -i title=<window>` (マウスの矢印も映る) → 辺を偶数に切る → libx264 (`veryfast`・`zerolatency`・
  baseline・yuv420p・`-g <fps>`・`-bf 0`・`-b:v <bitrate>`) → `-f rtp -payload_type 96 rtp://127.0.0.1:<port>?pkt_size=1200`。
  1 目標に ffmpeg は 1 つで、視聴が 0 になったら止める。RTP は各接続の payload type と SSRC に付け替えて送る。
  キーフレームは 1 秒ごと (後から来た視聴も 1 秒以内に映る)。x264 は外から帯域やキーフレームを変えられないので、RTCP (PLI / REMB) は読まない。
- 窓を最小化すると映らない (gdigrab の制約)。ffmpeg は `PATH` か `ERGO_STUDIO_FFMPEG`。
- 入力: Windows の `SendInput`。キーは仮想キーとスキャンコード、ポインタは窓のクライアント領域の座標に直して絶対座標で送る。
  送る前に窓を前面にする。窓が無いときは捨てる (エラーは返さない)。Windows 以外は入力を受け付けない (`error: input_unsupported`)。

## ICE

- STUN: `ERGO_STUDIO_STUN_URLS` (カンマ区切り)、無ければ `stun:stun.cloudflare.com:3478`。
- TURN: `ERGO_STUDIO_TURN_KEY_ID` と `ERGO_STUDIO_TURN_KEY_API_TOKEN` が両方あるとき、Cloudflare の
  `https://rtc.live.cloudflare.com/v1/turn/keys/<id>/credentials/generate-ice-servers` (`{"ttl":86400}`) で取り、期限の 1 時間前まで使い回す。
  ポート 53 の URL は捨てる。片方だけなら起動エラー。鍵はログにも API にも出さない (API は生成された一時的な資格情報だけ)。
- Studio 側の接続に渡すのは `stun:` と UDP の `turn:` だけ (libdatachannel は TURN over TCP/TLS を使わない)。ブラウザには全部渡す。

## 不変条件

### PLAY-INV-01 実行できるのはプロフィールに書いたコマンドだけ {#SPEC-ergo-play-commands}

ビルド・実行のコマンドと引数はプロフィールの `play` だけから取る。要求からはコマンド・引数・作業フォルダ・環境変数を受けない
(目標の `name` だけ)。子プロセスは Studio の環境を受け継ぎ、`shell` を通さない。

### PLAY-INV-02 入力は視聴している接続だけ {#SPEC-ergo-play-input}

`key` / `pointer` / `wheel` は、同じ WS で `offer` が通った目標にだけ届く。値は範囲で切る (座標 0〜1、ボタン 0〜2、ホイール ±10)。
キー名は表にあるものだけで、それ以外は捨てる。

### PLAY-INV-03 公開名からも使えるが、Access を通った要求だけ {#SPEC-ergo-play-access}

`/play/*` は web.md の WEB-INV-01 と同じ検査 (公開名は `Cf-Access-Jwt-Assertion` 付きだけ、WS の Origin も同じ名前の https)。
遠隔プレイのために、ビルド・実行・停止・入力は公開名からも受ける (WEB-INV-02 を改めた)。

### PLAY-INV-04 上限 {#SPEC-ergo-play-limits}

WS メッセージ 64 KiB、SDP 32 KiB、視聴は目標ごとに 4、ログは目標ごとに 2000 行 (1 行 4 KiB で切る)、
ビルドは `timeout_sec` (既定 3600、最大 6 時間) で止める。ffmpeg からの UDP は 2 KiB 以上を捨てる。

## テスト

`tests/studio/test_studio_play.cpp` (プラグイン側): プロフィールの `play` の読み取りと誤り、入力メッセージの解釈と範囲、
行の分割、ビルド・実行の状態とログ、プラグインの一覧・409・404・405・ログ、WS の offer → answer、
「視聴している接続だけ入力が届く」と知らないキーの拒否 (遠隔プレイは偽物)。ffmpeg の引数・RTP・SDP・ICE・キー名は
Odeum の `odeum.remote_play` が試す。WebRTC の実接続・ffmpeg・SendInput は自動テストにしない
(ブラウザと窓が要る)。
