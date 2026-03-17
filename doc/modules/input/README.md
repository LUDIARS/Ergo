# Ergo Input System モジュール設計書

## 1. 概要

Ergo Input System は、複数種類の入力デバイス（マウス・キーボード・ゲームパッド・USB HID）を統一インターフェースで管理する C++17 モジュールである。
スレッドセーフなロックフリー状態管理、柔軟なイベント配信ポリシー、複数デバイスインスタンスのサポートを主な特徴とする。

- **モジュール名:** `ergo_input`
- **名前空間:** `ergo::input`
- **C++ 標準:** C++17
- **ライブラリ形式:** 静的ライブラリ (`ergo_input`)
- **対応 OS:** Windows / Linux / macOS（プラットフォーム抽象化層）
- **依存:** pthread (std::thread)
- **ブランチ:** `module/input`

## 2. 設計目標

| 目標 | 説明 |
|------|------|
| 統一インターフェース | すべてのデバイスを `IInputDevice` 抽象クラスで統一 |
| スレッドセーフ | ダブルバッファリングによるロックフリーな状態同期 |
| 拡張性 | 新しいデバイスタイプの追加が容易 |
| テスト容易性 | テスト用の状態注入メソッドを各デバイスに提供 |
| フレーム同期 | ゲームループとの統合を前提とした設計 |

## 3. アーキテクチャ

```
┌─────────────────────────────────────────────────┐
│                  InputSystem                     │
│              (オーケストレーター)                  │
├─────────┬──────────┬────────────┬───────────────┤
│  Mouse  │ Keyboard │  Gamepad   │  UsbDevice    │
│ Device  │  Device  │  Device    │               │
├─────────┴──────────┴────────────┴───────────────┤
│              IInputDevice (抽象基底)              │
├─────────────────────────────────────────────────┤
│  DoubleBuffer │ InputBuffer │ Observer │ Polling │
│  (状態同期)    │ (履歴管理)   │ (通知)   │ Thread  │
└─────────────────────────────────────────────────┘
```

## 4. クラス定義

### 4.1 IInputDevice（抽象基底クラス）

**ファイル:** `include/ergo/input/input_device.h`

すべてのデバイスが実装する純粋仮想インターフェース。

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `initialize()` | なし | `void` | デバイス初期化 |
| `shutdown()` | なし | `void` | デバイス終了処理 |
| `poll()` | なし | `void` | 入力状態取得 |
| `swapBuffers()` | なし | `void` | ダブルバッファ切り替え |
| `isConnected(DeviceIndex)` | `DeviceIndex` | `bool` | 接続状態確認 |
| `deviceType()` | なし | `DeviceType` | デバイス種別取得 |

---

### 4.2 DoubleBuffer\<T\>（テンプレートクラス）

**ファイル:** `include/ergo/input/double_buffer.h`

ロックフリーのダブルバッファリングテンプレート。ポーリングスレッドとメインスレッド間の状態同期に使用。

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `write(const T&)` | `T` | `void` | 書き込みバッファに状態を書き込む |
| `writableRef()` | なし | `T&` | 書き込みバッファへの参照取得 |
| `read()` | なし | `const T&` | 読み取りバッファから状態取得 |
| `swap()` | なし | `void` | バッファをアトミックに切り替え |

**メモリオーダリング:**
- 書き込み: `memory_order_relaxed`
- スワップ: `memory_order_release` / `memory_order_acquire`

---

### 4.3 InputBuffer（イベント履歴リングバッファ）

**ファイル:** `include/ergo/input/input_buffer.h`

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `push(const InputEvent&)` | `InputEvent` | `void` | イベント追加 |
| `clear()` | なし | `void` | バッファクリア |
| `size()` | なし | `uint32_t` | 格納イベント数 |
| `capacity()` | なし | `uint32_t` | バッファ容量 |
| `at(uint32_t)` | `uint32_t index` | `const TimedInputEntry&` | インデックスアクセス |
| `holdDuration(DeviceType, uint16_t)` | `DeviceType`, `uint16_t code` | `Duration` | キー押下時間取得 |
| `matchSequence(vector<uint16_t>, Duration)` | `codes`, `window` | `bool` | コンボ検出 |
| `nextSequence()` | なし | `uint64_t` | シーケンス番号発行 |

---

### 4.4 Observer（イベント通知）

**ファイル:** `include/ergo/input/observer.h`

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `subscribe(EventCallback, EventFilter)` | `callback`, `filter` | `SubscriptionHandle` | イベント購読 |
| `unsubscribe(SubscriptionHandle)` | `handle` | `void` | 購読解除 |
| `notify(const InputEvent&)` | `InputEvent` | `void` | イベント通知 |
| `flush()` | なし | `void` | キュー一括配信（FrameSync用） |

**配信ポリシー:**

| ポリシー | 動作 |
|---------|------|
| `Immediate` | `notify()` 呼び出し時に即座にコールバック実行 |
| `FrameSync` | イベントをキューに蓄積し `flush()` で一括配信 |

---

### 4.5 PollingThread

**ファイル:** `include/ergo/input/polling_thread.h`

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `start(IInputDevice*, uint64_t)` | `device`, `intervalUs` | `void` | ポーリング開始 |
| `stop()` | なし | `void` | ポーリング停止 |
| `isRunning()` | なし | `bool` | 実行状態確認 |
| `setInterval(uint64_t)` | `intervalUs` | `void` | インターバル変更 |

---

### 4.6 MouseDevice

**ファイル:** `include/ergo/input/mouse_device.h`
**最大インスタンス数:** 4

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `isButtonDown(MouseButton, DeviceIndex)` | button, index | `bool` | ボタン押下中判定 |
| `isButtonPressed(MouseButton, DeviceIndex)` | button, index | `bool` | ボタン押下開始判定 |
| `isButtonReleased(MouseButton, DeviceIndex)` | button, index | `bool` | ボタン離し判定 |
| `cursorPosition(DeviceIndex)` | index | `Vec2f` | カーソル座標取得 |
| `moveDelta(DeviceIndex)` | index | `Vec2f` | 移動量取得 |
| `scrollDelta(DeviceIndex)` | index | `Vec2f` | スクロール量取得 |
| `setCursorLock(bool, DeviceIndex)` | locked, index | `void` | カーソルロック設定 |
| `isCursorLocked(DeviceIndex)` | index | `bool` | ロック状態確認 |

**ボタン一覧:** Left, Right, Middle, Button4, Button5

---

### 4.7 KeyboardDevice

**ファイル:** `include/ergo/input/keyboard_device.h`
**最大インスタンス数:** 4

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `isKeyDown(KeyCode, DeviceIndex)` | key, index | `bool` | キー押下中判定 |
| `isKeyPressed(KeyCode, DeviceIndex)` | key, index | `bool` | キー押下開始判定 |
| `isKeyReleased(KeyCode, DeviceIndex)` | key, index | `bool` | キー離し判定 |
| `modifiers(DeviceIndex)` | index | `ModifierFlags` | 修飾キー状態取得 |
| `textInput(DeviceIndex)` | index | `const u32string&` | テキスト入力取得 |
| `clearTextInput(DeviceIndex)` | index | `void` | テキストバッファクリア |
| `keyHoldDuration(KeyCode, DeviceIndex)` | key, index | `Duration` | キー押下時間取得 |

**修飾キー:** Shift, Ctrl, Alt, Super（ビットフラグ）
**キーコード:** USB HID 標準準拠（最大512）

---

### 4.8 GamepadDevice

**ファイル:** `include/ergo/input/gamepad_device.h`
**最大インスタンス数:** 8

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `isButtonDown(GamepadButton, DeviceIndex)` | button, index | `bool` | ボタン押下中判定 |
| `isButtonPressed(GamepadButton, DeviceIndex)` | button, index | `bool` | ボタン押下開始判定 |
| `isButtonReleased(GamepadButton, DeviceIndex)` | button, index | `bool` | ボタン離し判定 |
| `axisValue(uint8_t, DeviceIndex)` | axis, index | `float` | 軸の値取得 |
| `leftStick(DeviceIndex)` | index | `Vec2f` | 左スティック値取得 |
| `rightStick(DeviceIndex)` | index | `Vec2f` | 右スティック値取得 |
| `leftTrigger(DeviceIndex)` | index | `float` | 左トリガー値取得 |
| `rightTrigger(DeviceIndex)` | index | `float` | 右トリガー値取得 |
| `setDeadZone(float, DeviceIndex)` | deadZone, index | `void` | デッドゾーン設定 |
| `deadZone(DeviceIndex)` | index | `float` | デッドゾーン取得 |
| `setVibration(VibrationParams, DeviceIndex)` | params, index | `void` | バイブレーション設定 |
| `batteryLevel(DeviceIndex)` | index | `float` | バッテリー残量取得 |
| `isWireless(DeviceIndex)` | index | `bool` | ワイヤレス判定 |

**ボタン:** A, B, X, Y, LeftBumper, RightBumper, Back, Start, Guide, LeftThumb, RightThumb, DpadUp/Right/Down/Left
**アナログ軸:** 6（左右スティック×2軸 + 左右トリガー）

---

### 4.9 UsbDevice

**ファイル:** `include/ergo/input/usb_device.h`
**最大インスタンス数:** 8

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `rawReport(DeviceIndex)` | index | `const vector<uint8_t>&` | 生HIDレポート取得 |
| `axisValue(uint32_t, DeviceIndex)` | axis, index | `float` | 軸の値取得 |
| `buttonState(uint32_t, DeviceIndex)` | button, index | `bool` | ボタン状態取得 |
| `descriptor(DeviceIndex)` | index | `const HidDescriptor&` | HIDデスクリプタ取得 |

**HidDescriptor:**
- `vendorId` / `productId` — デバイス識別
- `axisCount` / `buttonCount` — 軸・ボタン数

---

### 4.10 InputSystem（ファサード）

**ファイル:** `include/ergo/input/input_system.h`

| メソッド | 入力 | 出力 | 説明 |
|----------|------|------|------|
| `initialize(const InputConfig&)` | config | `void` | 全デバイス初期化 |
| `shutdown()` | なし | `void` | 全デバイス終了 |
| `beginFrame()` | なし | `void` | フレーム開始処理（poll + swap） |
| `endFrame()` | なし | `void` | フレーム終了処理 |
| `mouse()` | なし | `MouseDevice*` | マウスデバイス取得 |
| `keyboard()` | なし | `KeyboardDevice*` | キーボードデバイス取得 |
| `gamepad()` | なし | `GamepadDevice*` | ゲームパッドデバイス取得 |
| `usb()` | なし | `UsbDevice*` | USBデバイス取得 |
| `onDeviceConnection(callback)` | `DeviceConnectionCallback` | `SubscriptionHandle` | 接続コールバック登録 |
| `removeDeviceConnectionCallback(handle)` | `SubscriptionHandle` | `void` | コールバック解除 |

**ライフサイクル:** `initialize(config) → beginFrame() → [入力処理] → endFrame() → ... → shutdown()`

## 5. 型定義・列挙型

**ファイル:** `include/ergo/input/types.h`

### 列挙型一覧

| 列挙型 | 値 |
|--------|-----|
| `DeviceType` | Mouse, Keyboard, Gamepad, UsbGeneric |
| `ThreadMode` | Independent, MainSync |
| `DeliveryPolicy` | Immediate, FrameSync |
| `EventType` | Press, Release, Repeat, Move, Scroll, Axis, Text, Connect, Disconnect |
| `MouseButton` | Left, Right, Middle, Button4, Button5 |
| `KeyCode` | A-Z, Num0-9, Enter, Escape, F1-F12, Arrow keys, 修飾キー (最大512) |
| `ModifierFlags` | None, Shift, Ctrl, Alt, Super (ビットフラグ) |
| `GamepadButton` | A, B, X, Y, LB, RB, Back, Start, Guide, LT, RT, DPad×4 |

### 構造体一覧

| 構造体 | 説明 |
|--------|------|
| `Vec2f` | 2D float ベクトル (x, y) |
| `InputConfig` | システム設定 (スレッドモード, ポーリング間隔, バッファ容量, デバイス有効化) |
| `InputEvent` | 入力イベント (デバイス種別, イベント種別, コード, 値, シーケンス, タイムスタンプ) |
| `TimedInputEntry` | イベント + 押下時間 |
| `VibrationParams` | バイブレーションパラメータ (低周波, 高周波) |
| `HidDescriptor` | USB HID デスクリプタ (ベンダーID, プロダクトID, 軸数, ボタン数) |
| `EventFilter` | イベントフィルタ (デバイスインデックス, キーコード) |

## 6. 設計パターン

| パターン | 適用箇所 |
|----------|----------|
| Double Buffering | 状態同期 (`DoubleBuffer<T>`) |
| Ring Buffer | イベント履歴 (`InputBuffer`) |
| Observer | イベント配信 (`Observer`) |
| Strategy | 配信ポリシー (Immediate / FrameSync) |
| Template Method | `IInputDevice` ライフサイクル |
| Facade | `InputSystem` |

## 7. テストカバレッジ

| テスト | 対象 |
|--------|------|
| `test_double_buffer` | ロックフリー並行状態管理 |
| `test_input_buffer` | イベント履歴, 押下時間, コンボ検出 |
| `test_observer` | 購読, フィルタリング, 配信ポリシー |
| `test_polling_thread` | バックグラウンドポーリング |
| `test_mouse_device` | ボタン状態, 座標, カーソルロック |
| `test_keyboard_device` | キー状態, 修飾キー, テキスト入力 |
| `test_gamepad_device` | ボタン/軸, デッドゾーン, バイブレーション |
| `test_usb_device` | HIDレポート, 任意軸/ボタン |
| `test_input_system` | デバイス初期化, フレーム同期, 接続コールバック |

## 8. ビルド

```bash
mkdir build && cd build
cmake .. -DERGO_INPUT_BUILD_TESTS=ON
cmake --build .
ctest
```

**CMake オプション:**
- `ERGO_INPUT_BUILD_TESTS` — テストビルド（デフォルト: ON）
- `ERGO_INPUT_BUILD_DUMMY` — ダミー実装ビルド（デフォルト: ON）
