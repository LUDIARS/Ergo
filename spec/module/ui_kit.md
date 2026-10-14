# ergo_ui_kit モジュール定義

> 状態: 実装済みのモジュールに後から付けた仕様書のひな形 (2026-10-14)。
> 公開ヘッダ (`include/ergo/ui_kit/`) とテスト (`tests/ui_kit/`) から読み取れる範囲だけを書いている。
> 足りない節は次に手を入れる人が埋める。

## 概要

リテインドモードの UI コンポーネントフレームワーク。 ノードのツリー (Frame / Text /
Image / Shape / NineSlice) を `uidoc.json` から読み込み、 毎フレーム
レイアウトを解決して GPU に依存しない描画リスト (`DrawCmd` の配列) を出す。
描画そのものは Pictor の `UIRenderer` が受け持ち、 本モジュールはツリー・レイアウト・
入力ディスパッチ・アニメーションを受け持つ。

レイアウトは auto-layout (横 / 縦、 gap・padding・主軸 / 交差軸の整列、 Fixed / Hug / Fill)
と、 親が auto-layout でないときのピン留め (constraints) の 2 通り。

`ergo_ui` (SVG ラスタ + 9-slice の部品) と `ergo_ui_layout` (データ駆動のレイアウト +
バインド) とは層が違う。 `ergo_ui_layout` はプリミティブの描画に本モジュールを使う側
(`spec/feature/module/ui_layout.md`)。

## カテゴリ

UI

## 所属ドメイン

`ui-layout` (`spec/domains/ui-layout.domain.json`)。

## 必要なデータ

- `uidoc.json`: `root` ノード 1 つ。 ノードは `name` / `kind` / `w` / `h` / `layout` /
  `style` / `text` / `children` を持つ。 `serialize_document` で同じ形に書き戻せる。
- ビューポートの大きさ (既定 1280x720)。
- ホストが毎フレーム渡すポインタ状態 (`PointerInput`)。

## 依存

- C++17 標準ライブラリ (`<functional>` / `<string>` / `<unordered_map>` / `<vector>`)
- `ergo_common` (JSON の読み書き)、 `ergo_io` (ファイル読み込み)
- テスト: mini-gtest (`ergo_gtest_main`)

GPU・Pictor には依存しない。

## 変数

- ロード済みの `Document` (ノードのツリー。 解決後の矩形・不透明度・パスを各ノードに持つ)
- ビューポートの幅と高さ
- 直近の描画リスト
- 進行中のアニメーション (ノードのパス・プロパティ・始点・終点・長さ・経過・イージング)
- ノードのパスごとのイベントハンドラ
- 今回と前回のポインタ状態、 ホバー中・押下中のノードのパス

## 作業

### 入力

- `load_document(path, doc)` / `parse_document(json, doc)` → `UIContext::set_document(doc)`
- `UIContext::set_viewport(w, h)`
- `UIContext::feed_input(PointerInput)`: 次の `update` でヒットテストとイベント発火に使う
- `UIContext::on(path, EventKind, handler)`: PointerEnter / Exit / Down / Up / Click
- `UIContext::animate(path, AnimProp, to, duration, easing)` / `stop_anim(path)`

### 出力

- `UIContext::draw_list()`: Rect / NineSlice / Text / Image / PushClip / PopClip の描画コマンド
- イベントハンドラの呼び出し。 起点のノードから祖先へバブリングし、
  `Event::propagate = false` で止まる
- `serialize_document(doc)`: `uidoc.json` の文字列

### タスク

`UIContext::update(dt)` が 1 フレームの処理をすべて行う。 順番はアニメーションを進める →
レイアウトを解決する → 入力をディスパッチする → 描画リストを作る。 スレッドは持たない。

## テスト

`tests/ui_kit/test_ui_kit.cpp`:

- `uidoc.json` を読むとツリーができる
- 縦 / 横の auto-layout で子が正しい位置と大きさに置かれる (整列・stretch を含む)
- 描画リストに矩形と文字が出る
- 書き出して読み直すと同じツリーになる
- 不透明度のアニメーションが目標値まで進む
- クリックがノードに届き、 親へバブリングする
