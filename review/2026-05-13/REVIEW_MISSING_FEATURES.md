# REVIEW_MISSING_FEATURES — Ergo (2026-05-13)

評価: **B**

仕様書 vs 実装のギャップを列挙する (機能の欠落、テストカバレッジ不足、TODO/将来課題)。

## 発見事項

- **M-1 (中)**: `spec/module/custos.md:73-75` 「拡張点 (将来)」で WS streaming / マウス inject / Vulkan layer hook を挙げているが、現状は GET /screenshot のスナップショット駆動のみ。Custos 親プロジェクト (`memory/MEMORY.md` の Custos Phase 2 = WebRTC capture / Cernere 認証 / Android 対応) と合流するためにストリーミング枠が必要。`src/custos/custos_module.cpp:121-126` の dispatch は HTTP のみで、WS upgrade ルートが無い。
- **M-2 (中)**: `tools/ergo/src/core/registry.ts:15-22` で `Phase 2: makeInspectorPlugin` がコメントアウト。`spec/tool/ergo.md:230-247` で **廃止判定** されているにも関わらず registry に文字列が残っているので、削除して経緯コメントを `CHANGELOG` 相当 (`spec/tool/ergo.md` 廃止プラン節) に集約すべき。
- **M-3 (中)**: `README.md:42-53` の Module テーブルが `world_time` までしか並んでいない。`spec/` / `include/` / `CMakeLists.txt` には `ergo_physics` / `ergo_shuriken_migrator` / `ergo_actor` / `ergo_health` / `ergo_score` / `ergo_combo_counter` / `ergo_timing_judge` / `ergo_audio` / `ergo_ui` / `ergo_custos` が存在する。README は実態の半分しか表現していない。
- **M-4 (中)**: `spec/module/physics.md` が `git ls-files` 結果に存在しない (`spec/module/` リストに `physics.md` が無い)。 `include/ergo/physics/` + `src/physics/` + `tests/physics/test_physics.cpp` の実装はあるが、定義書未整備で、CLAUDE.md「新規モジュール追加手順 step 1」(spec/module/<名>.md 作成) を満たしていない。同様に `spec/module/shuriken_migrator.md` の有無を再確認推奨 (現リストに無い)。
- **M-5 (低)**: `spec/module/bind.md:75` 「`ERGO_BIND_ENABLED` 未定義時にビルド・リンクが通ること (ダミー使用)」は対応する `src/bind/bind_dummy.cpp` で実装済みだが、当該 cpp 冒頭 7 行で `#define ERGO_BIND_ENABLED 1` を **強制** している。テスト意図 (未定義時の挙動) を直接検証できない。spec と実装で「Enabled マクロの責務」がねじれている。
- **M-6 (低)**: `tools/ergo/package.json:11-14` の `scripts` に `test` がない。`spec/tool/ergo.md:259-265` のスモークテストは手動 `npm install && npm run start` 前提で、自動化テスト (`/api/plugins` 件数、`/{id}/` 200 確認) が未実装。
- **M-7 (低)**: ergo_inspector 廃止 (`CLAUDE.md:30`) に伴い、ホストアプリ側の旧 `inspector` 取り込みパスがあれば撤去が必要。`module_list.md:11` の旧記述と合わせて誤誘導が残っている。

## まとめ

設計書と実装の整合 (M-3, M-4) と registry の死コード (M-2) を直すだけで一段クリーンになる。M-1 は Custos Phase 2 と同期して伸ばす長期課題。
