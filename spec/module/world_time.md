# WorldTime モジュール定義

## 概要

ゲーム全体の **time scale** (ワールド時間倍率) を一元管理するモジュール。
代表的な用途:

- **ヒットストップ**: 被弾・打撃時に短時間ゲーム時間を完全停止し打撃感を演出
- **ヒットスロー**: フェードイン → 一定値ホールド → フェードアウトの 3 段階
  でゆるやかな時間減速演出

参考実装: VGA-Team2026/Foundation の `WorldTimeScale` 機能 (Unity)。
Ergo 版は Unity 依存を取り払った C++17 ポート。

## カテゴリ
システム (ゲームプレイ)

## 所属ドメイン
ゲームプレイ / 演出

## 必要なデータ

- 実時間 `real_dt` (秒)。ホストが毎フレーム供給
- HitStop パラメータ: `duration`
- HitSlow パラメータ: `duration / center_weight / center_time_scale /
  center_hold_time / ease`
- 通知先 `ITimeScaleTarget` の登録テーブル

## 依存

- C++17 標準ライブラリ
- (テスト) GoogleTest 互換 mini-gtest

## 変数

- 現在のフェーズ `Phase` (None / HitStopActive / HitSlowIn / HitSlowLoop / HitSlowOut)
- 現在の `time_scale` (0.0..1.0)
- HitStop 残り時間
- HitSlow 全体パラメータ + 経過時間 (`duration / hold_time / ease / elapsed`)
- 登録された `ITimeScaleTarget` の vector

## 設計上の決定

- **単一アクティブ効果** (上書きモデル)。新規 `hit_stop` / `hit_slow` は
  既存の効果を打ち切って置き換える。多重要求のマージはしない (Foundation
  と同じ挙動)。
- **Push-pull 両対応**: `update(real_dt)` の戻り値で current_scale を
  受け取りつつ、登録済み `ITimeScaleTarget::on_time_scale_update(scale)`
  にも毎フレーム通知する
- **easing は内蔵の最小実装** (Linear / InQuad / OutQuad / InOutQuad /
  InCubic / OutCubic / InOutCubic)。DOTween 等の外部依存は持たない
- **Quantize はスコープ外**。Foundation 側は `Quantizer` 連携を持つが
  Ergo にはまだ Quantizer モジュールが無い (`ergo_sound` 内のクォンタイザ
  は別目的)。必要になったら別途連携を足す
- リリースビルドでも常時有効 (gameplay 機能のため compile-strippable に
  しない)

## 作業

### 入力 (Engine API)

- `Engine::instance()` — singleton accessor
- `hit_stop(duration_seconds)` — 即時停止を発動。既存効果を上書き
- `hit_slow(duration, center_weight, center_time_scale, center_hold_time,
  ease = InOutQuad)` — 3 段階スロー。既存効果を上書き
- `force_stop()` — 効果を即座に解除して time_scale を 1.0 に戻す
- `register_target(ITimeScaleTarget*)` — observer 登録
- `unregister_target(ITimeScaleTarget*)` — observer 解除
- `update(real_dt)` — 1 フレーム進めて effective dt を返す + observer 通知

### 出力

- `update()` 戻り値: `real_dt * current_time_scale`
- `current_time_scale()`、`current_phase()`、`is_hit_stop()`、`is_hit_slow()`、
  `target_count()` の状態クエリ
- 各 `ITimeScaleTarget::on_time_scale_update(time_scale)` を毎フレーム呼び出し

### タスク

- HitStop: `time_scale = 0`、残り時間を実時間で減らし、**残り時間が 0 を切った**
  フレームでリリース (= time_scale を 1 に戻し、そのフレームの戻り値は `real_dt`、
  observer 通知も 1.0)。Foundation 実装は「残り時間が 0 になった次のフレーム」で
  release だが、Ergo 版はゲームループが 1 フレーム余分に止まらない方が直感的と
  判断し前倒し
- HitSlow: `elapsed_time` を実時間で進め、3 フェーズ (In / Loop / Out) を計算
  - In:   `elapsed < transition` → `lerp(1.0, center, ease(elapsed / transition))`
  - Loop: `elapsed < transition + hold` → `center`
  - Out:  それ以後 → `lerp(center, 1.0, ease((elapsed - transition - hold) / transition))`
  - `transition = (duration - hold) / 2`
- 期限切れで Phase = None に戻し `time_scale = 1.0` を notify
- Observer 通知時に nullptr 化されたターゲットは自動除去 (Foundation 互換)
- `register_target` の重複登録は無視
- `force_stop` は内部状態を初期化し `time_scale = 1.0` を 1 度 notify

### プラットフォーム別タスク

- 全 OS 同等。プラットフォーム依存なし。

## 利用例

```cpp
class Camera : public ergo::world_time::ITimeScaleTarget {
public:
    void on_time_scale_update(float scale) override {
        // スケールに応じてカメラ揺れを抑制するなど
        shake_intensity_ = base_intensity_ * scale;
    }
};

// 初期化:
Camera cam;
ergo::world_time::Engine::instance().register_target(&cam);

// 被弾時:
ergo::world_time::Engine::instance().hit_stop(0.06f);   // 60ms 完全停止

// 必殺技開始:
ergo::world_time::Engine::instance().hit_slow(
    /*duration*/ 0.8f,
    /*center_weight*/ 0.0f,        // 互換引数 (現状未使用、Foundation 互換)
    /*center_time_scale*/ 0.2f,    // ホールド中の倍率
    /*center_hold_time*/ 0.2f,     // 中央のホールド時間
    ergo::world_time::Ease::InOutQuad);

// メインループ:
float real_dt = clock.tick();
float game_dt = ergo::world_time::Engine::instance().update(real_dt);
world.update(game_dt);
ui.update(real_dt);
```

# テスト

- 何も発動していなければ `update(dt)` は `dt` を返し phase は None / scale は 1.0
- `hit_stop(0.1)` 直後に `update(0.05)` → scale 0、戻り値 0、phase HitStopActive
- HitStop 残り時間が 0 を超えて経過すると phase が None に戻り scale が 1.0
- 新しい `hit_stop` は前の効果を即上書きする (時間が延長されるのではなく置換)
- `hit_slow` の各フェーズ境界 (In/Loop/Out) で正しい scale が出る
- HitSlow 中に `force_stop` で即 None / scale 1.0 に戻る
- 登録済み observer は毎 update で `on_time_scale_update` を受ける
- 同じ observer を 2 度 register しても 1 つしか登録されない
- nullptr 化された target は次の update で除去される
- `Linear` ease で In フェーズの scale が線形補間される (代表的サンプル点で検証)
