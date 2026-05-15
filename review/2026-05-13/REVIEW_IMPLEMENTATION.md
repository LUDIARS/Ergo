# REVIEW_IMPLEMENTATION — Ergo (2026-05-13)

評価: **A−**

## 良い点

- `src/bind/bind_engine.cpp:105-256` の `Engine::Impl` は (a) スレッド安全な registry mutex, (b) `wq_mtx` 分離による recv-thread → main-thread MPSC ハンドオフ, (c) 再接続時の actor → bind 順序保証 (`on_open` 245-246 行)、(d) `clamp_to_meta` + read_only ガード (220, 432 行) が揃っており、ライブチューニング基盤として完成度が高い。
- `src/bind/ws_client.cpp:117-144` の masked text-frame encoder と `298-347` の非同期 frame reader は WebSocket RFC6455 の必要十分なサブセット (text+close+ping/pong skip)。oversized frame (`kMaxFramePayloadBytes`) で接続を切る `return -1` 経路も妥当 (`worker_loop:241-246`)。
- `src/custos/http_server.cpp` は WSAStartup スコープを `WsaScope` (line 32-40) で RAII 管理、`SO_REUSEADDR` セット (208) と `getsockname` で実 port 取得 (246-252) が丁寧。`handle_connection:298-304` の handler 例外 → 500 変換も明示的。
- `src/custos/png_writer.cpp` の zlib STORED + Adler32 自前実装は依存ゼロ要件 (`spec/module/custos.md:60-62`) を満たしつつコンパクト。コメント (24-23 行) が PNG/zlib のレイアウトを明示。
- `src/particle/particle_system.cpp:44-102` の update は `cfg_mtx_` を簡単に通り抜けるダブルバッファ (`active_cfg_` line 47-49)、`dt` クランプ (50-51)、swap-and-pop 削除 (71-73) と典型的最適化が揃っている。

## 改善余地

- **I-1 (中)**: `src/bind/bind_engine.cpp:454-463` の値変化検出は **全 var を毎フレーム getter 呼び + last_seen 比較** で O(N)。100+ var で `Engine::apply_pending_writes()` を 60fps で回す前提 (`spec/module/bind.md:78`) は通るが、`std::string` 比較 (line 58 `s == o.s`) がコピー込みで効く。dirty マーキング型 API を検討余地あり。
- **I-2 (低)**: `src/custos/http_server.cpp:64-66` `read_request` は ヘッダ 16 KiB cap でループするが、body 側 `108-113` には全体サイズ上限が無い。`/key` body は実用上小さいので問題は表面化しないが、`Content-Length: 4294967295` 等で OOM を誘発される (V-1 と合わせて要バリデーション)。
- **I-3 (低)**: `tools/ergo/src/plugins/variable/index.ts:152-167` の close handler は engine 切断時に owner の var/actor をループ削除するが、N×M (registry × actors) で、broadcast を 2 回呼ぶ程度。ただし `for (const [name, v] of registry) { ... registry.delete(name); }` は走査中 mutate でも Map 仕様上は安全な範囲。コメントで明示しておきたい。
- **I-4 (低)**: `tools/ergo/electron/main.cjs:64` の `setTimeout(createWindow, 150)` は `serve()` callback で listen 確定後に進めるのが正攻法。`boot()` を Promise 化するか listen done コールバックを露出させると 150ms 推定が消える。

## まとめ

主要 4 モジュール (bind / custos / particle / tools/ergo) は責務分離 + スレッド安全 + 例外境界が概ね一貫している。残課題は最適化系 (I-1) と入力 validation (I-2) で、構造的破綻は無い。
