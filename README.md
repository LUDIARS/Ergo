# Ergo Inspector

ホストアプリケーションの内部変数を**ブラウザから直接参照・更新**できる開発者ツール基盤。
変数を型消し API で登録するだけで、`http://localhost:17317/` からスライダー/トグル/数値入力で
ライブチューニングできる。リリースビルドではコンパイル時に完全切り離し可能。

## 概要

- **In-process WebSocket サーバ** (1スレッド)
- **JSON RPC** — `enumerate` / `get` / `set` / `subscribe` / `changed`
- **型消し変数レジストリ** — Bool / Int / Float / Double / String / Color / Vec3
- **コマンドキュー方式** — ネットスレッドからの set はキューに積み, ホストが自前タイミングで apply
- **ダミープラグ + 完全分離** — `ERGO_INSPECTOR_BUILD_SERVER=OFF` でネットコード不要、
  `ERGO_INSPECTOR_ENABLED` 未定義時は登録マクロが空展開

## 基本利用

```cpp
#include "ergo/inspector/inspector.h"

// 起動
ergo::inspector::Inspector::instance().start_server(17317);

// 変数登録 (release では空展開)
ERGO_INSPECT_VAR("player.bpm", bpm_value, ergo::inspector::VarMeta{
    .min = 20, .max = 300, .unit = "bpm"
});

// メインループ
while (running) {
    ergo::inspector::Inspector::instance().apply_pending_writes();
    // ... game logic ...
}
```

ブラウザで `http://localhost:17317/` を開くと UI が出る。

## ビルド

```bash
mkdir build && cd build
cmake .. -DERGO_INSPECTOR_BUILD_TESTS=ON
cmake --build .
ctest
```

オプション:
- `ERGO_INSPECTOR_BUILD_TESTS` (ON) — GoogleTest スイート
- `ERGO_INSPECTOR_BUILD_DUMMY` (ON) — シンボル満たすだけのダミーライブラリ
- `ERGO_INSPECTOR_BUILD_SERVER` (ON) — OFF でレジストリ単体ビルド (ネット非依存)

## 設計書

`spec/module/inspector.md` 参照。
