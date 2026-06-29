# Log モジュール定義

## 概要

開発用の簡易ロガー。4 レベル (**Error / Warn / Info / Debug**) を持ち、
各出力行の先頭に **現在のフレーム番号** をダンプする。フレーム番号は
`ergo_frame` の `count()` が既定ソース (ホストが `set_frame_provider` で
差し替え可能)。

Error / Warn は stderr、Info / Debug は stdout に出力。出力書き込みは
`std::mutex` でシリアライズされ、マルチスレッドから安全に呼べる。

レベルフィルタにより指定レベル以下のみ出力する。既定は `Info`
(Debug を抑制)。

## カテゴリ
システム

## 所属ドメイン
デバッグ / 開発者ツール

## 必要なデータ
- カレントレベル (Error=0 / Warn=1 / Info=2 / Debug=3)
- フレーム番号プロバイダ (`std::function<uint64_t()>`)
- 出力用 mutex

## 依存
- C++17 標準ライブラリ (`<mutex>`, `<cstdio>`, `<cstdarg>`, `<functional>`)
- (任意) `ergo_frame` — ホストが `set_frame_provider(&ergo::frame::count)`
  でつなぐ
- (テスト) GoogleTest 互換 mini-gtest

## 変数
- `level_` : カレントフィルタレベル
- `provider_` : フレーム番号取得関数
- `print_mtx_` : 出力排他 mutex

## 作業

### 入力
- `log(Level, fmt, ...)` 呼び出し (または `ERGO_LOG_*` マクロ)
- `set_level(Level)` によるレベル変更
- `set_frame_provider(fn)` によるフレームソース差し替え

### 出力
- `[F<frame>][<level>] <message>` 形式で 1 行
  - 例: `[F00012345][INFO] player connected`
- レベルごとに出力先 (stderr / stdout) を切り替え

### タスク
- printf スタイル `vsnprintf` で整形
- レベルフィルタで早期 return
- mutex 保持中に書き込み + fflush
- ダミープラグ (`log` は空) を提供

## テスト
- 各レベルで出力されること (stdout キャプチャ)
- フィルタが効くこと (Debug を隠す設定で Debug 行が出ない)
- `set_frame_provider` で注入したフレーム番号がプレフィクスに出ること
- マクロが展開されること
- マルチスレッド同時書き込みで行が混ざらないこと (ベストエフォート)
