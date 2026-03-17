# Ergo Input System 設計書

## 1. 概要

Ergo Input System は、複数種類の入力デバイスを統一的なインターフェースで管理するC++17モジュールである。
スレッドセーフなロックフリー状態管理、柔軟なイベント配信ポリシー、複数デバイスインスタンスのサポートを主な特徴とする。

## 2. 設計目標

| 目標 | 説明 |
|------|------|
| 統一インターフェース | すべてのデバイスを `IInputDevice` 抽象クラスで統一 |
| スレッドセーフ | ダブルバッファリングによるロックフリーな状態同期 |
| 拡張性 | 新しいデバイスタイプの追加が容易 |
| テスト容易性 | テスト用の状態注入メソッドを各デバイスに提供 |
| フレーム同期 | ゲームループとの統合を前提とした設計 |

## 3. アーキテクチャ

### 3.1 モジュール構成図

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

### 3.2 ディレクトリ構成

```
include/ergo/input/    ── 公開ヘッダ
  ├── types.h              型定義・列挙型
  ├── input_device.h       IInputDevice 抽象基底クラス
  ├── double_buffer.h      ロックフリーダブルバッファ
  ├── input_buffer.h       イベント履歴リングバッファ
  ├── observer.h           イベント購読・通知
  ├── polling_thread.h     バックグラウンドポーリング
  ├── mouse_device.h       マウスデバイス
  ├── keyboard_device.h    キーボードデバイス
  ├── gamepad_device.h     ゲームパッドデバイス
  ├── usb_device.h         汎用USBデバイス
  └── input_system.h       システムファサード

src/input/             ── 実装ファイル
tests/input/           ── テストスイート (GoogleTest)
docs/design/           ── 設計ドキュメント
```

## 4. コアコンポーネント詳細

### 4.1 IInputDevice (抽象基底クラス)

すべてのデバイスが実装する純粋仮想インターフェース。

```cpp
class IInputDevice {
public:
    virtual bool initialize() = 0;     // デバイス初期化
    virtual void shutdown() = 0;        // デバイス終了処理
    virtual void poll() = 0;            // 入力取得
    virtual void swapBuffers() = 0;     // 状態バッファ交換
    virtual bool isConnected(DeviceIndex) const = 0;
    virtual DeviceType deviceType() const = 0;
};
```

### 4.2 DoubleBuffer\<T\>

ロックフリーのダブルバッファリングテンプレート。ポーリングスレッドとメインスレッド間の状態同期に使用する。

**動作原理:**
1. `write()` で書き込みバッファに状態を書き込む
2. `swap()` でアトミックにバッファを切り替え、状態をコピー
3. `read()` で読み取り側バッファから安全に状態を取得

**メモリオーダリング:**
- 書き込み: `memory_order_relaxed`（高速化）
- スワップ: `memory_order_release` / `memory_order_acquire`（同期保証）

### 4.3 InputBuffer (イベント履歴)

リングバッファによるイベント履歴管理。ミューテックスによるスレッドセーフ保証。

**主要機能:**
- イベント蓄積（シーケンス番号自動付与）
- キー押下時間計測 (`holdDuration`)
- コンボ検出 (`matchSequence`) — 時間窓内のキーシーケンス一致判定

### 4.4 Observer (イベント通知)

Observer パターンによるイベント配信システム。

**配信ポリシー:**
| ポリシー | 動作 |
|---------|------|
| `Immediate` | `notify()` 呼び出し時に即座にコールバック実行 |
| `FrameSync` | イベントをキューに蓄積し、`flush()` で一括配信 |

**フィルタリング:** `EventFilter` により `deviceIndex` と `keyCode` で購読対象を絞り込み可能。

### 4.5 PollingThread

バックグラウンドスレッドでデバイスポーリングを実行するコンポーネント。

- `condition_variable` によるマイクロ秒精度のインターバル制御
- アトミックフラグによるスレッドセーフな起動・停止

## 5. デバイス仕様

### 5.1 MouseDevice

| 項目 | 値 |
|------|-----|
| 最大インスタンス数 | 4 |
| ボタン | Left, Right, Middle, Button4, Button5 |
| 状態 | ボタンビットセット, 座標, 移動デルタ, スクロール |
| 特殊機能 | カーソルロック |

### 5.2 KeyboardDevice

| 項目 | 値 |
|------|-----|
| 最大インスタンス数 | 4 |
| キーコード | 512 (USB HID 標準) |
| 修飾キー | Shift, Ctrl, Alt, Super (ビットフラグ) |
| テキスト入力 | `std::u32string` (Unicode) |
| 特殊機能 | キー押下時間計測 |

### 5.3 GamepadDevice

| 項目 | 値 |
|------|-----|
| 最大インスタンス数 | 8 |
| ボタン | 15 (A/B/X/Y, バンパー, トリガー, スティック, D-Pad 等) |
| アナログ軸 | 6 (左右スティック×2軸 + 左右トリガー) |
| 特殊機能 | デッドゾーン設定, バイブレーション, バッテリー残量 |

### 5.4 UsbDevice

| 項目 | 値 |
|------|-----|
| 最大インスタンス数 | 8 |
| データ形式 | 生HIDレポート |
| 軸/ボタン | 任意数 (スパースマップ) |
| 特殊機能 | HIDデスクリプタ, ベンダー/プロダクトID |

## 6. InputSystem (ファサード)

全デバイスを統括するオーケストレーター。

### 6.1 ライフサイクル

```
initialize(config) → beginFrame() → [入力処理] → endFrame() → ... → shutdown()
```

### 6.2 InputConfig

```cpp
struct InputConfig {
    ThreadMode threadMode;        // Independent | MainSync
    uint32_t pollingIntervalUs;   // ポーリング間隔 (マイクロ秒)
    uint32_t bufferCapacity;      // イベントバッファ容量
    bool enableMouse;
    bool enableKeyboard;
    bool enableGamepad;
    bool enableUsb;
};
```

### 6.3 スレッドモード

| モード | 説明 |
|--------|------|
| `MainSync` | メインスレッドの `beginFrame()` でポーリング |
| `Independent` | `PollingThread` によるバックグラウンドポーリング |

## 7. イベントシステム

### 7.1 InputEvent 構造体

```cpp
struct InputEvent {
    DeviceType deviceType;     // デバイス種別
    DeviceIndex deviceIndex;   // デバイスインスタンス番号
    EventType eventType;       // Press/Release/Move/Scroll/Axis/Text/Connect/Disconnect
    uint32_t code;             // キーコード / ボタンコード
    float value;               // アナログ値
    uint64_t sequence;         // シーケンス番号
    TimePoint timestamp;       // タイムスタンプ
};
```

### 7.2 イベントフロー

```
デバイス → poll() → InputBuffer → Observer → コールバック
                         ↓
                   DoubleBuffer.swap()
                         ↓
                  メインスレッドから読み取り
```

## 8. 設計パターン

| パターン | 適用箇所 |
|----------|----------|
| Double Buffering | 状態同期 (`DoubleBuffer<T>`) |
| Ring Buffer | イベント履歴 (`InputBuffer`) |
| Observer | イベント配信 (`Observer`) |
| Strategy | 配信ポリシー (Immediate / FrameSync) |
| Template Method | `IInputDevice` ライフサイクル |
| Facade | `InputSystem` |

## 9. ビルド構成

- **C++ 標準:** C++17
- **ライブラリ:** `ergo_input` (静的ライブラリ)
- **テストフレームワーク:** GoogleTest
- **必須依存:** pthread (std::thread サポート)
- **テスト実行ファイル:** 9 個 (各コンポーネント + 統合テスト)

## 10. テストカバレッジ

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
