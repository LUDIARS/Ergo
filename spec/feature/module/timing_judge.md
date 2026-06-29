# timing_judge モジュール定義

## 概要

ノートの正解時刻と入力時刻の差分から **PERFECT / GREAT / GOOD / MISS** を返す
純粋関数群。 状態を持たないので、 譜面パーサや UI とは独立に組み合わせられる。

`ergo::combo_counter` と組ませて「Good 以下でコンボ切る」 のような閾値ロジックを
組むためのヘルパ `breaks_combo()` も提供。

Ars 側ゲーム辞書 [`spec/game-lexicon/features/rhythm/timing-judge.toml`](../../../ars/spec/game-lexicon/features/rhythm/timing-judge.toml)
のリファレンス実装。

## カテゴリ

ロジック

## 所属ドメイン

音ゲー / 入力タイミング

## 必要なデータ

`Windows` 構造体 (3 つの ms 値):

- `perfect_ms`: Perfect 判定半幅
- `great_ms`: Great 判定半幅
- `good_ms`: Good 判定半幅

`perfect <= great <= good` を前提とする (loader 側で検証する想定)。

## 依存

- `<cstdint>` のみ
- 他 Ergo モジュール依存なし

## 変数

なし (ステートレス)

## 作業

### 入力

- `judge(target_ms, actual_ms, windows)`: 1 ペアを判定
- `breaks_combo(j, min_kept)`: 判定がコンボ切断条件を満たすか

### 出力

- `Judgment` enum 値 + `name()` で英語ラベル

### タスク

- `delta = actual - target`、 絶対値で 3 段階の窓と比較
- `breaks_combo` は単純に enum 順序比較

## テスト

- 中央 = Perfect
- 境界値が含まれる側 (`<=`) で正しく分類
- 早 / 遅で対称
- 名前文字列が enum と一致
- breaks_combo が min_kept ごとに正しい
