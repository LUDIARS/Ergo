# Perf モジュール定義

## 概要

`ergo_perf` はゲームの**負荷を数え、PC の性能に合わせて重い処理を軽くする**ための
計測ライブラリ。 3 つの部品からなる。

1. **フレームの統計** — CPU のフレーム時間と、名前の付いた段 (GPU のパス、CPU の区間)
   の時間を窓に溜め、移動平均・p50 / p95 / p99・最悪値を出す。
2. **予算と自動調整 (governor)** — 目標フレーム時間に対し、段ごとの品質レベル
   (0 = 最高 .. n = 最軽) をヒステリシス付きで上げ下げする。下げる順番と下限は
   呼び出し側が宣言する。
3. **PC スペックの初期推定** — GPU の名前・VRAM・解像度 (と任意の起動時の計測) から、
   最初の品質の段を決める。

グラフィクス API には依存しない。 GPU の時間は呼び出し側が測って (Vulkan のタイム
スタンプ query の読み出しなど) 段の名前と ms で渡す。 ゲーム名も知らない。

Chrome Trace への GPU 段の書き出しは `ergo_profile` の lane
(`register_lane` / `record_complete_on`) で行う。 モジュール同士は import しないので、
呼び出し側が両方へ同じ値を渡す (下の「ergo_profile との併用」)。

## カテゴリ
システム

## 所属ドメイン
observability (デバッグ / 計測)

## 必要なデータ
- 窓 (固定長のリングバッファ) に溜めたサンプル (ms)
- 段の名前と id (最初に名前が出た順)
- 段の宣言 `{ name, lightest }` と、そこから作る段の並び (ladder)
- governor の設定 (目標 ms・上げ下げの線・判定に使うパーセンタイル・窓・保持フレーム数)
- ハードウェアの記述 (GPU 名、VRAM、dGPU/iGPU、解像度、起動時の計測)

## 依存
- C++17 標準ライブラリのみ
- 他の ergo モジュールには依存しない
- (テスト) mini-gtest

## API

### `RollingSeries` (`rolling_series.h`)
- `add(ms)` — 有限でない値・負の値は捨てる (測れなかった段)
- `mean()` (移動平均) / `percentile(p)` (nearest-rank、p は 0..1) / `worst()` / `last()` / `summary()`

### `FrameStats` (`frame_stats.h`)
- `add_frame(cpu_ms)` — プレイヤーが見るフレーム時間
- `stage(name) -> id`、 `add_stage(id or name, ms)` — 段の時間
- `stage_mean_total()` — 段の平均の合計 (段が GPU のパスなら GPU のフレーム時間)
- `perf_report.h` の `format_frame_report()` で 1 行のログにする

### `QualityLadder` (`quality_ladder.h`)
- 段の宣言 `StageDecl{name, lightest}`。 `lightest` はその段が到達してよい最も軽いレベル。
  **消してはならない段は、まだ描くレベルを `lightest` にする** (「この段は消さない、下限はレベル k」)。
- 既定の並び: 宣言順に各段を 1 つずつ軽くし、それを次のレベルで繰り返す
  (先に宣言した段から品質を譲る)。
- `from_steps(stages, steps)` — 呼び出し側が並びを書く。 次のレベル以外への飛び越し・
  `lightest` 超え・未知の段は拒否 (空を返す)。
- `levels_at(step)` — 各段のレベル (step は 0 .. `step_count()`)。

### `QualityGovernor` (`quality_governor.h`)
- `observe(cost_ms)` — 毎フレーム呼ぶ。 **cost は GPU の時間と CPU の処理時間の大きい方**
  を渡す (vsync やペーシングの sleep で待った時間を含めると余裕が見えなくなる)。
- 窓の `percentile` (既定 p90) で判定する。 上位 10% は瞬間の遅延として数えない。
- 目標 × `degrade_above` を `degrade_hold` フレーム続けて超えたら 1 段軽く、
  目標 × `upgrade_below` を `upgrade_hold` フレーム続けて下回ったら 1 段重く。
- 段を変えたら `settle` フレーム捨て、窓を溜め直してから判定する (1 判定 1 段)。
- 重くしてすぐ戻された場合、次に重くするまでの保持を 2 倍にする (`max_upgrade_hold` まで)。
- `set_step(s)` — 推定から始める (以後も調整する)。 `pin(s)` / `unpin()` — 固定。
- `level(name)` — 現在の段のレベル。

### `estimate_initial_step` (`spec_estimate.h`)
- 全品質のコスト予測 = `reference_cost_ms × (fixed + (1 - fixed) × 画素比) / power`
- `power` は `GpuClass{name_contains, relative_power}` の最初の一致 (大文字小文字を無視)。
  一致しなければ dGPU / iGPU の既定値。 VRAM が `low_vram_mb` 未満なら `low_vram_scale` を掛ける。
- 起動時の計測 (`measured_cost_ms`、 `measured_at_step`) があれば名前より優先する。
- 1 段ごとに `step_saving` の割合が減ると見て、予算に収まる最初の段を返す。
  governor が実フレームで補正する前提の、控えめな初期値。

## ergo_profile との併用

```cpp
const uint64_t gpu_lane = ergo::profile::register_lane("GPU");
// フレームごと: GPU の段を読み出せたら
int64_t t = frame_begin_us;  // ergo::profile::now_us() で取った、そのフレームの CPU 開始
for (stage : stages) {
    stats.add_stage(stage.id, stage.ms);
    ergo::profile::record_complete_on(gpu_lane, stage.name, t, us(stage.ms));
    t += us(stage.ms);
}
```

GPU の時計は CPU の時計と別なので、 段は CPU の開始から並べて置く (長さは正確、 位置は目安)。

## 変数
- `RollingSeries::ring_` / `next_` / `count_`
- `FrameStats::names_` / `stages_`
- `QualityGovernor::step_` / `pinned_` / `over_` / `under_` / `settle_left_` / `upgrade_hold_`

## 作業

### 入力
- 毎フレームの CPU フレーム時間・段の時間・コスト
- 起動時のハードウェアの記述

### 出力
- 統計 (平均・パーセンタイル・最悪値) と 1 行のログ
- 現在の段と各段の品質レベル、 段の変化 (`lighter` / `heavier`)
- 初期の段の推定

## テスト
- `tests/perf/test_frame_stats.cpp` — パーセンタイル (nearest-rank)・移動平均・窓の入れ替わり・
  測れなかった値の無視・段の id・ログの書式
- `tests/perf/test_quality_governor.cpp` — 段の並び (既定・手書き・拒否)、 予算内で動かない、
  負荷が続くと下限まで軽くなり下限を超えない、 瞬間の遅延で揺れない、 1 判定 1 段、
  余裕で戻り、 戻してすぐ下げたら保持が倍、 固定、 測れなかったフレームの無視
- `tests/perf/test_spec_estimate.cpp` — 基準 GPU・解像度・速い / 遅い GPU・iGPU・VRAM・計測の優先
- `tests/profile/test_profile.cpp` — lane のイベントが lane の tid と名前で Chrome Trace に出る
