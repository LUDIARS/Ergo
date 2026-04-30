# health モジュール定義

## 概要

任意のアクター / エンティティに組み込める **HP コンテナ**。
ダメージ・回復・死亡コールバックと、 任意の自動回復 (regen) を持つ。

物理 / 描画 / スレッドに依存しない純粋な状態 + コールバック。 ホストが `Health`
を**所有**して、 戦闘モジュールから `apply_damage()` を呼ぶ、 という使い方を想定。

Ars 側ゲーム辞書 [`spec/game-lexicon/features/core/health-system.toml`](../../../ars/spec/game-lexicon/features/core/health-system.toml)
のリファレンス実装。

## カテゴリ

ロジック

## 所属ドメイン

戦闘 / ステータス管理

## 必要なデータ

ホストから設定する `Config`:

- `max_hp` (int): 最大 HP。 既定 100
- `regen_per_second` (float): 自動回復量/秒 (0 で無効)
- `fire_death_event` (bool): HP=0 で `on_death` を発火するか

## 依存

- `<cstdint>`, `<functional>` のみ
- 他 Ergo モジュールへの依存なし

## 変数

- `cfg_`: 上記の `Config` コピー
- `hp_`: 現在 HP (0..max_hp)
- `regen_accum_`: 端数 regen を貯める float
- `death_fired_`: 死亡コールバック発火済みフラグ
- `on_damage_` / `on_heal_` / `on_death_`: ホスト登録コールバック

## 作業

### 入力

- `apply_damage(int)`: ダメージ適用。 0 以下と死亡後は無視
- `heal(int)`: 回復 (最大値で頭打ち)。 死亡中は無視 (`revive()` 必須)
- `tick(float dt)`: 自動回復のフレーム駆動
- `revive()`: HP を最大に戻し death 状態をクリア
- `set_on_damage / set_on_heal / set_on_death`: コールバック登録

### 出力

- `hp() / max_hp() / is_dead() / ratio()`: 読み取り
- 登録された 3 種コールバック (damage / heal / death)

### タスク

- 状態遷移は単純: 生 → ダメージで減少 → 0 で死亡 → revive で復帰
- `tick()` を毎フレーム呼ぶことで小数点 regen を切り捨てずに足し込む

## テスト

- 起動直後は max かつ alive
- ダメージで HP が減り、 0 で死亡コールバックが 1 回発火
- 死亡後の damage / heal は無視
- regen は端数を蓄積、 max を超えない
- revive で再開可能
- ratio が正しく計算される
