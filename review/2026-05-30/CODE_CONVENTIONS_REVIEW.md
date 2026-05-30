# Ergo コード規約レビュー (2026-05-30)

規約: `AIFormat/RULE_CODE.md` (共通) + `Ergo/CLAUDE.md` 「コード規約 (Ergo 固有)」 — モジュールベース / プラグイン拡張思考 / SOLID 全 5 / `ergo_<domain>` cross-import 禁止 / consumer は plugin host 経由。

## サマリ
- 違反件数: Critical 2 / High 2 / Medium 2 / Low 1
- 注目点: `Engine::Impl` の責務複合と値変換 (`value_to_json` / `json_to_value`) の `VarKind` switch 分岐 (= OCP 違反) が筆頭。 モジュール cross-import / 公開 interface 境界は概ね健全。

## Critical

- `src/bind/bind_engine.cpp:106` — `Engine::Impl` が複数責務を同居
  - 違反: 共通 §1 (SRP) / §2 (ファイル分割) + Ergo 固有 SOLID-S
  - 詳細: 変数 registry / 保留中の書き込みキュー / WS クライアント / アクター階層 を 1 struct に内包。 「変更される理由」 が 4 つ以上。
  - 修正提案: `VariableRegistry` / `PendingWriteQueue` / `WsClient (既存)` / `ActorTreeHost` に分割。 まず `Impl` 内に sub-struct を切る (= 段階移行)、 次に file 分割。

- `src/shuriken_migrator/migrator.cpp` — 値変換系の空 catch が複数、 理由コメントなし
  - 違反: 共通 §「例外の空 catch 禁止 (理由コメント必須)」
  - 詳細: `try { ... } catch (...) { return false; }` が点在。 「変換失敗の自然な fallback」 であっても理由を明記する規約。
  - 修正提案: `catch (...) { /* Unity → ergo_particle 変換失敗時は false で skip (個別検証は呼び出し側) */ }` のように 1 行コメント付与。 もしくは `std::optional<T>` で例外を使わない構造に置換。

## High

- `src/bind/bind_engine.cpp:20-80` — `value_to_json()` / `json_to_value()` が `VarKind` enum で switch 分岐 (全 case 列挙)
  - 違反: Ergo 固有 SOLID-O (Open/Closed)
  - 詳細: `VarKind` に新型が増えるたび、 既存 2 関数を改修する必要がある。 plugin / registry になっておらず、 「ergo_bind を触らずに新型を足す」 が成立しない。
  - 修正提案: `VarKind` → converter (`ToJson` / `FromJson`) を **registry に登録** する形に。 新型追加は `BindRegistry::register<MyKind>(toJsonFn, fromJsonFn)` のみで完了させる。

- `include/ergo/actor/actor.h:21-22` — `Actor` が `bind::Engine::instance()` / `bind::Handle` / `bind::VarKind` を公開型として直接参照
  - 違反: Ergo 固有 SOLID-D (DI 不在) + モジュール cross-import の境界曖昧化
  - 詳細: actor が bind の具体 API に強結合。 `bind` 側を改変すると actor が壊れる + 切り出し / モック差し替えが困難。
  - 修正提案: `actor_bind_adapter.h` で opaque handle を切り、 actor は adapter 経由のみで bind と通信。 bind の具体型は actor public API から隠す。

## Medium

- `src/input/input_system.cpp:11-38` — `InputSystem::initialize()` で 4 デバイス分の条件 + 生成 + polling thread 起動が同居
  - 違反: 共通 §1 (SRP 境界微妙)
  - 詳細: 「マウス有効か? キーボード有効か? Touch は? Gamepad は?」 を 1 関数で順次処理。 テスト時の差し替えが効きにくい。
  - 修正提案: `DeviceFactory::build(config) → vector<unique_ptr<IInputDevice>>` で生成を分離、 `initialize()` は factory 結果を受け取って thread 起動に集中。

- `include/ergo/common/json_min.h` — `json_min` が `ergo::bind::jsonm` と `ergo::inspector::jsonm` の 2 namespace alias で公開され、 複数モジュールから再 export
  - 違反: 共通 §3 (レイヤ依存方向) — 共有ライブラリ位置の曖昧さ
  - 詳細: 同一実装が複数 namespace から見えると、 将来「bind だけ別 repo に切る」 等のリファクタが難しくなる。
  - 修正提案: `ergo_core` / `ergo_shared` 下層に `ergo::common::jsonm` を 1 本だけ置き、 各モジュールは alias せず明示的に参照する。

## Low / その他観察

- `include/ergo/render/stage_renderer.h` — `IRenderLayer` 実装は 1 責務 (色キューブ描画) に限定で SRP 良好。 将来「複数 pipeline 対応」 が必要になったら、 同 file 内分岐ではなく `IRenderLayer` 派生を増やす経路で対応する (= 既存規約に従う) こと。
- `src/bind/ws_client.cpp` — TLS / socket close 時の error 取扱いを点検し、 「best-effort swallow」 はコメント付与で正当化する。

## 全体評価

- **モジュール分割 / cross-import 禁止**: 概ね遵守。 `ergo_<domain>` 同士の直接依存は見つからず。
- **公開境界 (`IRenderLayer` / `IInputDevice` 等)**: interface 分離は良好。
- **筆頭リスク**: `Engine::Impl` の SRP 違反と値変換系の OCP 違反。 ここを直すだけで「新変数型 / 新通信パターンの追加」 速度が大きく上がる。
