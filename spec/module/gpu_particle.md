# GPU Particle モジュール定義

## 概要

Unity の Shuriken 相当の機能を想定した **GPU コンピュートパーティクル**
モジュール。1 つのエミッターが spawn / update の 2 パスを持ち、ライフ
タイム中の曲線 (size / rotation / color / velocity / atlas frame) を
1D テクスチャに焼いて compute シェーダからサンプルする。

GPU API 呼び出しは `IGpuBackend` 抽象に逃がしてあり、実際の Vulkan /
WebGPU バインディングはホスト統合層 (AdventureCube など) が実装する。
これにより本モジュール自体は **純粋な C++ コード** として単体ビルド・
テスト可能。

GPU バックエンドが与えられない場合 (テスト環境、CPU 検証時) は
`allow_cpu_fallback=true` で小さな CPU シミュレーションに切り替わる。
CPU 版は descriptor の妥当性とステートマシンを確認するためのもので、
完全な GPU 版と機能等価ではない (shape 別の初期位置分布、
サブエミッターなどは未実装)。

## カテゴリ

システム

## 所属ドメイン

描画 / VFX / ゲームオブジェクト補助

## 必要なデータ

- SPIR-V 化済み compute shader (`particle_spawn.comp` / `particle_update.comp`)
- エミッター記述 (`EmitterDescriptor`): lifetime / speed / size / rotation カーブ,
  gravity, damping, wind, shape (Sphere / Cone / Box), cone 角度・半径,
  ワールド変換, rate / rate_over_distance / bursts, simulation_space, max_particles, etc.
- エミッターごとの GPU リソース: particle SSBO, instance SSBO, counter SSBO,
  per-frame UBO, 曲線サンプル用 1D テクスチャ × N (size, rotation, color RGBA, atlas, velocity XYZ)
- CPU fallback 用の `Particle` / `InstanceData` 配列とランダム state

## 依存

- C++17 標準ライブラリ
- `IGpuBackend` を実装する外部 GPU ラッパ (Vulkan / WebGPU / モック)
- GLSL compute shader (本モジュールに同梱) + glslc (SPIR-V ベーク; 任意)
- (テスト) GoogleTest 互換 mini-gtest

## 変数

- `ParticleSystem::Impl` 内部:
  - `backend` (IGpuBackend*), `cfg` (ParticleSystemConfig)
  - `emitters` (handle → `EmitterRuntime`)
  - コンパイル済み compute shader ハンドル (`spawn_shader`, `update_shader`)
- `EmitterRuntime`:
  - `desc` (EmitterDescriptor), `world` (Float4x4), `state` (Play/Pause/Stop)
  - `time_since_play`, `spawn_accumulator`, `distance_accumulator`,
    `random_state`, `last_world_position`
  - GPU バッファハンドル (`particle_buffer`, `instance_buffer`,
    `counters_buffer`, `ubo_buffer`, 複数 `curve_tex_*`)
  - CPU fallback 用: `cpu_particles`, `cpu_instances`, `cpu_alive`

## 作業

### 入力

- ホストからの `initialize(backend, cfg)` / `shutdown()`
- `create_emitter(desc)` / `destroy_emitter(h)`
- 毎フレームの `update(dt)`
- エミッターの `play(h)` / `pause(h)` / `stop(h)` / `set_world_transform(h, m)` /
  `set_emitter_position(h, v)`

### 出力

- GPU モード: `get_instance_buffer(h)` / `get_counters_buffer(h)` —
  レンダリング層が indirect draw 等で読む。`get_live_particle_count(h)`
  は counters SSBO から readback (CPU 同期のためスタックしうる)。
- CPU フォールバック: `cpu_instances` (InstanceData 配列) を同 API で参照可能
- `get_descriptor(h)` / `get_state(h)` で記述と状態を問い合わせ

### タスク

- `initialize` で compute shader を SPIR-V から生成し、以降再利用
- `create_emitter` で GPU バッファを確保 + カーブを 1D テクスチャに焼く
- `update(dt)` ごとに:
  1. rate と rate_over_distance から `spawn_accumulator` / `distance_accumulator`
     を更新。累積が 1 を超えたら整数個の spawn を要求
  2. bursts は time マーカを超えたフレームで一括 spawn (cycles/interval は TODO)
  3. `EmitterUBO` を packing してバックエンドに upload
  4. spawn compute → update compute の順に dispatch、必要な buffer barrier を挟む
- CPU フォールバック時は上記を内部で線形シミュレーションに置換え
- `destroy_emitter` でバッファ群を解放、`shutdown` で backend の生成物を後片付け

### プラットフォーム別タスク

- **Vulkan 側実装 (ホスト統合層)**: `IGpuBackend` を実装し、SSBO / UBO / 1D
  texture / compute pipeline を Vulkan API へ変換する
- **WebGPU 側実装**: 同様に `IGpuBackend` を実装。SSBO は storage buffer、
  1D texture は storage texture もしくは sampled texture に対応させる
- **バックエンド不在 (CPU fallback)**: `allow_cpu_fallback=true` で
  `initialize(nullptr, cfg)` を許容。ユニットテストで使用
- **glslc 不在**: `ERGO_GPU_PARTICLE_COMPILE_SHADERS=OFF` または glslc が見つか
  らない場合は SPIR-V ベークをスキップ。ホスト側が事前ビルド SPIR-V を与える

## 既知の未完了

- Bursts の `cycles` / `interval` (繰り返し発火) は未対応 (one-shot のみ)
- CPU フォールバックの shape 別初期位置 (Sphere / Box など) は
  単純な +Y 方向の cone 近似になっている
- GPU 側の readback API (`get_live_particle_count`) は毎フレーム呼ぶと
  同期コストが高い — indirect draw 経由が本線、readback は診断用途のみ推奨

## テスト

- `EmitterDescriptor` のバリデーション (不正な min/max / 未サポート shape)
- 曲線 (`Curve`) の評価値 (端点, 線形補間, ランダム係数の反映)
- `ParticleSystem` のライフサイクル: initialize → create → play → update → destroy → shutdown
- CPU fallback で粒子数が emission_rate × 時間にほぼ比例すること
- `play` / `pause` / `stop(clear=true)` の状態遷移
- 多数エミッター並列作成・破棄でリソースリークしないこと (backend モックで
  allocate/free をカウント)
- バーストが指定時刻に 1 回だけ発火すること
