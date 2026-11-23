# Ergo でゲームを制作するときに不足する実装

2026-11-24。Ergo のローカル main `4ec5061` のソース調査。対象は既存ゲームを Studio に接続して、制作・調整・試遊を一周する経路。実機動作は未確認。下表の順序は実装の依存関係に基づく提案で、個別機能の着手指示は含まない。

|工程|確認した実装・再利用元|不足する実装と所有先|受入条件|
|---|---|---|---|
|1. プロジェクトの受け入れ|[様式判定](../../src/studio/project/project_style.cpp)、[ランチャー](../../src/studio/project/launcher.cpp)、`assets`・`play`・`tuning` を読み込む[プロフィール処理](../../tools/studio/plugins/common/workspace_profiles.cpp)。|Ergo に Gen5 必須項目の読み取り点検と生成差分のプレビューが必要。項目ごとの確認後に対象リポジトリ向けの生成物を作り、再実行時は既存値と照合する。|設定済みのプロフィールと不足のあるプロフィールを入力し、既存値を維持して不足項目と生成先を示せる。|
|2. 用途別の制作環境|[内蔵プラグイン登録](../../tools/studio/builtin_plugins.cpp)、Studio の[プラグイン取得](../../tools/studio/plugins/studio/plugin_installer.cpp)と[台帳](../../tools/studio/plugins/studio/plugin_ledger.cpp)。|プロフィールの用途 (`domain`) に応じて構成を選び、外部索引から候補を確認して有効化する導線。索引側が信頼情報を管理し、Studio は署名・取得物を検証して構成を適用する。|同じ様式でも `lesson` と `game` で異なる構成を選べる。必要な pack の不足と検証エラーを識別できる。|
|3. ゲーム固有の編集|Ergo の [scene patch](../../src/scene/scene_patch.cpp) はルック用シーン文書を扱う。|タイトルの編集機能を Studio から開き、選択対象・保存先・プレイ対象を対応付ける。ステージのルールとスキーマはタイトルが所有する。|対象ゲームのステージを一件選び、編集・再読込・プレイで同じステージ ID を追跡できる。シーンビューのレイアウトは別途設計する。|
|4. 調整データの共通編集|[tuning の版管理](../../tools/studio/plugins/tuning/tuning_store.cpp)、[lookdev の保存](../../tools/studio/plugins/lookdev/lookdev_store.cpp)。|プラグインが列・型・単位・範囲を宣言し、Studio が行編集、エラー表示、ライブ値との比較、復元を行う共通経路。テーブルの保存先はプロジェクトのプロフィールで指定する。|範囲外の値は行と列を示す。妥当な変更は差分保存・再読込・旧版復元まで同じ値を追える。|
|5. 素材とプレイ中の出来事を結ぶ|[アクション教材の演出処理](../../examples/lesson_action_feel/core/presentation.cpp)は命中イベントを受けて表示を更新する。Ergo にはパーティクル・音声・UI のモジュールと素材管理プラグインがある。|タイトル側のイベントと素材 ID の対応を pack から辿り、該当イベントで試す経路。演出の実装はタイトル側が持つ。|命中イベント一件を固定し、素材差し替え前後でイベントの発生時刻が一致することと、参照先が欠けている場合に特定できることを確認する。|
|6. 試遊と改善|プロフィールの `play` から[プレイ目標](../../tools/studio/plugins/play/play_target.cpp)を組み立て、[プレイ画面](../../tools/studio/views/play_view.cpp)で起動・停止を操作できる。遠隔映像の表示はブラウザ側が担う。|プロフィールから試遊ルール・観測項目を引き渡し、ゲーム版・入力・画面・結果を改善案と結ぶ。ネイティブ画面にも遠隔映像を表示する。|同じ版・初期条件から、一つの成功例と失敗例をそれぞれの入力列で再現し、試遊結果から元の版・場面・操作を辿れる。|

## 制作スキル

1. [ergo-game-intake](../../.agents/skills/ergo-game-intake/SKILL.md) — 対象、存在する入口、不足、所有リポ、次に必要な人間判断の一覧。
2. [ergo-game-loop](../../.agents/skills/ergo-game-loop/SKILL.md) — 一枚のプレイ仕様と、実装すべきタイトル側の状態遷移、担当モジュール、観察例。
3. [ergo-game-mechanics](../../.agents/skills/ergo-game-mechanics/SKILL.md) — 通常入力と境界入力それぞれの期待状態、変更先、ゲーム側の実装差分。
4. [ergo-game-level](../../.agents/skills/ergo-game-level/SKILL.md) — ステージ差分、変更した ID、期待する経路と失敗時の復帰地点。
5. [ergo-game-feedback](../../.agents/skills/ergo-game-feedback/SKILL.md) — 素材仕様または素材差分、イベント対応表、音量・表示時間・同時発生上限、プレビュー条件。
6. [ergo-game-balance](../../.agents/skills/ergo-game-balance/SKILL.md) — 旧値・候補値・単位・観測指標・戻し先。
7. [ergo-game-playtest](../../.agents/skills/ergo-game-playtest/SKILL.md) — 再現可能な試遊票、観測事実、改善仮説、次に比較する版。
8. [ergo-game-delivery](../../.agents/skills/ergo-game-delivery/SKILL.md) — ビルド ID、対応環境、配布物、既知の制約、復旧手順、確認した経路。

工程 1 と 2 の検証には、対象ゲームの既存プロフィールと不足項目を含むテスト用プロフィールを使う。編集には対象ゲームの既存ステージを一件選ぶ。演出から試遊までの接続には、入力を固定して再現できるイベントを一件選ぶ。同一タイトルで全工程を確認する場合は、各工程で対象ビルドとデータの版を記録する。

関連設計は [Gen5 インテーク](2026-11-23-studio-gen5-intake.md)、[用途別構成とデータテーブル](2026-11-23-ergo-curation-and-data-tables.md)、[プラグイン索引](2026-11-23-ergo-plugin-registry.md)。
