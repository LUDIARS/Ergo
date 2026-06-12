# Character モジュール定義 (ergo_character)

> 状態: v0.1 実装 (2026-06-12)。
> 思想の出典: Unity の [Project_TCC (Tiny Character Controller)](https://github.com/unity3d-jp/Project_TCC)。

## 概要

キャラクター制御を **小さなコンポーネントの合成** で表現する Character
Controller 基盤。TCC の 4 分類 (Brain / Check / Control / Effect) をそのまま
踏襲し、エンジン非依存 (Unity の Transform / Rigidbody / レイキャストに相当
する部分は全てホスト DI) の C++17 で再構成する。

- **Brain** — 各コンポーネントの出力を集約し、最終的な位置・向きを 1 箇所で
  確定する書込み層。位置の反映方法は派生 Brain が決める (v0.1 は
  `TransformBrain` = ホスト所有の `Pose` へ直接書込み)。
- **Check** — センサー層。Brain 反映より先に環境情報を更新する
  (`IUpdateComponent`)。レイキャスト等はホスト固有なので、本モジュールは
  接地情報の口 (`IGroundContact`) と手動実装 (`ManualGroundCheck`) のみ持つ。
- **Control** — 入力をキャラの移動・向きの「希望値」に変換する層
  (`IMoveSource` / `ITurnSource`)。**複数アタッチして優先度で切替える**。
- **Effect** — 移動の後処理として加算される速度 (`IEffectSource`)。重力・
  ノックバック等。Control と違い **全部加算** される。

合成規則 (TCC と同一):

1. 移動: `priority > 0` の `IMoveSource` のうち **最高優先度の 1 つだけ** が
   採用される (`control_velocity`)。
2. 向き: 同様に最高優先度の `ITurnSource` 1 つが採用され、その
   `target_yaw_deg` へ `turn_speed_deg` (deg/s) で最短弧補間する
   (負値なら即時スナップ)。
3. Effect: 登録された全 `IEffectSource` の速度を **加算** する。
4. `total_velocity = control + effect` を Brain が位置に反映する。
5. warp (瞬間移動・向きの強制) は Control / Effect より優先し、位置 warp 時は
   Effect 速度をリセットする。

優先度の切替は状態機械の代替になる: 歩行 Control (priority 1) と
はしご Control (priority 5、はしご接触中のみ > 0) を両方付けておけば、
priority の上下だけでモード遷移が表現できる。最高優先度の獲得/喪失は
lifecycle フック (`on_*_acquire_highest` 等) で通知する。

## カテゴリ

ゲームオブジェクト

## 所属ドメイン

キャラクター制御 / 移動・操作

## 必要なデータ

- ホスト所有の `Pose { Vec3 position; float yaw_deg; }` (TransformBrain の書込み先)
- 各 Control の入力値 (移動方向スティック等) — ホストが毎フレーム供給
- 接地状態 — ホスト実装の `IGroundContact` (または `ManualGroundCheck` に毎フレーム set)
- (任意) `TransformBrain::set_move_filter` に渡す衝突解決関数
  (`(position, delta) -> 実際に動かす delta`)。未設定なら自由移動

## 依存

- C++17 標準ライブラリ (`<vector>`, `<algorithm>`, `<functional>`, `<cmath>`)
- 他 Ergo モジュール依存なし (最下層)
- テスト: mini-gtest (`ergo_gtest_main`)

## 変数

- `BrainBase`: Move/Turn/Effect/UpdateComponent 各マネージャ + warp 保留状態 +
  position / yaw のキャッシュ
- `MoveManager`: 登録 `IMoveSource*` 一覧、現在の最高優先度 source、確定速度
- `TurnManager`: 登録 `ITurnSource*` 一覧、現在 source、目標 yaw / 補間後 yaw
- `EffectManager`: 登録 `IEffectSource*` 一覧、合算速度
- `MoveControl`: 入力ベクトル、速度、move/turn 優先度、最終入力方向の yaw
- `Gravity`: 落下速度、接地参照、着地/離地コールバック

## 作業

### 入力

- `BrainBase::add_move / add_turn / add_effect / add_update` (+ `remove_*`) —
  コンポーネントの明示登録 (Unity の GetComponents 相当は行わない)
- `BrainBase::update(dt)` — ホストのゲームループから毎フレーム 1 回
- `BrainBase::warp(position)` / `warp_yaw(yaw)` / `warp(position, yaw)`
- 各コンポーネント固有 API (`MoveControl::set_move_input` /
  `Gravity::set_ground_contact` / `ExtraForce::add_impulse` 等)

### 出力

- `TransformBrain` がホスト所有 `Pose` を更新
- アクセサ: `control_velocity` / `effect_velocity` / `total_velocity` /
  `current_speed` / `yaw_deg` / `target_yaw_deg` / `delta_turn_deg`
- lifecycle フック: 最高優先度の獲得・喪失・保持中更新 (Move / Turn 別)
- `Gravity::on_land` / `on_leave` コールバック

### タスク (update(dt) の処理順 — TCC BrainBase.UpdateBrain 準拠)

1. Check 層: `IUpdateComponent` を `update_order` 昇順で `on_update(dt)`
2. Move / Turn の最高優先度を再評価し、変化があれば lifecycle フック発火、
   保持中 source に `on_*_update_highest(dt)`
3. Effect 速度合算 → Control 速度確定 → 目標 yaw への補間計算
4. `total_velocity = control + effect`
5. 位置反映: warp 保留があれば直接セット (+ Effect リセット)、無ければ
   `apply_position(total, dt)` (派生 Brain 実装)
6. 向き反映: warp 保留があれば直接セット、無ければ最高優先度 Turn が
   いるときのみ補間後 yaw を `apply_yaw`
7. warp 保留をクリア

## テスト

- priority 0 以下の Control は不在扱い (採用されない)
- 複数 IMoveSource から最高優先度だけが velocity に採用される
- 最高優先度の獲得 / 喪失で lifecycle フックが正しい順序で発火する
- Turn は最短弧で補間し (350°→10° は +20° 側)、turn_speed 負値で即時
- Effect は全数加算され、Control と合算される
- warp は Control / Effect より優先し、位置 warp で Effect 速度がリセットされる
- `TransformBrain::set_move_filter` で移動 delta がホスト側で加工できる
- `MoveControl` は入力方向から目標 yaw を計算し、入力が無い間は最後の向きを保持
- `Gravity` は空中で加速し、接地で停止 + `on_land` 発火
- `IUpdateComponent` は `update_order` 昇順で処理される

## 制約 / 先送り

- RigidbodyBrain / NavmeshBrain 相当 (物理エンジン・経路探索統合) は対象外。
  必要になったホストが `BrainBase` を派生して実装する
- カメラ追従 (TCC CameraManager) はスコープ外 (Ergo ではホスト or 別モジュール)
- ピッチ / ロール回転は持たない (yaw のみ。TCC と同じ Y 軸 1 自由度)
- コンポーネントの自動収集はしない (明示登録のみ)
