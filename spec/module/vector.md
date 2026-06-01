# ergo_vector — SVG / Lottie ベクター描画モジュール (設計書)

> 状態: 設計 (2026-06-01)。実装は Codex 委託予定。
> 区分: 新規 Ergo モジュール `ergo_vector`。
> 関連: [ui_layout.md](./ui_layout.md) (このモジュールの主要 consumer = vector ノード), KuzuSurvivors `spec/hud_svg.md` (最初の実利用), [render.md](./render.md), Pictor UIRenderer。

## 0. 背景と目的

KuzuSurvivors の HUD を「SVG + アニメーション」で作りたいが、LUDIARS のレンダ基盤には **SVG を直接描く経路が無い**。Pictor のベクター標準は Rive (.riv) だが、Rive は実行時の数値/テキスト data-binding が弱く、また素材を Rive エディタで .riv 化する人手工程が必要。

そこで **ThorVG** (SVG 1.1 Tiny + Lottie 対応のベクターエンジン) を vendoring し、「SVG/Lottie を CPU ラスタライズ → RGBA バッファ → GPU テクスチャ → 既存の合成経路 (UIRenderer / Pictor サンプラ) で描画」する汎用ベクターモジュールを新設する。

### スコープ
- **やる**: SVG / Lottie ファイルのロード、CPU ラスタライズ (ThorVG SwCanvas)、RGBA8 バッファ出力、アニメーション前進 (Lottie フレーム / 経過時間)、簡易シーングラフ操作 (named ノードの数値・色・可視・変形の差し替え = data-binding)、テクスチャ更新ヘルパ。
- **やらない (out of scope)**: GPU 直接ベクターラスタライズ (将来; 今回は SW→texture)、SVG オーサリング GUI、Rive 置換 (Rive は 3D ビルボード/敵で継続)。

## 1. ThorVG の vendoring 方針

ThorVG は **Meson 専用 (CMake 非対応)**。KS/Ergo は CMake+MSVC のため Meson 連携は避け、**ソースを vendoring して自前 CMake で静的ライブラリ化**する。

- 取得元: <https://github.com/thorvg/thorvg> (ライセンス MIT)。固定タグ/コミットを `third_party/thorvg/VERSION` に記録 (例 v1.0-rc 系の特定 commit、再現性のため必ず pin)。
- 配置: `third_party/thorvg/` に必要サブツリーのみ vendoring:
  - `inc/` (公開ヘッダ `thorvg.h`)
  - `src/common/`
  - `src/renderer/` (シーングラフ: Paint/Shape/Scene/Picture/Canvas/Animation/Fill/Text 等) + `src/renderer/cpu_engine/` (SwCanvas 一式)
  - `src/loaders/svg/`, `src/loaders/lottie/`, `src/loaders/raw/`
  - 任意: `src/loaders/png/`, `jpg/` は **不要なら除外** (外部 libpng/jpeg 依存を避ける。HUD 用途は SVG/Lottie で足りる)
  - 除外: `savers/`, `bindings/`, `test/`, `tools/`, `src/renderer/gpu_engine/` (gl/wg), `external_*` ローダ
- **`config.h` を自前生成**: ThorVG は meson の `configure_file` で `config.h` を作る。同等のものを CMake の `configure_file` (or 直書きヘッダ) で生成。最低限の define:
  ```
  #define THORVG_VERSION_STRING "x.y.z"
  #define THORVG_SW_RASTER_SUPPORT 1      // cpu_engine
  #define THORVG_SVG_LOADER_SUPPORT 1
  #define THORVG_LOTTIE_LOADER_SUPPORT 1
  #define THORVG_THREAD_SUPPORT 1         // TaskScheduler (要 std::thread)。問題が出たら 0 で単スレッド
  #define THORVG_FILE_IO_SUPPORT 1
  #define WIN32_LEAN_AND_MEAN 1
  // GL/WG/PNG/JPG/WEBP/GIF/SFNT/EXPRESSIONS 系は未定義 (=無効)
  ```
  ※ 実際に必要な define はソースの `#ifdef THORVG_*` を grep で確認して過不足なく設定 (config.h を include するのは ~14 ファイル)。
- CMake: `third_party/thorvg/CMakeLists.txt` を新規作成し `add_library(thorvg STATIC ...)`。ソースは glob でなく **明示列挙** (再現性)。`target_include_directories` で `inc/` と各 `src/*` を PUBLIC/PRIVATE 適切に。MSVC は `/utf-8` 付与 ([[feedback_msvc_utf8_test_targets]] と同様、ThorVG ソースに UTF-8 がある)。例外/RTTI は ThorVG 既定に合わせる。
- Ergo 本体 CMake から `add_subdirectory(third_party/thorvg)` し、`ergo_vector` が `thorvg` をリンク。`ERGO_BUILD_VECTOR` (既定 ON) でガード。
- **ビルド検証は Release 必須** ([[feedback_ks_release_build_required]]): KS の prebuilt が Release CRT 専用。Debug は通っても KS リンクで落ちるので、受け入れ確認は必ず Release。

## 2. モジュール構成 (Ergo 規約準拠)

| 項目 | 値 |
|---|---|
| name | `ergo_vector` |
| header_dir | `include/ergo/vector/` |
| source_dir | `src/vector/` |
| test_dir | `tests/vector/` |
| spec | `spec/module/vector.md` (本書) |
| cmake_option | `ERGO_BUILD_VECTOR` |
| tool_plugin | null |
| status | preview |

`module_list.yaml` + `module_list.md` に追記すること。

### 公開 API (include/ergo/vector/)

```cpp
namespace ergo::vector {

// ライブラリ初期化/終了 (ThorVG Initializer のラップ、参照カウント)
bool initialize(unsigned threads = 0);   // 0 = ハード並列数
void terminate();

struct RasterResult {
  std::vector<uint32_t> pixels;  // RGBA8 (premultiplied? ThorVG は ARGB32; 仕様に明記し UIRenderer 期待形式へ変換)
  int width = 0;
  int height = 0;
};

// 1枚のベクターアセット (SVG or Lottie)。スレッド非共有前提。
class VectorAsset {
public:
  static std::unique_ptr<VectorAsset> load_file(const std::string& path);
  static std::unique_ptr<VectorAsset> load_svg_data(std::string_view svg, const std::string& base_dir = "");
  static std::unique_ptr<VectorAsset> load_lottie_data(std::string_view json);

  // 出力解像度を指定 (SVG は viewBox→任意解像度にスケール)
  void set_size(int w, int h);
  std::pair<int,int> native_size() const;

  // --- アニメーション (Lottie / SVG SMIL は ThorVG 対応範囲) ---
  bool  animatable() const;
  float duration_sec() const;       // 0 = 静止
  void  seek(float t_sec);          // 絶対時刻
  void  advance(float dt_sec, bool loop = true);

  // --- data-binding: named ノードに実行時値を流し込む ---
  // ThorVG Accessor (tvg::Accessor) で id 一致の Paint を探して操作する薄いラッパ。
  // SVG の id="hp_fill" 等を対象にする。
  void set_opacity(std::string_view node_id, float a01);
  void set_fill_color(std::string_view node_id, uint8_t r, uint8_t g, uint8_t b);
  void set_visible(std::string_view node_id, bool v);
  void set_transform(std::string_view node_id, float tx, float ty, float sx, float sy, float rot_deg);
  // バー類は「クリップ矩形/スケールXで fill 量を表現」する規約 (§4) を使う:
  void set_scale_x(std::string_view node_id, float s01);

  // --- ラスタライズ ---
  // dirty (seek/advance/set_* があった) のときのみ再ラスタ。変化なければ前回結果を返す。
  const RasterResult& rasterize();
  bool dirty() const;
};

} // namespace ergo::vector
```

- 実装は ThorVG `SwCanvas` を内部に持ち、`Picture::load`、`Animation`(Lottie)、`Accessor` を使う。
- **テキスト/数値の扱い (重要・2026-06-01 訂正)**: Pictor には既に **`TextSvgRenderer`** (TrueType グリフ outline → SVG パス `d` 文字列化、解像度非依存) と `TextImageRenderer` (TT outline → 動的ラスタ) があり、TrueType を自前パースしている (FreeType 非依存)。KS HUD が今使っている `BitmapTextRenderer` は ASCII 固定のデバッグ用。
  - よって **HUD の文字もベクターで統一する**。ビットマップ併用は採らない。
  - 推奨: `TextSvgRenderer::outline_to_svg_path()` で動的文字列 (タイマー mm:ss / キル数 / Lv) を SVG パスに変換し、**HUD の SVG シーンに `<path>` ノードとして流し込む** (= 文字も図形も 1 枚の SVG に統合)。または `TextImageRenderer` で文字だけ別テクスチャにして ui_layout の text ノードで合成。どちらを採るかは実装時に Pictor API の実体を見て決定 (両者とも Pictor 既存)。
  - これにより任意フォント・任意サイズ・日本語可、解像度非依存になる。

### テクスチャ更新ヘルパ (Pictor 連携)
- `ergo_vector` は Pictor 非依存に保つ (RGBA バッファまでが責務)。
- Pictor テクスチャへの upload は consumer 側 (ui_layout / KS HUD layer) が、既存の `RiveEnemyLayer` の bake→texture 経路 (`src/render/layers/rive_enemy_layer.cpp` 参照) と同じ作法で行う。`ergo_vector` には `RasterResult` を返すところまでを実装し、Pictor 依存コードは置かない (レイヤ依存の単方向を守る)。

## 3. レンダリング/更新ポリシー
- **再ラスタは dirty 時のみ**。静止 SVG はロード時 1 回。Lottie/アニメは `advance` した frame でのみ。bar の値変化も dirty 扱い。
- HUD の更新頻度: 値が毎フレーム変わるバー (HP/XP/ゲージ) は「テクスチャ全再ラスタ」だと重いので、**バーは ui_layout 側の矩形プリミティブ + ベクターは枠/装飾のみ**を既定 (§4)。完全ベクターバーが欲しい場合は解像度を抑える/更新を間引く。
- 24fps 上限などの間引きは consumer 側で制御可能にする (advance の dt を渡す側が決める)。

## 4. SVG オーサリング規約 (consumer 向け、本モジュールが期待する形)
- ノードには `id` を付ける (例 `hp_fill`, `xp_fill`, `gauge_fill`, `frame`, `icon_slot_a`)。data-binding は id 一致で探す。
- **バー表現**: fill 要素を「左原点で scale_x 0..1」または「右側を覆うマスク矩形の幅」で表現する。`set_scale_x("hp_fill", ratio)` で伸縮。アンカーは左端。
- アニメーション: idle 演出 (パルス/グロー) は Lottie か SVG SMIL で持たせる。状態遷移 (被ダメ点滅、レベルアップ強調) は Lottie の marker かフレーム seek でトリガ。

## 5. テスト (tests/vector/)
- SVG ロード→native_size 取得。
- SVG→rasterize で期待解像度の非ゼロピクセルが出る (Vulkan 不要、純 CPU)。
- `set_scale_x` で fill 矩形のカバー面積が単調変化する (ピクセル数を数える)。
- Lottie ロード→duration>0、advance で dirty=true、同一 t で再 advance しても出力一致。
- 単スレッド/マルチスレッド両 config でビルド・実行可能。

## 6. 受け入れ基準 (acceptance)
1. `ERGO_BUILD_VECTOR=ON` で **Release** ビルドが通る (ThorVG vendoring 込み)。
2. tests/vector/ が ctest green (Vulkan 非依存)。
3. サンプル SVG (バー+枠+アイコン) と Lottie を読み、`rasterize()` が RGBA を返す。
4. `module_list.yaml`/`.md` 追記、`spec/module/vector.md` をソースに同期。
5. KS は本モジュールを consumer として利用できる (KS 側は別 PR / `spec/hud_svg.md`)。

## 7. 実装ステップ (prototyping-flow: 粗く動かす→分割→結合)
1. ThorVG vendoring + config.h + CMake 静的lib化 → **Release で thorvg 単体がコンパイル** (最大のリスク、最初に潰す)。
2. `ergo_vector` 最小: load_file(SVG) → set_size → rasterize → RGBA。test で非ゼロ確認。
3. data-binding (Accessor で set_scale_x / set_opacity / set_fill_color)。
4. Lottie ロード + advance/seek。
5. tests 整備、module_list 反映、Release 受け入れ。

## 委託メモ (Codex)
- cwd = `E:/Document/Ars/ergo`。ブランチは `feat/ergo-vector` を新規。Ergo は **全変更 feat ブランチ + PR** ([[feedback_ergo_branch_pr_required]])。
- 規約: SRP・ファイル分割 (`coding-conventions`)。God Class 禁止。レイヤ依存は単方向 (ergo_vector は Pictor 非依存)。
- 不明点 (config.h の過不足 define、premul ARGB の扱い、TaskScheduler のスレッド要否) は **着手前に grep で実体確認** してから決める。憶測でマクロを盛らない。
