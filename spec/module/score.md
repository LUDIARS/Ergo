# score モジュール定義

## 概要

スコアの加算 / リセットと、 ハイスコア更新通知を扱うシンプルな数値器。
任意のコンボ倍率機能 (`combo_multiplier`) を持つ。

I/O は持たず、 ハイスコアの永続化はホストの責務 (`set_high_score()` で
ロード時に流し込む)。

Ars 側ゲーム辞書 [`spec/game-lexicon/features/core/score-system.toml`](../../../ars/spec/game-lexicon/features/core/score-system.toml)
のリファレンス実装。

## カテゴリ

ロジック

## 所属ドメイン

進行 / UI 表示

## 必要なデータ

`Config`:

- `combo_multiplier` (bool): コンボ倍率を掛けるか
- `combo_factor` (float): 倍率式 `m = 1 + combo * factor`
- `multiplier_cap` (float): 倍率上限 (0 で無制限)

## 依存

- `<cstdint>`, `<functional>` のみ
- 他 Ergo モジュール依存なし

## 変数

- `cfg_`: 上記 `Config`
- `score_` / `high_score_`: 内部カウンタ
- `on_change_` / `on_high_score_`: コールバック

## 作業

### 入力

- `add(base, combo_count = 0)`: ポイント加算。 戻り値は実際に加算された値
- `reset()`: 現在スコアを 0 に。 ハイスコアは維持
- `set_high_score(v)`: 永続化したハイスコアを流し込む (コールバックは発火しない)

### 出力

- `score()` / `high_score()`: 読み取り
- `on_change` / `on_high_score`: 通知コールバック

### タスク

- 状態は単純にカウンタの足し算 + ハイスコア比較
- 倍率は `1 + combo * factor`、 cap が指定されていれば cap で頭打ち

## テスト

- 初期 0
- combo 無効時は素の base 加算
- combo 有効時は m = 1 + combo*factor で増幅
- multiplier_cap で頭打ち
- ハイスコア更新時のみ on_high_score 発火
- reset でスコア 0、 ハイスコアは維持
- set_high_score はコールバック発火しない
