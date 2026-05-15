# AUTOFIX — Ergo (2026-05-13)

ルール: ソースコード修正は禁止。本ファイルは **列挙のみ** (`autofix_count = 0`)。

実装フィックスは行わず、次回 `/ludiars-review` 走行時 or 手動修正候補としてリスト化する。

## 自動修正候補 (未実施)

| # | 種別 | 対象 | 詳細 |
|---|------|------|------|
| A-1 | doc | `module_list.md:11` | 廃止済 `ergo_inspector` 行を削除 (`CLAUDE.md:30` / `spec/tool/ergo.md:230-247` 参照) |
| A-2 | doc | `README.md:42-53` | Module テーブルを `module_list.md` と同期 (physics/shuriken_migrator/actor/health/score/combo_counter/timing_judge/audio/ui/custos を追記) |
| A-3 | doc | `tools/ergo/src/core/registry.ts:21` | `// Phase 2: makeInspectorPlugin,` コメントを削除 |
| A-4 | spec | `spec/module/physics.md` (新規) | 物理モジュールの spec ファイル新設 (template/module_template.md に従う) |
| A-5 | spec | `spec/module/shuriken_migrator.md` (新規確認) | 同上、未整備なら新設 |
| A-6 | code | `src/custos/http_server.cpp:98` | `std::stoul(val)` を try/catch でガード (V-1) |
| A-7 | code | `src/custos/custos_module.cpp:36-60` | `parse_key_body` の `down_s` 不正値を 400 で返す (V-2) |
| A-8 | code | `src/bind/ws_client.cpp` / `src/custos/http_server.cpp` | Win32/POSIX socket 薄膜を `include/ergo/common/socket.h` に集約 (Q-1) |
| A-9 | code | `src/bind/bind_engine.cpp:308` / `src/custos/*.cpp` | `std::fprintf(stderr,...)` を `ERGO_LOG_WARN/INFO` に置換 (Q-2) |
| A-10 | code | `tools/ergo/src/plugins/variable/index.ts:84` | `raw.toString()` を `raw.toString('utf8')` に明示化 (Q-3) |
| A-11 | test | `tools/ergo/package.json:11` | `"test"` script + 最小 fetch スモークの追加 (M-6) |
| A-12 | code | `tools/ergo/electron/main.cjs:64` | `setTimeout(createWindow, 150)` を `boot()` の listen callback ベースに置換 (I-4) |

autofix_count: **0**
