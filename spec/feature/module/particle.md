# Particle モジュール定義

## 概要
パーティクルエフェクトの **CPU シミュレーション** と **Pictor を介した Vulkan
ビルボード描画** を提供するモジュール。
unified ergo tool の `particle` プラグイン (TS/Node, `tools/ergo/`) から WebSocket で配信される
`ParticleEffectConfig` を受け取り (受信側は `ergo_particle_link` の `EditorLink`)、
ホストアプリ側で生きた粒子を更新・描画する。
ライブ編集 → 即視覚反映を主目的とする。

## カテゴリ
システム

## 所属ドメイン
ゲーム描画 / VFX

## 必要なデータ
- `ParticleEffectConfig` (`particle` プラグインのスキーマと同一; emission/initial/overLife/forces/render の各セクション)
- 起動時に渡される Pictor の `VulkanContext` 参照 + 描画ターゲットの `VkRenderPass`
- カメラ行列 (view / projection, column-major float[16])
- フレーム時間 (delta time, seconds)

## 依存
- C++17 標準ライブラリ
- `pictor` (ヘッダ + Vulkan コンテキスト)
- Vulkan SDK (glslc によるシェーダコンパイル)
- (任意) `ergo_particle_link` — `particle` プラグインの受信側 `EditorLink`。
  `ergo_particle` + `ergo_common_ws` (共有 WebSocket クライアント、
  [`common.md`](common.md)) + `ergo_common`。 `ERGO_BUILD_PARTICLE_LINK`
  (既定 ON) で切り離せる。 `ergo_particle` 本体はソケットに依存しない
- (テスト) GoogleTest 互換 mini-gtest

## 変数
- 粒子配列 (位置/速度/寿命/年齢/初期サイズ/初期色)
- 発生アキュムレータ (rate × dt の小数加算)
- 現在の `ParticleEffectConfig` (atomic / mutex 守られた単一値)
- Vulkan パイプライン (ビルボード描画用)
- 頂点/インデックスバッファ (1枚の quad mesh、全粒子で共有)
- インスタンスバッファ (粒子毎の model + color, ホスト可視メモリ)
- 描画モード (additive / alpha) ごとのパイプライン (もしくは動的ステート切替)

## 作業

### 入力
- ホストからの `set_config(ParticleEffectConfig)` 呼び出し
- (`ergo_particle_link`) `particle` プラグインが配信する `state` フレーム
  (`{op:"state", config, clients}`)。 `EditorLink` がワーカースレッドで受けて
  デコードし、 ホストが `poll(out)` / `apply_to(system)` でメインスレッドに
  取り込む (下記「エディタ連携」)
- ホストからの `update(dt)` 呼び出し
- ホストからの `record(VkCommandBuffer, VkRenderPass, view, proj)` 呼び出し

### 出力
- Vulkan command buffer に記録されたパーティクル描画コマンド
- (デバッグ) 現在の生存粒子数

### タスク
- 設定の差し替え (mid-frame race を避けるため二重バッファ or apply 関数で原子的反映)
- 発生処理 (rate × dt のアキュムレータ + maxAlive 上限)
- 粒子寿命管理 (age += dt; age >= lifetime で削除)
- 物理更新 (gravity 加算, velocity damping を per-second 指数で減衰)
- 補間値計算 (size, color を 0..1 の正規化年齢で線形補間)
- インスタンスバッファ更新 (毎フレーム host visible memory に書き込み)
- Vulkan パイプライン構築 (vert/frag シェーダ ロード, 頂点入力, ブレンド)
- ダミープラグ提供 (リンクのみ満たす no-op)
- リリース時の切り離し: ホスト側 CMake で本モジュール非リンク + ダミー差し替え

### エディタ連携 — `ergo_particle_link`
`ergo::particle::EditorLink` (`include/ergo/particle/editor_link.h`) は
`ws://host:5170/particle/ws` に接続する受信専用クライアント。 書き手は
ブラウザ UI だけで、 エンジンからは何も送らない。

- `connect(host = "127.0.0.1", port = 5170, path = "/particle/ws")` /
  `disconnect()` / `is_connected()`。 接続は非同期で、 切れても自動で再接続する
- `poll(out)` — 前回の `poll` 以降に **異なる** config が届いたときだけ true
  (世代カウンタをミューテックスで保護。 クライアントの増減で再配信される
  同一 config は数えない)。 `apply_to(system)` は `poll` +
  `ParticleSystem::set_config`
- `decode_state_frame(frame, out)` — `op == "state"` のフレームの `config`
  だけを、 既定値の `ParticleEffectConfig` に `parse_config_json` で重ねる。
  フレーム全体を `parse_config_json` に渡すと「成功して何も反映しない」 ので
  そうしない。 `ack` 等の他 op、 不正 JSON、 `config` 欠落は false
- `feed(frame)` — ソケットなしで受信経路を駆動する (テスト用、 または独自
  トランスポートを持つホスト用)
- ダミーは無い。 リリースビルドでは本ターゲットをリンクしない

```cpp
ergo::particle::EditorLink link;
link.connect();
// フレームごと (メインスレッド)
if (link.apply_to(system)) { /* config が変わった */ }
system.update(dt);
```

### プラットフォーム別タスク
- **Windows / Linux / macOS**: Vulkan が利用可能ならフル機能
- **WebGL**: 対象外 (Pictor の WebGL 経路は別途)

# テスト
- 設定なしでも `update(0)` / `record()` が落ちないこと
- rate=0 + maxAlive=0 で粒子が発生しないこと
- rate=N で 1秒後に約 N 個発生していること (許容誤差 ±10%)
- 寿命を超えた粒子が削除されること
- velocityDamping=1.0 で速度が変化しないこと
- gravity が速度を期待通り加算すること (1秒で gravity 値 ぶん増える)
- maxAlive を超える発生要求は無視されること
- size/color の補間が 0/1 の境界で初期/終端値と一致すること
- additive / alpha のブレンド切替が反映されること
- set_config を別スレッドから連打しても update/record が落ちないこと
- (`test_editor_link`) `state` フレームの config が反映されること /
  `ack` フレーム・不正 JSON・`config` 欠落は false で出力を変えないこと /
  部分的な config は欠けたフィールドを既定値にすること / `poll` が新しい
  config のときだけ true を返し、 同一 config の再配信は数えないこと /
  `apply_to` が `ParticleSystem` に反映すること
