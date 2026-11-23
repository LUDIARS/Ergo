# Ergo Studio の UI を Tela で描く (窓とブラウザ)

設計: `spec/plan/2026-11-23-studio-on-tela.md` (段 3)。Tela 側は Tela `spec/plan/2026-11-23-web-renderer-and-studio.md`
(部品と文字入力・`Tela::Web` は Tela #4105、GPU 描画 `Tela::Gpu` は Tela #4111)。

Studio の UI は 1 つの宣言 (`tela::Document`) にまとめ、それを画面ごとの `tela::Runtime` が配置・入力・フォーカス・
スクロールする。画面は Studio の窓 (Tela の GPU バックエンド) と、ブラウザのページ (`/shell/`) の 2 種類。

## 取り込み (ビルド)

| 構成 | 窓 | ブラウザ (`/shell/`) |
|---|---|---|
| `ERGO_TELA_SOURCE_DIR` が空 (既定) | 今までの ui_kit の窓 (`src/studio/shell/`) | 無い |
| `ERGO_TELA_SOURCE_DIR=<Tela>`、Pictor なし | 無い (ヘッドレス) | ある |
| `ERGO_TELA_SOURCE_DIR=<Tela>` + `ERGO_PACKAGES=pictor` (Vulkan SDK) | Tela の GPU 描画 (`Tela::Gpu`) | ある |

- 段 3 では **Tela の取り込みは任意** とする。Tela が無い構成でも Studio はこれまでどおりビルドでき、窓も
  テストもそのまま動く。段 4 (旧 UI 層の撤去) で必須にする。
- Tela は `add_subdirectory` で取り込む (`cmake/ErgoStudioTela.cmake`。Odeum と同じ口)。Tela のテストと例は
  ビルドしない (`EXCLUDE_FROM_ALL`、`BUILD_TESTING=OFF`)。
- Tela は C++20 で、公開ヘッダーも C++20 の書き方 (`operator==` の default、指示付き初期化子) を使うため
  C++17 では読めない。そこで Tela のヘッダーを読むターゲットだけを C++20 にする: `ergo_studio_tela`・
  `ergo_studio_tela_window`・`ergo_studio`・`test_studio_tela`。ビュー (`tools/studio/views/`) と
  `ergo_studio_ui` は C++17 のまま (Tela を知らない)。
- Tela の Pictor アダプタは Pictor を **ファイル** として探す (`find_library`) ため、この構成で作る `pictor`
  ターゲットをそのまま渡せない。`pictor` の出力先を `<build>/_tela_pictor/<config>/` に固定し、そのファイルを
  Tela に渡して、Tela の GPU ターゲットを `pictor` の後にビルドさせる。パッケージハブ (`ERGO_PACKAGES=tela`) で
  Tela がすでにターゲットとしてある場合は、それをヘッドレスで使う (GPU なし)。

## 宣言 (`src/studio/tela/`)

- ビューの公開の形 (`ViewBuilder` のメソッド) は変えない。`ViewBuilder` は描き手に依存しない `ViewNode` の木
  (`include/ergo/studio/ui/view_node.h`) を作り、それを 2 つの書き出しが描く:
  - `ui_kit_emitter` — Tela なし構成の窓 (今までの見た目)
  - `tela_emitter` — `tela::Document`
- 対応 (`tela_emitter.cpp`):

| ViewBuilder | Tela |
|---|---|
| `title` / `label` / `muted` / `paragraph` / `wrapped` / `clipped_label` / `field` | `text` (大きさは `text_scale`) |
| テーマの ok / warn / error の色の `label` | `status` (good / warning / bad)。Tela の text には色の指定が無い |
| `button` | `button` |
| `text_field` / `secret_field` / `text_area` | `text_field` (`secret`) / `text_area`。値はビューの `std::string`、`on_change` で書き戻す |
| `list_item` / `slider` / `status` / `separator` | 同名 |
| `spacer` | 空の `text` (高さだけ) |
| `row` / `panel` | `panel` (`Flow::row` / `Flow::column`) |
| `video` (新設) | `video` |
| タブの中身全体 | `scroll` |

- ID は node の名前をつないだパス (`content/<プラグイン id>/panel0/button2`)。タブごとに `content/<id>` の
  下に入るので、スクロール位置とキャレットはタブごと・画面ごとに残る。
- Tela の row は幅 0 の子に残りの幅を全部渡すため、row の子には文字幅から求めた自然な幅を与える
  (文字幅は Tela と同じ見積もり: ASCII 0.55 em、それ以外 1 em)。
- `ViewBuilder::node()` は `ViewNode&` を返す (固定幅 `width`、`align_start`、`fill`)。Tela では
  `align_start` と `fill` は描かない (パネルの色は 1 つ)。
- `ViewBuilder::name_next(name)` で次の要素の名前を決められる (ページのスクリプトとテストが探す)。

## 画面ごとの Runtime

- `StudioDeclaration` が宣言を持つ。作り直すのは: 操作 (ボタン・一覧・入力・スライダー) の後、表示中の
  ビューの revision が変わったとき、概要タブと汎用タブのヘルスの定期更新 (1 秒)。表示中のタブも宣言の
  一部なので、どの画面で選んでも全画面で同じタブになる。
- `ScreenHub` が新しい宣言を各画面の `Runtime` に渡す。値の正本はプラグイン (ビュー) なので、どの画面で
  変えても全画面に出る。フォーカス・キャレット・スクロールは画面ごと。
- すべて 1 つの UI スレッドで動く (ビューの callback もそこで走る)。窓があるときは窓のループ、
  無いとき (ヘッドレス) は main スレッドが 16 ms ごとに `ScreenHub::pump()` を回す。

## ブラウザ (`/shell/`)

| 経路 | 内容 |
|---|---|
| `GET /shell/` | ページ (canvas、`tela.js`、`shell.js`) |
| `GET /shell/tela.js` | Tela の `web/tela.js` (ビルド時に実行ファイルに埋め込む) |
| `GET /shell/shell.js` | ページのスクリプト (`src/studio/tela/page/shell.js`) |
| `WS /shell/ws` | ページ 1 枚 = `tela::Runtime` 1 つ + `tela::WebSession` 1 つ |

- `shell` はプラグイン id として予約済み (`is_reserved_plugin_id`)。経路は `ShellRoutes` がプラグインホストの
  前に置く。
- 接続元の検査は今の `request_guard` のまま (Host、Origin、公開名では Cloudflare Access のヘッダー)。
- 同時に開けるページは 8 枚まで。9 枚目は 1013 で閉じる。入力の大きさの上限は Tela の `WebLimits` の既定。
- **公開名から開いたページ** (Host が loopback の名前でも bind host でもない) では、変更系が loopback に限られる
  タブ (`studio` / `lesson`、`mutation_guard.h`) の中身を操作できない: ボタン・一覧・入力・スライダーの操作を
  宣言に入れず、タブの先頭に「公開名からはこのタブを操作できません」と出す。タブの切り替えはできる。
  ビューはプラグインの API をプロセス内から loopback の名前で呼ぶため、この判定は宣言の側で行う。

## 追加したタブ

- **アセット** (`assets` プラグイン、`tools/studio/views/assets_view.cpp`): プロフィールの assets のフォルダを
  選び、フォルダを順にたどる (種類・大きさ・更新時刻)。文字のファイル (data / shader / glTF) は中身を出す。
  画像は一覧に出すが、画像そのものはまだ描けない (Studio に画像のデコーダが無い)。
- **遠隔プレイ** (`play` プラグイン、`tools/studio/views/play_view.cpp`): 目標の選択・ビルド・実行・停止・
  ログ・操作ボタン・映像。映像は `video` 要素で、見ている間は stream キーが `play:<目標名>` になる。
  - ブラウザでは `shell.js` がそのキーを見て `/play/ws` (シグナリングは今のまま) で WebRTC をつなぎ、
    `tela.js` の `attachStream` で映像を重ねる。映像の上のポインタ・ホイール、映像を押した後のキー、
    操作ボタン (要素 id の末尾 `/key-<名前>`) の押す・離すは同じ `/play/ws` でゲームへ送る
    (play プラグインは映像を見ている接続からの入力だけを受ける)。
  - 窓には WebRTC が無いので、枠と「映像はブラウザの Studio (/shell/) に出ます」を出す。

## テスト

`tests/studio/test_studio_tela.cpp` (`ERGO_TELA_SOURCE_DIR` があるときだけ):
ViewBuilder が作る `tela::Document` の形 (部品の種類・ID・値・Layout)、タブの切り替えと作り直し、
窓とページの 2 つの Runtime で値が揃いフォーカスは別なこと、ページの入力 (`/shell/ws` の受け渡し) で
callback が走ること、`/shell/` の配信と接続数の上限、公開名のページで loopback 限定タブが操作できないこと。
Tela なし構成の既存テスト (`test_studio_ui` など) は ui_kit の書き出しを通して従来どおり。

窓とブラウザでの実際の見た目・操作感 (文字の位置、GPU 描画、WebRTC の映像) は人間の確認として残す。

## 段 4 (この段ではしない)

`web` プラグインの HTML ページ、`src/studio/shell/` の ui_kit 描画・フォントアトラス・文字の canvas、
`ergo_studio_ui` の `ui_kit` 依存 (`ui_kit_emitter`・`StudioApp`) を消し、Tela の取り込みを必須にする。
