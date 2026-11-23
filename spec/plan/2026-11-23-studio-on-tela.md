# Ergo Studio の UI を Tela に載せ替える (段 3・段 4)

依頼 (2026-11-23, neco): 「ErgoStudio のウインドウ、UI を Web 経由で描画する」「Tela を Web レンダリングする実装を作る」
「Ergo だけ別の仕組みなのはなんか嫌だな。Tela で実装し直す」「Tela の描画を GPU に対応して」。
全体の計画は Tela `spec/plan/2026-11-23-web-renderer-and-studio.md`。Tela 側の段 1 (部品と文字入力)・段 2 (`Tela::Web`) は
Tela #4105 でマージ済み、GPU 描画は委託中 (run 62da99f7)。

## 今の Studio の UI

| 部分 | 場所 | Tela に移した後 |
|---|---|---|
| タブの中身 (15 個) | `tools/studio/views/*.cpp` | そのまま (`ViewBuilder` の公開の形を変えない) |
| 組み立て役 | `src/studio/ui/view_builder.cpp` (`ergo_ui_kit` の木を作る) | `tela::Document` を作る |
| アプリ (タブ・スクロール・フォーカス・文字入力・クリック) | `src/studio/ui/studio_app.cpp` (`ui_kit::Context` の描画リスト) | 宣言の組み立てだけ。配置・入力・フォーカス・スクロールは Tela の `Runtime` |
| 窓 | `src/studio/shell/` (GLFW + Vulkan + Pictor UIRenderer + フォントアトラス + 文字の canvas) | GLFW + Vulkan の窓はそのまま、描画は Tela の GPU バックエンド |
| ブラウザ | `web` プラグインの HTML ページ | Tela の `WebSession` + `web/tela.js` で同じ UI |

## ViewBuilder → Tela の対応

| ViewBuilder | Tela |
|---|---|
| `title` / `label` / `muted` / `paragraph` / `wrapped` / `clipped_label` / `field` | `text` (文字の大きさ・色・行数を Layout とテーマで) |
| `button(primary)` | `button` |
| `text_field` / `secret_field` / `text_area` | `text_field` (`secret`) / `text_area`。値は `std::string&` のまま、`on_change` で書き戻す |
| `list_item` | `list_item` |
| `slider` | `slider` |
| `status` / `separator` / `spacer` | `status` / `separator` / 空の `panel` |
| `row` / `panel` | `panel` (`Flow::row` / `Flow::column`) |
| タブの中身全体 | `scroll` |
| 遠隔プレイの映像 | `video` (ブラウザでは MediaStream を重ねる。窓では枠と文言) |

ID は今の node path (`ViewBuilder` が数えている名前) をそのまま Tela の安定 ID にする。

## 画面ごとの Runtime

- 宣言 (`tela::Document`) は 1 つ。`StudioApp` がプラグインの revision が変わったときに作り直す。
- 画面ごとに `tela::Runtime` を 1 つずつ持つ: ネイティブの窓 1 つ、ブラウザのページ 1 つにつき 1 つ (`WebSession`)。
  値の正本はプラグインなので、どの画面で変えても全画面に反映される。フォーカス・キャレット・スクロールは画面ごと。
- ブラウザへは Studio の WS `/shell/ws` (新設) で `WebSession::poll()` のメッセージを送り、受けた入力を `receive()` に渡す。
  ページは `GET /shell/` で `web/tela.js` を配る (Tela の `web/tela.js` を実行ファイルに埋め込む)。
- 公開名の検査は今の `request_guard` のまま (Cloudflare Access のヘッダーと Origin)。`studio` / `lesson` の変更系を
  loopback に限る決まりも、Tela の操作経由でも同じに保つ (公開名からの操作では、その操作を出さないか断る)。

## タブを足すもの

HTML のページにしか無かったものを Studio のタブにする。

- アセット: プロフィールの `assets` の一覧 (順にたどる・種類・大きさ・更新時刻) と、画像・文字ファイルの表示。
- 遠隔プレイ: 目標の選択・ビルド・実行・停止・ログ・操作ボタン、`video` 要素。WebRTC の接続はブラウザ側の `tela.js` の
  拡張点で行う (`/play/ws` のシグナリングは今のまま)。

## 段 4: 撤去

Tela の窓とブラウザで同じことができるのを確かめてから消す。

- `web` プラグインの HTML ページ (`tools/studio/plugins/web/page/`) と `GET /web/api/config`
- `src/studio/shell/` の Pictor UIRenderer の UI 層・フォントアトラス・文字の canvas、`ergo_studio_ui` の `ui_kit` 依存
- 関係する仕様 (web.md の HTML のページの節) とテスト

## 未確定

- Tela の GPU バックエンドの口 (委託の PR で決まる)。窓の作り (`StudioWindow`) をどこまで残すかはそれに合わせる。
- ブラウザの画面の大きさと DPI (ページの大きさに合わせて配置し直す。`WebSession` の resize)。

## 段 3 で決めたこと

- Tela の取り込みは段 3 では任意 (`ERGO_TELA_SOURCE_DIR` が空なら今の ui_kit の窓のまま)。段 4 で必須にする。
- Tela の公開ヘッダーは C++20 の書き方を使うので C++17 では読めない。Tela のヘッダーを読むターゲットだけ C++20 にし、
  ビューと `ergo_studio_ui` は C++17 のまま (描き手に依存しない `ViewNode` の木を挟む)。
- GPU バックエンドの口は Tela #4111 の `tela::GpuRenderer` (`GpuTarget` = Pictor の VulkanContext + render pass)。
  窓 (`StudioWindow`) の GLFW・swapchain・FrameComposer はそのまま使い、UI 層だけを `TelaLayer` に替えた。
- ブラウザの大きさと DPI は `tela.js` の resize (ResizeObserver) がページごとに送り、Runtime がそれで配置し直す。
- 詳細は `spec/feature/tool/shell.md`。

## テスト

`ViewBuilder` が作る `tela::Document` の形 (部品の種類・ID・値・Layout)、`StudioApp` のタブの切り替えと作り直し、
ネイティブとブラウザの 2 つの Runtime で値が揃い、フォーカスが別々なこと、`/shell/ws` の受け渡し (`WebSession` の
入出力)、公開名からの変更系の扱い。窓とブラウザでの実際の見た目は、人間の確認として残す。
