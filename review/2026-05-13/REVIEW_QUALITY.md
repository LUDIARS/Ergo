# REVIEW_QUALITY — Ergo (2026-05-13)

評価: **A−**

## コード品質

- **命名 / スタイル**: 名前空間 `ergo::<module>` で統一、ヘッダ `include/ergo/<name>/<file>.h`、impl `src/<name>/` と階層が予測可能。 ファイル先頭の `///` ドキュメントコメントが各 cpp/ts でほぼ網羅 (`src/bind/bind_engine.cpp:1-13`, `src/custos/http_server.cpp:1-25`, `tools/ergo/src/core/server.ts:1-5`)。
- **マルチプラットフォーム抽象化**: `src/bind/ws_client.cpp:9-44` と `src/custos/http_server.cpp:6-25` で `_WIN32` / POSIX をマクロで切り、`socket_t` / `INVALID_SOCK` / `sock_close` を統一。重複があるが (ws_client と http_server で同じパターン)、各 cpp 内に閉じていて読みやすい。
- **テスト**: `tests/` 配下に各モジュール対応テスト (`tests/bind/`, `tests/custos/`, `tests/particle/`, `tests/physics/`, `tests/shuriken_migrator/` を含む 17 モジュール分)、mini-gtest ベースで CTest 統合 (`CMakeLists.txt:60-64`, `103-112`)。
- **ベンチマーク**: `benchmarks/bench_curve.cpp` / `bench_particle_cpu.cpp` を opt-in (`ERGO_BUILD_BENCHMARKS=OFF` 既定、`CMakeLists.txt:13`) で持っている。

## 改善余地

- **Q-1 (中)**: `src/bind/ws_client.cpp:9-44` と `src/custos/http_server.cpp:6-25` で **同じ Win32/POSIX ソケット薄膜が二重定義**。1 個の `include/ergo/common/socket.h` (header-only) に括ると重複が消える。
- **Q-2 (低)**: `src/bind/bind_engine.cpp:308-310` の `std::fprintf(stderr, ...)` 直書きは `ergo_log` (Error/Warn/Info/Debug) を持っているにもかかわらず使っていない。同 dir の他モジュールも `fprintf` 散見 (`src/custos/custos_module.cpp:144`, `src/custos/http_server.cpp:233`)。ergo_log への切り替えで「フレーム番号埋め込み」(`spec/module/log.md` の特徴) が効く。
- **Q-3 (低)**: `tools/ergo/src/plugins/variable/index.ts:82-84` の `JSON.parse(raw.toString())` は raw が `Buffer | ArrayBuffer | Buffer[]` の可能性 (`ws` lib spec)。 `raw.toString()` で大抵動くが、binary mode 受信で意外な落とし穴になりうる。 `'utf8'` 明示の方が安全。
- **Q-4 (低)**: テストは存在するが、`tools/ergo/` 配下に自動テストがない (`package.json:11-14` 参照、M-6 と重複)。
- **Q-5 (低)**: 一部コメントが日本語と英語が混在 (`src/custos/http_server.cpp` は日本語、`src/bind/bind_engine.cpp` は英語)。チーム規約が無いなら良いが、grep 検索性は片方向に寄せた方が良い。

## まとめ

C++/TS とも構造化された書き味で、テスト・ベンチ・spec が揃っているのが強み。横断重複 (Q-1) と ergo_log 不採用 (Q-2) は機械的に直せる範囲。
