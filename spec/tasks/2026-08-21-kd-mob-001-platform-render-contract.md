---
task: kd-mob-001-ergo-platform-render-contract
project: Ergo
kind: 実装
status: in-review
created: 2026-08-21
spec_refs:
  - spec/feature/module/render.md
  - spec/setup/build-cpp.md
---

# KD-MOB-001 — Ergo platform-neutral render contract

## 目的

`ergo_render` の描画オーケストレーションを GLFW / デスクトップ Vulkan の
具象前提から切り離し、Pictor の Windows / Android / iOS surface provider を
同じ契約で扱えるようにする。 モバイルホスト側リポジトリのモバイル対応
(KD-MOB-000 で決めた platform contract) が要求する upstream 側の前提。

## 背景 (変更前の状態)

- `RenderContext::surface` が `pictor::GlfwSurfaceProvider*` を持ち、公開
  ヘッダがデスクトップ具象型を要求していた。
- `CMakeLists.txt` の render gate が `find_package(Vulkan)` + `Vulkan::Vulkan`
  というデスクトップ固定判定で、Android / iOS では失敗し `message(STATUS)` の
  まま Vulkan-free ビルドへ黙って縮退していた。
- Pictor 側は既にモバイル契約を持っており (Android は NDK 同梱 Vulkan、
  iOS は MoltenVK)、いずれも `PICTOR_HAS_VULKAN` を PUBLIC で公開している。

## 変更内容

1. `include/ergo/render/render_context.h` — `surface` を
   `pictor::ISurfaceProvider*` に変更 (前方宣言のみ、具象型非依存)。
2. `include/ergo/render/render_backend.h` + `src/render/render_backend.cpp`
   (新規) — 実描画バックエンドのビルド時契約。`RenderPlatform` /
   `RenderBackendError` / `RenderBackendContract`。
3. `include/ergo/render/render_requirements.h` +
   `src/render/render_requirements.cpp` (新規) — `RenderContext` が実描画の
   前提を満たすかを型付きで検査する。
4. `FrameComposer::initialize()` — 戻り値を `RenderBackendError` にし、
   `last_error()` を追加。`run_frame()` の Vulkan-free 経路も失敗理由を
   型付きで記録する。**レイヤーの所有・初期化順・shutdown 順は不変**。
5. `cmake/ErgoRenderBackend.cmake` (新規) — `PICTOR_HAS_VULKAN` を正本に
   desktop / Android / iOS の実描画可否を判定し、確保できない構成を
   モバイルでは `FATAL_ERROR`、デスクトップでは警告
   (`ERGO_RENDER_REQUIRE_REAL=ON` で格上げ) にする。
6. `CMakeLists.txt` — render セクションを上記モジュールへ委譲。
7. spec — `spec/feature/module/render.md` (F)〜(H) と
   `spec/setup/build-cpp.md` にクロスプラットフォームビルド契約を記録。
8. `tests/render/test_render_backend.cpp` (新規) — 回帰テスト。

## 完了条件

- [x] `RenderContext::surface` が `pictor::ISurfaceProvider*` を borrow する
- [x] 公開 render ヘッダが `GlfwSurfaceProvider` 具象型を要求しない
- [x] desktop では既存 GLFW provider を同じ interface 経由で利用する
- [x] Android / iOS で Pictor target の runtime Vulkan contract を認識する
- [x] mobile configure 時に desktop `Vulkan::Vulkan` 不在だけで render を
      無効化しない
- [x] real render 不可を Vulkan-free の成功へ silent fallback せず、
      明示的な型付き失敗 (`RenderBackendError`) を返す
- [x] FrameComposer / layer の所有・shutdown 順を変更しない
- [x] cross-platform build 契約を Ergo spec へ記録する

## 未実施 (意図的)

- ビルド・ユニットテスト・統合テスト・起動テストは実行していない
  (ユーザからの明示指示が無いため)。`test_render_backend` は追加のみ。
- タッチ入力 / native host / app packaging は KD-MOB-003 / 004 / 005 の範囲。
- `ergo_particle` のモバイル対応は本タスクのスコープ外 (desktop 判定のまま)。

## 後続

- モバイルホスト側の Ergo dependency revision 更新と consumer adapter は
  本 PR マージ後の作業 (KD-MOB-001 のホスト側タスク)。
