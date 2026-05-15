# REVIEW_DESIGN — Ergo (2026-05-13)

評価: **A−**

## 良い点

- `README.md:6-17` / `CLAUDE.md:9-30` で「全モジュール main 集約 + module/* ブランチ廃止」の運用変更とその理由を明文化しており、過去経緯 (worktree-per-module 廃止、ergo_inspector → ergo_bind 吸収) も追跡できる。
- 「内部モジュール (`include/ergo/<name>/` + `src/<name>/` + `tests/<name>/` + `spec/module/<name>.md`)」と「Web 開発者ツール (`tools/ergo/src/plugins/<id>/`)」を明確に分離 (`spec/tool/ergo.md:52-94`)。プラグインは factory 関数 + Hono サブアプリ + WS upgrade ハンドラの 3 点契約 (`tools/ergo/src/core/plugin.ts`, `server.ts:73-91`) で結合度が低い。
- `ergo_bind` をアウトバウンド WS クライアントに統一 (`src/bind/ws_client.cpp:175-204`) して Win32/POSIX のサーバ差分を吸収。`spec/tool/ergo.md:230-247` で旧 `ergo_inspector` との重複機能比較表を残し、廃止判断が明示されている。
- `ergo_custos` は HTTP/snapshot + callback 提供型に絞り、Vulkan 非依存 (`spec/module/custos.md:58-69`)。`src/custos/custos_module.cpp:121-126` の 3 エンドポイントは最小集合で、外部依存ゼロの PNG エンコーダ `src/custos/png_writer.cpp` を vendor して self-contained。

## 改善余地

- **D-1 (中)**: `module_list.md:11` が「`ergo_inspector` (廃止済み)」を依然として上から 2 番目に掲載しており、CLAUDE.md / README / spec の「2026-04-21 廃止」記述と矛盾する。新規メンバーが混乱しやすい。
- **D-2 (中)**: `module_list.md` は `ergo_physics` / `ergo_shuriken_migrator` / `ergo_actor` / `ergo_audio` / `ergo_world_time` / `ergo_blackboard` / `ergo_frame` / `ergo_log` / `ergo_io` / `ergo_ui` / `ergo_health` / `ergo_score` / `ergo_combo_counter` / `ergo_timing_judge` を網羅しているが、`README.md:42-53` のテーブルは `world_time` / `blackboard` までで途切れ、`physics` / `shuriken_migrator` / `health` / `score` / `combo_counter` / `timing_judge` / `ui` / `custos` / `actor` の記載が欠落。`README` が真の一覧として機能していない。
- **D-3 (低)**: `spec/module/bind.md:54` で「指数バックオフではなく一定間隔」を明示しているが、`src/bind/ws_client.cpp:200-203` の 150ms × 10 ループ (合計 1.5s) は固定でジッタがない。多インスタンス起動時の再接続スパイクが懸念される (将来課題として spec に書く価値あり)。
- **D-4 (低)**: `tools/ergo/src/core/registry.ts:21` の `Phase 2: makeInspectorPlugin` コメントは ergo_inspector 廃止 (2026-04-21) で陳腐化済み。
