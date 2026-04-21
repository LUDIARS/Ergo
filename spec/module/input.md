# Input モジュール定義

## 概要

マウス / キーボード / ゲームパッド / 汎用 USB HID を **同一 API** でアプリに
供給する統一入力レイヤ。デバイスタイプごとに個別クラスを持ちつつ、
`InputSystem` が寿命・ポーリング・イベント通知を取りまとめる。

デバイスは **テスト優先** の設計で、外部 OS API を踏まずに
`inject*()` API からイベントを注入できる。ポーリングは OS 依存の実装を
後から差し込む前提 (現状 `poll()` は空実装で inject 経由のみ動作)。

状態は **ダブルバッファ** で保持し、`beginFrame()` で `poll()` → `swap` を
走らせる。これによりフレーム内では読み出しが安定し、`isButtonPressed`
(立ち上がり) / `isButtonReleased` (立ち下がり) を正しく判定できる。

## カテゴリ

システム

## 所属ドメイン

入力 / プラットフォーム抽象

## 必要なデータ

- デバイスごとの現在状態 (ボタンマスク、軸、ポインタ位置、スクロール、HID report)
- 前フレームの状態 (edge 検出用)
- 接続状態 (`connected` フラグ)
- USB HID 用: 任意数の軸・ボタンマップ、生 report バイト列、HID descriptor
- フレーム共通イベントバッファ (`InputBuffer`) と Observer 購読者
- `InputConfig` (ポーリング間隔、スレッドモード、バッファ容量、各デバイス有効/無効)

## 依存

- C++17 標準ライブラリ (`<atomic>`, `<chrono>`, `<functional>`, `<memory>`, `<mutex>`, `<thread>`, `<unordered_map>`)
- Threads (std::thread — `PollingThread` 使用時のみ)
- (将来) OS ネイティブ入力 API (Windows Raw Input / evdev / HID API 等)
- (テスト) GoogleTest 互換 mini-gtest

## 変数

- `InputSystem::mouse_` / `keyboard_` / `gamepad_` / `usb_` — std::unique_ptr で所有
- `InputSystem::pollingThread_` — `ThreadMode::Independent` 時に起動
- 各デバイス内部:
  - `devices_[kMaxDevices]` 配列 (複数デバイス同時接続を想定; マウスは通常 1 だが拡張可)
  - `DoubleBuffer<State>` 現在/裏面バッファ
  - 前フレーム state (`prevState`) — edge 判定用
  - `InputBuffer inputBuffer_` / `Observer observer_`
- `connectionCallbacks_` — 接続/切断通知の購読リスト

## 作業

### 入力

- ホストからの `initialize(config)` / `shutdown()`
- 毎フレームの `beginFrame()` / `endFrame()`
- OS から届く低レベル入力 (将来 `poll()` 経由; 現状は inject のみ)
- テストから: `injectButtonState`, `injectPosition`, `injectScroll`,
  `injectAxis`, `injectRawReport`, `injectDescriptor`, `injectButton`
- 接続/切断通知: `notifyDeviceConnected` / `notifyDeviceDisconnected`
- 購読: `onDeviceConnection`, `observer().subscribe(...)`

### 出力

- 問い合わせ API:
  - Mouse: `isButtonDown/Pressed/Released`, `cursorPosition`, `moveDelta`, `scrollDelta`
  - Keyboard: 同上 + `KeyCode` ベース API, modifiers
  - Gamepad: ボタン判定 + `leftStick`/`rightStick`/`leftTrigger`/`rightTrigger`,
    `batteryLevel`, `isWireless`, `setVibration`
  - USB: `rawReport`, `axisValue(axis)`, `buttonState(button)`, `descriptor()`
- `InputEvent` 通知 (Press / Release / Axis / Move / Scroll / Connect / Disconnect)
- `Observer::notify` 経由の同期コールバック
- `InputBuffer` に積まれる時系列イベント列 (FrameSync / Immediate)

### タスク

- `initialize` 時に `InputConfig` に従って各デバイスを生成。`Independent` モード
  なら `PollingThread` を起動し、設定済みのデバイスを周期的に `poll`。
- `beginFrame` で各デバイスの `poll()` → `swapBuffers()` を順に実行。
- 各デバイスはボタンマスクの差分から edge イベントを生成し、`InputBuffer` に
  push + `Observer` に notify。
- 接続状態は `setConnected(bool)` で変化させ、`notifyDeviceConnected` /
  `notifyDeviceDisconnected` でホストに通知。
- `shutdown` ですべてのデバイスを解放し、`PollingThread` を停止する。

### プラットフォーム別タスク

- **Windows / Linux / macOS**: 現状 `poll()` は no-op。利用側が OS ネイティブ
  API を叩いて `inject*` 系で状態を供給する形で統合する想定 (Win32 Raw Input、
  evdev、IOKit など)。将来的にデバイスクラス内部へ取り込む。
- **WebGL**: 対象外 (ブラウザ側の DOM イベントを別レイヤで捌く)。

## テスト

- `DoubleBuffer` の書き込み/スワップ/読み出し不変条件
- `InputBuffer` 容量超過時の FIFO 挙動 (固定 capacity、古いイベント破棄)
- `Observer`: 購読・解除・多重通知
- `PollingThread`: start/stop 冪等性、interval 変更の反映
- 各デバイスで inject → state 更新 → edge (Pressed/Released) が正しく立つ
- Gamepad: デッドゾーン適用、`leftStick` / `rightStick` の軸マッピング
- USB: 任意軸・任意ボタン ID の永続化、HID descriptor 受け渡し
- `InputSystem`: `initialize`/`shutdown` サイクル、`onDeviceConnection` 通知
- `InputConfig` に応じて生成するデバイスが切り替わること
