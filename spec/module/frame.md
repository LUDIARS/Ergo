# Frame モジュール定義

## 概要

アプリ起動時からの **累積フレーム数** と **現在のフレームレート (rolling
average FPS)** を管理するモジュール。ホストアプリは毎フレーム 1 回
`ergo::frame::tick()` を呼び出し、任意のタイミングで `count()` / `fps()`
を照会できる。`format_hud()` で 1 行の HUD 文字列を得られ、シーン描画の
デバッグ表示 (画面上/コンソール) に利用できる。

`ergo_log` はこのモジュールの `count()` を既定のフレーム番号ソースとして
参照する (疎結合 — `set_frame_provider` で差し替え可)。

## カテゴリ
システム

## 所属ドメイン
デバッグ / 計測 / 開発者ツール

## 必要なデータ
- フレーム開始時刻 (`steady_clock`)
- 直近 N フレーム分の dt (デフォルト 60 フレームの rolling window)

## 依存
- C++17 標準ライブラリ (`<chrono>`, `<deque>`)
- (テスト) GoogleTest 互換 mini-gtest

## 変数
- `count_` : 起動後の累計フレーム数 (uint64_t)
- `last_tick_` : 直近 `tick()` の時刻
- `last_dt_` : 直近フレームの経過時間 (秒)
- `dt_window_` : FPS 計算用 dt キュー
- `window_size_` : FPS 計算ウィンドウ長 (既定 60)

## 作業

### 入力
- ホストからの `tick()` 呼び出し (毎フレーム)
- `set_window_size(n)` による FPS ウィンドウ長変更
- `reset()` による再初期化

### 出力
- `count()` : 累計フレーム数
- `fps()` : 直近ウィンドウの平均 FPS
- `dt_seconds()` : 直近フレームの dt (秒)
- `format_hud()` : `"F12345 FPS=60.0 dt=16.67ms"` 形式の 1 行文字列

### タスク
- 毎フレーム `tick()` で `count_++` と dt サンプリング
- ウィンドウが満タンなら先頭を pop、末尾に push
- FPS = window_count / window_sum_dt
- シングルスレッド前提 (ホストのメインスレッドから呼ばれる想定)
- ダミープラグ (no-op Counter) をリンクのみ用に提供

## テスト
- 初期状態 (`count()==0`, `fps()==0`)
- `tick()` で `count()` が 1 ずつ増える
- `reset()` で 0 に戻る
- FPS が一定 dt で正しく収束する
- ウィンドウ変更が反映される
- `format_hud()` が非空文字列を返す
