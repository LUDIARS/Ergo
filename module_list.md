# Ergo モジュール一覧

> 最終更新: 2026-03-17

## モジュールサポートリスト

| モジュール名 | 概要 | 対応 OS | プラットフォーム | 最終更新日 | 依存モジュール | ブランチ |
|-------------|------|---------|-----------------|-----------|---------------|---------|
| `ergo_input` | 複数入力デバイス（マウス・キーボード・ゲームパッド・USB HID）を統一インターフェースで管理する入力システム | Windows / Linux / macOS | x86_64 / ARM64 | 2026-03-17 | pthread | `module/input` |

## モジュール詳細

### ergo_input

- **名前空間:** `ergo::input`
- **C++ 標準:** C++17
- **ライブラリ:** 静的ライブラリ
- **主要クラス:**
  - `InputSystem` — 全デバイス統括ファサード
  - `MouseDevice` — マウス入力（最大4台）
  - `KeyboardDevice` — キーボード入力（最大4台）
  - `GamepadDevice` — ゲームパッド入力（最大8台）
  - `UsbDevice` — 汎用USB HID入力（最大8台）
  - `DoubleBuffer<T>` — ロックフリー状態同期
  - `InputBuffer` — イベント履歴リングバッファ
  - `Observer` — イベント通知システム
  - `PollingThread` — バックグラウンドポーリング
- **設計書:** [doc/modules/input/README.md](doc/modules/input/README.md)
