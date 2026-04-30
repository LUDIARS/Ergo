# combo_counter モジュール定義

## 概要

連続成功 (hit) を数え、 失敗 (break) でリセットする小さなカウンタ。
ピーク (最大コンボ) と、 任意の **フルコンボ閾値** に到達したときの通知を持つ。

音ゲー (ノート連続正解 / ミスでリセット) と、 アクションの連撃カウントの両方で使える。

Ars 側ゲーム辞書 [`spec/game-lexicon/features/rhythm/combo-counter.toml`](../../../ars/spec/game-lexicon/features/rhythm/combo-counter.toml)
のリファレンス実装。

## カテゴリ

ロジック

## 所属ドメイン

進行 / UI 表示

## 必要なデータ

`Config`:

- `full_combo_threshold` (int): フルコンボ判定の閾値 (0 で無効)

## 依存

- `<cstdint>`, `<functional>` のみ
- 他 Ergo モジュール依存なし

## 変数

- `cfg_`: 上記 `Config`
- `count_`: 現在コンボ数
- `peak_`: 達成最大コンボ
- `on_change_` / `on_break_` / `on_full_combo_`: コールバック

## 作業

### 入力

- `hit()`: 成功。 count++ + peak 更新。 閾値到達で `on_full_combo`
- `break_()`: 失敗。 count を 0 に。 直前 > 0 のときだけ `on_break(直前値)`
- `reset()`: peak 含めゼロクリア。 `on_break` は出さない

### 出力

- `count() / peak()`: 読み取り
- `on_change` / `on_break` / `on_full_combo`: 通知

### タスク

- 状態は単純なカウンタ
- フルコンボ通知は **閾値ぴったり** で 1 回だけ (超過しても再発火しない)

## テスト

- 起動 0
- hit で count / peak 更新
- break で count=0 + on_break (count 0 のときは on_break 出さない)
- 閾値ちょうどで on_full_combo、 超過時は出ない
- on_change は hit / break (有効時) / 任意の状態変更で出る
- reset で peak まで 0 に
