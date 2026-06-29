# ergo_ui 仕様

## 概要

2D UI 描画のための **ヘッドレスユーティリティ** モジュール。

- **SVG ラスタ** (nanoSVG ラップ) — SVG を RGBA8 バイト列に焼く。
- **9-slice コンポーザ** — 四隅を固定ピクセルで保ったまま任意サイズへ伸縮。
  ウィンドウ枠 / アイコン枠 / リスト枠のような装飾枠を 1 枚のテンプレート
  から任意サイズに展開するのに使う。
- **Gradient ペインタ** — linear / radial グラデーションを直接 RGBA8 に描く。
  SVG 内 `<linearGradient>` と違って、stops と角度 / 中心を実行時に
  編集できる。frame の背景に別レイヤとして敷くのに便利。
- **Line-glow オーバーレイ** — 既に合成済みの frame bitmap に、境界線に
  沿って左→右 (あるいは周回) で流れる光を **アニメーションフレーム単位**
  で載せる。time_sec を渡すと位相が進む、ゲームループ 1 tick で 1 回呼ぶ
  前提の設計。bake 済み画像と違い、呼ぶたびに異なるフレームが生成される
  ので、caller はその出力を毎フレーム texture にアップロードする。

Pictor / Vulkan / GPU 依存は一切持たず、返すのは素の
`std::vector<uint8_t>` 入り `Bitmap` のみ。ホスト側は必要に応じて
`pictor::TextureRegistry::register_texture` へ渡すなり、テスト内で
バイトを直接検証するなり自由に使える。

## カテゴリ

システム (UI ビルディングブロック)

## 所属ドメイン

UI — AdventureCube の装備 UI / HUD 枠 / 設定ダイアログなど、動的にサイズが
変わる枠の合成で共有する想定。ゲームロジックやネットワーク層には触れない。

## 必要なデータ

- **SVG ソース**: ディスク上の `.svg` ファイル、もしくは文字列で埋め込んだ
  SVG。`viewBox` + `width/height` が揃っていれば OK。
- **SliceInsets**: 9-slice の四隅サイズ (pixel) — `left/top/right/bottom`。
  素材のどこまでが「角」かを指定する。

## 依存

- `third_party/nanosvg/` (header-only, zlib-like) を同梱。他に外部依存なし。
- テストは `ergo_gtest_main` に相乗り。

## 変数

なし (モジュール自体は関数だけの純粋ユーティリティ)。

## 作業

### 入力

- `raster_svg_file(path, w, h, opts)` / `raster_svg_memory(src, w, h, opts)`
  → `Bitmap`
- `compose_nine_slice(src, insets, target_w, target_h, mode)` → `Bitmap`
- `compose_gradient(spec, w, h)` → `Bitmap`
  (`GradientSpec { kind, stops[], angle_deg, cx, cy, radius }`)
- `overlay_line_glow(src, spec, time_sec)` → `Bitmap`
  `overlay_line_glow_into(dst, spec, time_sec)` → bool (in-place 上書き)
  (`LineGlowSpec { path, blend, r,g,b, peak_alpha, thickness_px,
                    head_length_px, period_seconds, phase }`)

### 出力

- `Bitmap { width, height, rgba[] }` — RGBA8、row-major、top-down。
- 失敗時は `Bitmap::valid() == false` を返し、optional `out_err` に
  エラー文字列 (`"ergo::ui::..."` プレフィクス) を書く。

### タスク

1. SVG 枠アセットをラスタ (典型: 128×128 〜 256×256 のテンプレート)。
2. 目的のウィジェットサイズで `compose_nine_slice` を実行して RGBA8 を得る。
3. ホスト (AC / Pictor) がその RGBA8 を texture としてアップロード。

## テスト

`tests/ui/test_nine_slice.cpp` (9 cases)

- `can_fit_corners` がターゲットサイズを正しく判定する
- identity round-trip: `target == src` のとき byte-for-byte で一致する
- 4 隅ピクセルが任意ターゲットサイズでもソースと同値を保つ
- 上辺 / 中央帯がそれぞれのソース領域のみから samplingされる
- 0 insets は単なる nearest-neighbor 拡縮に縮退する
- insets ≥ source / target < corners の不正入力を弾く
- Tile mode は今は未実装でエラーを返す (将来拡張点)
- 「corner 合計ちょうど」の target で中央帯が 0 に縮退しても valid

`tests/ui/test_svg_raster.cpp` (4 cases)

- in-memory SVG が要求した寸法で返る
- 出力が全黒ではない (内容が描画されている)
- 空入力時はエラー報告 or 空 bitmap で落ちない
- 非正方形 target でアスペクト比が保存される

`tests/ui/test_gradient.cpp` (6 cases)

- linear L→R で左右端が指定 stop に一致
- linear T→B で上下で輝度順が正しい
- radial で中心が角より明るい
- stop が 1 個 / 寸法 0 の不正入力を弾く
- 無順序の stop も自動ソートされる

`tests/ui/test_line_glow.cpp` (7 cases)

- Perimeter path, time=0 で top-left が点灯し、遠い中央は暗いまま
- 時間経過で head 位置が動く (a != b)
- period の整数倍で元の絵に戻る (ループ)
- Horizontal path は top/bottom 行のみ影響
- thickness を上げると beam が内側に広がる
- 無効 Bitmap を渡すと invalid を返す
- in-place 版が dst を書き換える

## 追加予定のタスク (本モジュールの範囲外)

- **Tile fill mode** — `SliceFillMode::Tile` の実装。今は `Stretch`
  のみ。装飾が繰り返し模様のときに欲しくなる。
- **バイリニア sampling** — 現状 nearest-neighbor。拡大率が高いときに
  ぼかしたいならバイリニアが要る。
- **バウンディングボックスクロップ** — SVG の実描画領域で自動的に
  余白を取り除くオプション。
- **GPU パス** — 9-slice を実行時にシェーダで行う API (エッジ帯の
  UV を push_constant に押し込む)。Pictor 統合が入ったあたりで検討。
