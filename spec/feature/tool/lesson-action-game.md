# 授業のサンプルゲーム (`examples/lesson_action_feel`) {#SPEC-ergo-lesson-game}

依頼 (2026-11-11〜12):

- Odeum の授業「アクションゲームの作り方」(サンプル講座 `action-game`) の Ergo のサンプルを、敵を倒して遊べるゲームにする。
  敵にスーパーアーマーを付けるかを選べるようにする。
- モーションキャンセル・ひるみ・スーパーアーマーを戦闘の概念として実装し、敵とプレイヤーそれぞれに設定で適用する。
- ボスが咆哮したり、ため攻撃を打つときにブラーがかかるようにする。
- モーションブラーを入れる。モーションベクターから作り、ベクターを見られるようにする。
- 全ての演出系の実装を切れるようにしておく。

授業との通信 (Studio の `lesson` プラグイン) は [lesson.md](lesson.md)。このファイルはゲームそのものの正本。
参考実装は [vtn-team/ActionGameSample](https://github.com/vtn-team/ActionGameSample) (Unity。2026-11-11 に commit
`a81d6b3` を読んだ。`Setting.cs` に演出の切り替え 6 つ (CameraShake / ParticleEffect / HitStop / Knockback / DamageUI /
HPBarEffect)、`HitCtrl.cs` はダメージで体力を減らし、威力でノックバックの撃力とヒットストップの長さ `威力 ÷ 24 秒` を決め、
`Character.HitBack` は受けた側の後ろ向き (`-transform.forward`) に押す。音は無い)。

## 構成 {#SPEC-ergo-lesson-game-layout}

1 つのゲームの中身 (core) と、それを使う front-end の組。2 つの別々のサンプルにはしない。

| 置き場所 | 責務 | 層 (`.anatomia/layers.json`) |
|---|---|---|
| `core/` | 規則と状態 (`ActionGame`)、演出 (`Presentation`)、その組 (`GameSession`)。純粋 (I/O・時計・描画なし)、決定的 | domain-logic |
| `terminal/` | 端末の front-end: キーの解読と意味、移動の保持、画面の文字列、ブラーと軌跡の文字への置き換え、ANSI、raw 端末 | presentation |
| 直下 | 実行ファイル: コマンドライン、`ergo_bind` への公開、切り替えの記録、3 つのモード | application |
| `tests/` | core と front-end の試験 | shared |

core が使う Ergo のモジュール: `ergo_world_time` (ヒットストップ)、`ergo_health` (体力)、`ergo_score` と `ergo_combo_counter`
(得点と連続ヒット)。`ergo_math` (C++20) と `ergo_physics2d` は使わない: サンプルはリポジトリの標準の C++17 でビルドするので、
2D ベクトルは `core/vec2.h` に持つ。剛体の接触解決は要らない (体どうしは重なった分を質量に応じて押し戻すだけ、壁は位置の制限)
ので、ノックバックは「速さに撃力 ÷ 質量を足し、摩擦で減らす」で書く。`ergo_input` は窓や OS のデバイスを読む作りで、端末の文字入力
(キーを離したことが届かない) とは契約が合わないため、端末のキー読みはこの例の `terminal/` に置く。

**シミュレーションと演出を分ける**: `ActionGame` は規則と、ゲームを変えてよい 2 つの演出 (ヒットストップ・ノックバック) だけを持つ。
ほかの演出は `Presentation` が `ActionGame` とそのフレームの当たりの記録 (`Frame::hits`) を読んで作り、`GameView` に足すだけで、
`ActionGame` には書かない。だから見た目だけの演出を切っても、ゲームの状態はビット単位で同じ (試験で確かめる)。

## ルール {#SPEC-ergo-lesson-game-rules}

- アリーナ: 幅 30 × 高さ 12 (単位)。四方を壁で囲む (落ちる足場は無い)。見下ろし型。カメラは動かない (スクロールしない)。
- プレイヤー: 体力 100、速さ 9 単位/秒、半径 0.5、質量 1。歩いた向きを向く。攻撃は向いている方へ出る。
- 武器と技 (プレイヤーは K で持ち替え。攻撃の途中なら Ready になった時に替わる):

  | 武器・技 | ダメージ | 威力 | 届く距離 | 扇の半角 | 構え | 振り | 隙 |
  |---|---|---|---|---|---|---|---|
  | 剣 (sword) | 20 | 2 | 1.8 | 約 70° | 0.06 s | 0.10 s | 0.22 s |
  | ハンマー (hammer) | 35 | 6 | 2.2 | 90° | 0.28 s | 0.12 s | 0.42 s |
  | 敵の爪 (claw) | 12 | 3 | 1.4 | 60° | 0.55 s | 0.10 s | 0.50 s |
  | ボスの咆哮 (roar) | 0 | 0 | 6 (全方位) | — | 0.50 s | 0.60 s | 0.60 s |
  | ボスのため攻撃 (charge) | 40 | 9 | 2.6 | 90° | 1.40 s (ため) | 0.15 s | 0.80 s |

  ダメージは体力だけを減らす。威力はノックバックの撃力 (`威力 × 6`)、ヒットストップの長さ (`威力 ÷ 24 秒`、参考実装と同じ)、
  カメラの揺れ (`威力 × 0.15`、上限 1.0) を決める。
- 攻撃は 3 つの相: **構え** (当たらない。敵ではこれが予告)、**振り** (扇の中に入った相手に、1 回の攻撃につき 1 体 1 回当たる)、
  **隙** (当たらない)。構えの前に押した攻撃は 0.15 秒 (世界時間) 覚えておく。
- 敵: 体力 100、速さ 3.5。プレイヤーへ歩き、届く距離に入ったら、待ち時間 (1.8 秒。最初は 0.6 秒 + 出現順 × 0.45 秒でずらす) が明け、
  同時に構え・振りをしている敵が 2 体未満なら、プレイヤーを向いて爪を構える (`!`)。
- 波: 2 体 → 3 体 → 4 体 → **ボス (1 体)**。全員を倒すと 1 秒 (世界時間) 後に次の波が、アリーナの縁の、プレイヤーから遠い点から出る。
  **ボスを倒すと勝ち**、プレイヤーの体力が 0 で負け。`--endless` (と `--auto`・`--autopilot`) は終わらない: 2, 3, 4, … 体 (上限 6) の波の
  4 つ目ごとがボス。`--boss` はボスの波から始める (試験・実演用)。勝ち負けを付けたのは、授業の課題で「何体倒した」「勝てた」を
  目標にできるようにするため。
- 倒された敵は 0.4 秒 (世界時間) 残ってから消える (これは規則: 次の波が来る時を決める。`x` で描くかは演出 `defeat_mark`)。
- 得点: 当てたダメージ + 倒すと 100 (ボスは 500)。どちらも連続ヒットの倍率 (`1 + 連続数 × 0.1`) が掛かる。連続ヒットは、
  プレイヤーが当てると増え、当てられるか 1.5 秒 (世界時間) 当てないと切れる。
- 同じフレームで両者の振りが当たったときは、プレイヤーの当たりを先に処理する。

### ボス {#SPEC-ergo-lesson-game-boss}

体力 400、速さ 2.2、半径 1.0、**質量 4** (押しても 1/4 しか動かない)、**踏ん張り** (`boss_min_flinch_power` = 4): 威力 4 未満の当たり
(剣) ではひるまない。ハンマー (威力 6) なら、敵側のひるみが on でアーマーが無ければひるむ。記号は `B`。普通の敵の攻撃の枠
(同時に 2 体) には数えない。

- 順番: **咆哮 → ため攻撃 → ため攻撃** を繰り返す。1 つの技の後、技の長さ + 1.2 秒 (世界時間) は次の技を出さず、プレイヤーへ歩く。
  咆哮はプレイヤーが 6 以内、ため攻撃は届く距離 × 0.9 以内に入ったら出す。ボスはモーションキャンセルで続けて出さない。
- **咆哮**: 振り (0.6 秒) の間に、6 単位 (+ 相手の半径) 以内にいる相手を**強制的にひるませる** (`roar_stagger` 0.8 秒)。相手の陣営の
  ひるみの設定に関係なく効く (咆哮は当たりではなく「すくませる」技なので)。効かないのは、その瞬間に相手のスーパーアーマーが効いている
  とき (吸収として数える)。ダメージ・ノックバック・ヒットストップは無い。**咆哮そのものはスーパーアーマー付き** (技の定義
  `Weapon::armored`: 構えと振りの間は、敵側の設定に関係なく当たってもダメージだけ)。
- **ため攻撃**: 1.4 秒ためてから (届く範囲を点で予告)、放つ (ダメージ 40、威力 9 → 撃力 54、ヒットストップ 0.375 秒、揺れ 1.0)。
  ためている間に当てたときに止まるかは、敵側の設定とボスの踏ん張りで決まる:

  | 当てたもの / 敵側の設定 | ためは止まるか |
  |---|---|
  | ハンマー、ひるみ on、アーマー off | 止まる (ボスはひるむ) |
  | 剣 (威力 2 < 踏ん張り 4) | 止まらない |
  | ハンマー、アーマー attack / always | 止まらない (吸収) |
  | ハンマー、ひるみ off | 止まらない |

## 演出の一覧 (切り替えられるもの全部) {#SPEC-ergo-lesson-game-effects}

ゲームにある演出はすべてこの一覧 (`core/feel_effects.h` の `effect_table()`) に載り、どれも切れる。見た目の演出で、いつも ON に
固定したものは無い。入口はどれも同じ 3 つ: 起動時の `--effect <名前>=on|off` (`--plain` で全部 OFF、`--all-effects` で全部 ON (既定))、
Studio の変数 `feel.<名前>` (bool)、`--play` の演出の画面 (O。a〜j で 1 つずつ)。`feel.all` (bool) と 0 キーは全部をまとめて切り替える:
`feel.all` は読むと「全部 ON か」、false を書くと全部 OFF、true を書くと全部 ON。0 キーは 1 つでも OFF があれば全部 ON、全部 ON なら全部 OFF。
観測値は「実際に起きたこと」で、その演出が OFF の間は 0 のまま (試験で全部を列挙して確かめる)。

| 名前 (`feel.<名前>`) | 何をするか | ゲームを変えるか | キー | 既定 | 観測値 (読み取り専用) | 参考実装 |
|---|---|---|---|---|---|---|
| `hit_stop` | 当たると世界を `威力 ÷ 24` 秒止める (`ergo_world_time`) | **変える** (世界時間の進み) | 1 / a | ON | `feel.hit_stop_last_seconds`: 実際に止まった最後の秒数 | HitStop |
| `knockback` | 当たった相手を `威力 × 6 ÷ 質量` の速さで押す | **変える** (位置) | 2 / b | ON | `feel.knockback_last_impulse`: 最後の撃力 | Knockback |
| `camera_shake` | 絵全体を `威力 × 0.15` (上限 1.0) 揺らし、実時間で減らす | 見た目だけ | 3 / c | ON | `feel.camera_shake_last_amplitude` | CameraShake |
| `hit_spark` | 当たった相手の両脇に火花 `*` (実時間 0.15 秒) | 見た目だけ | d | ON | `feel.hit_spark_count` | ParticleEffect |
| `damage_numbers` | 当たった所にダメージの数字 `-20` (実時間 0.6 秒で 2 行上がる) | 見た目だけ | e | ON | `feel.damage_numbers_shown` | DamageUI |
| `hp_gauge` | 自分の体力のバーの減った分を `=` で 0.25 秒残し、60 HP/秒で減らす | 見た目だけ | f | ON | `feel.hp_gauge_last_drain`: 最後に減って見せた量 | HPBarEffect |
| `telegraph` | 構えている敵の届く範囲を `.` で描く | 見た目だけ | g | ON | `feel.telegraph_count` | — |
| `defeat_mark` | 倒した敵を消えるまで `x` で描く | 見た目だけ | h | ON | `feel.defeat_mark_count` | — |
| `boss_blur` | ボスの咆哮・ため攻撃の間の画面のブラー (下) | 見た目だけ | i | ON | `feel.boss_blur_last_peak` | — |
| `motion_blur` | 物ごとの動きの軌跡 (下) | 見た目だけ | j | ON | `feel.motion_blur_last_length` | — |

- 参考実装の 6 つは、この一覧の上 6 つにそのまま対応する (ParticleEffect は火花、DamageUI は数字、HPBarEffect は体力のバーの減り方)。
- **ゲームを変えてよいのはヒットストップとノックバックだけ**: ヒットストップは世界時間の進み方を、ノックバックは位置 (と、それで変わる
  間合い) を変える。どちらを切っても規則 (ダメージ、何回で倒れるか、ひるみ・アーマー・キャンセルの判定、勝ち負け) は変わらない。
  ほかの 8 つは描くだけで、切ってもシミュレーションの状態は同じ。
- **戦闘の決まりは演出ではない**: ひるみ・スーパーアーマー・モーションキャンセル (陣営ごと) は規則なので、この一覧にも `--plain` にも
  `feel.all` にも入れない (`combat.*`、下の「陣営ごとの設定」)。デバッグ用のモーションベクターの表示 (V) も演出ではない。
- 揺れ・火花・数字・体力のバーの減り方・ブラーの減り方は**実時間**で動く。ヒットストップの間も見る人の側は止めない。

### ボスのブラー (`boss_blur`) {#SPEC-ergo-lesson-game-boss-blur}

画面全体に 1 つの効果 (物ごとではない)。core が毎フレーム `GameView::blur` (`strength` 0〜1、`kind`、`centre` = ボスの位置) を出し、
描く側がそれで絵をぼかす。包絡 (`core/boss_blur.cpp`、数値は `PresentationTuning::boss_blur`):

- 咆哮: 構えの間 0 → 0.15、振りの間 0.15 → 0.6 (相の進み具合に比例)、種類は Radial (ボスから外へ広がる)。
- ため攻撃: ための間 0 → 0.7 (単調に増える)、放った瞬間 (構え → 振り) に **1.0**、種類は Zoom (外からボスへ集まる)。
- どちらの後も、また目標が下がったときも、毎秒 `e^-2.5` で減る。**相の進みは世界時間** (ボスの行動なので)、**減り方は実時間**: 放った
  当たりのヒットストップで世界が止まっても、ブラーは減り続ける (止まった絵の上でブラーだけが引いていく)。
- ボスの行動が無いとき・スイッチが OFF のときは 0。観測値 `feel.boss_blur_last_peak` は、終わった最新のボスの行動の最大値。

### モーションベクターとモーションブラー (`motion_blur`) {#SPEC-ergo-lesson-game-motion}

ボスのブラーとは別物: こちらは**物ごと**に、その物が動いた分だけ軌跡を付ける。

- **モーションベクター** (`FighterView::motion`、単位/フレーム): 画面上の、前のフレームからの位置の差。カメラはスクロールしないので
  画面上の差 = 世界の位置の差で、カメラ自身のベクター `GameView::camera_motion` はいつも 0。**カメラの揺れのずれは含めない** (揺れは
  見る人の側の小さな震えで、含めると揺れるたびに全部の物に軌跡が付いてしまう)。物が動く時計 (世界時間) で動いた分なので、
  **ヒットストップの間は 0** (止まった瞬間は軌跡の無いくっきりした絵になる)。初めて見る物 (出現・次の波・遊び直し) は 0 から
  始める (遊び直しは番号の世代 `ActionGame::epoch` で見分け、前の位置を全部捨てる)。演出のスイッチに関係なくいつも計算する
  (デバッグ表示と Studio が読む)。
- **軌跡** (`core/motion_blur.cpp` の `motion_streak`、数値は `PresentationTuning::motion_blur`): 長さ = `|ベクター| × シャッター (5
  フレーム分)`、`max_length` 3.0 で頭打ち、`min_length` 0.8 未満なら描かない (ふつうに歩く 0.15 単位/フレーム → 0.75 は出ない)。
  向きは進む向きのちょうど反対 (物の後ろ)。標本数 = `ceil(長さ ÷ 0.5)`。`FighterView::streak` = 向き × 長さ (スイッチ OFF なら 0)。
- 目に見えるのは主にノックバック: 剣 (撃力 12 → 0.2 単位/フレーム) で 1.0 の短い軌跡が 1〜2 フレーム、ハンマー (撃力 36) で 3.0 の
  長い軌跡が数フレーム。重いボスは押されても遅いので、ほとんど出ない。
- 観測値 `feel.motion_blur_last_length`: 軌跡を描いた最新のフレームの、いちばん長い軌跡。Studio に見せるベクター:
  `feel.motion_vector_player` と `feel.motion_vector_last_hit` (vec3 (x, y, 0)、単位/フレーム、読み取り専用。後者は最後に当たった敵)。

## 戦闘の三つの概念 {#SPEC-ergo-lesson-game-combat}

プレイヤーも敵もボスも同じ型 (`Combatant`) と同じ行動の決まり (`core/combat_rules.cpp`) で動き、陣営ごとの設定 (`SideSettings`) だけが違う。
敵は `EnemyBrain`、ボスは `BossBrain` が、プレイヤーはキー・台本・自動操縦が、同じ `Intent` (歩く向き・向く向き・攻撃・技) を作る。

- **ひるみ** (`flinch`): 吸収されなかった当たりで、受けた側は `flinch_seconds` (既定: プレイヤー 0.30 秒、敵 0.45 秒、世界時間)
  の間ひるむ。ひるんでいる間は歩けない・向きを変えられない・攻撃できない (押した攻撃も捨てる)。出していた攻撃 (構え・振り・隙の
  どれでも) はそこで終わる (`interrupted`)。ひるみを OFF にした陣営は、当たっても体力が減るだけで行動は続く (ノックバックは別の
  スイッチなので押されはする)。ボスは威力が踏ん張り未満の当たりではひるまない。
- **スーパーアーマー** (`super_armor`、`off` / `attack` / `always`): 付いている間に受けた当たりは**ダメージだけ**: ひるまない、
  行動が止まらない、**押し返されない**。ヒットストップとカメラの揺れは起きる (当てた側の手応えはそのまま)。`attack` は自分の攻撃の
  構えと振りの間だけ (隙には付かない: 振り終わりが反撃の機会)、`always` はいつも。付いているかは当たった瞬間の状態で決める。
  体力が 0 になればアーマーがあっても倒れる。技そのものにアーマーが付くこともある (ボスの咆哮)。
- **モーションキャンセル** (`motion_cancel`、on / off): **当たった攻撃の隙**を、新しい攻撃か移動で打ち切れる。打ち切れるのは隙の相だけで、
  構えと振りは打ち切れない。外した攻撃の隙も打ち切れない。「いつでも打ち切れる」は隙を 0 にするのと同じで、設計の違いとして見せる
  意味が薄いので、当たったときだけの 1 種類にした。敵は on のとき、当たった爪の隙を打ち切って、プレイヤーがまだ届く所にいれば 1 回だけ
  続けて振る。

**優先順位** (当たり 1 回の処理、`resolve_hit`):

1. ダメージは必ず入る。体力 0 → 倒れる (行動は終わる。ほかは見ない)。
2. スーパーアーマーが効いていれば → ダメージだけ (ひるまず、止まらず、押されない)。**アーマーはひるみより強い**。
3. そうでなく、ひるみが on で威力が踏ん張り以上 → 行動が止まってひるむ。**モーションキャンセルの設定に関係なく止まる**。
4. ノックバック (`feel.knockback` が ON で、アーマーに吸収されていなければ) で押す。
5. ヒットストップは吸収されたかどうかに関係なく、スイッチに従う。

咆哮 (`resolve_roar`) は当たりではない: アーマーが効いていれば吸収、そうでなければ (ひるみの設定に関係なく) ひるませる。
**ひるみは打ち切れない**。**時計**: 攻撃の相・ひるみ・押した攻撃の記憶・敵とボスの待ち時間はすべて世界時間なので、ヒットストップの間は
止まる (ひるみは実時間では止まりの分だけ長く見える)。

既定は、初めて遊ぶときに分かりやすいもの: プレイヤーはひるみ on・アーマー off・キャンセル on、敵 (とボス) はひるみ on・アーマー off・
キャンセル off。

### 陣営ごとの設定の 3 つの入口 {#SPEC-ergo-lesson-game-settings}

| 設定 | 起動時 (CLI) | Studio の変数 | キー (`--play`) | 既定 |
|---|---|---|---|---|
| 敵のひるみ | `--enemy-flinch on\|off` | `combat.enemy.flinch` (bool) | 4 | on |
| 敵のスーパーアーマー | `--enemy-super-armor off\|attack\|always`、`--super-armor` (= attack) | `combat.enemy.super_armor` (int32 0/1/2)、`feel.super_armor` (bool) | 5 (off → attack → always → off) | off |
| 敵のモーションキャンセル | `--enemy-cancel on\|off` | `combat.enemy.motion_cancel` (bool) | 6 | off |
| プレイヤーのひるみ | `--player-flinch on\|off` | `combat.player.flinch` (bool) | 7 | on |
| プレイヤーのスーパーアーマー | `--player-super-armor off\|attack\|always` | `combat.player.super_armor` (int32 0/1/2) | 8 | off |
| プレイヤーのモーションキャンセル | `--player-cancel on\|off` | `combat.player.motion_cancel` (bool) | 9 | on |
| ひるみの長さ | (`CombatSettings` の既定値) | `combat.<side>.flinch_seconds` (double 0..5) | — | 0.30 / 0.45 |

敵の設定はボスにも効く。`feel.super_armor` は授業の実演 (bool しか切り替えない) のための**敵のスーパーアーマー**の別名 (名前は `feel.` だが
演出の一覧には入らない): 読むと「off でない」、true を書くと off のときだけ attack にする、false を書くと off。範囲外の書き込み
(`super_armor` に 7、`flinch_seconds` に -1 や NaN) は適用せず、ログ (`--auto`) か画面の伝言 (`--play`) に「refused」と出す。そのため
この 2 つには `VarMeta` の min / max を付けていない (付けると `ergo_bind` が黙って範囲に丸めるため)。

### 何が起きたか (観測値) {#SPEC-ergo-lesson-game-observed}

すべて読み取り専用で、起動からの値 (R での遊び直しでも戻さない。授業の「済み」を外さないため)。演出の観測値は上の一覧。

| 変数 | 内容 |
|---|---|
| `combat.<side>.flinches` | その陣営がひるんだ回数 (咆哮で、を含む) |
| `combat.<side>.interrupted` | そのうち攻撃が止められた回数 |
| `combat.<side>.armor_absorbed` | スーパーアーマーで吸収した当たり (と咆哮) の数 |
| `combat.<side>.cancels` | その陣営が隙を打ち切った回数 |
| `game.wave` / `game.player_hp` / `game.boss_hp` (int32)、`game.defeated` / `game.player_hits` / `game.score` (int64) | 今のゲームの値 (遊び直すと戻る。ボスがいなければ `boss_hp` は 0) |

## 動かし方 {#SPEC-ergo-lesson-game-run}

```text
ergo_example_lesson_action_feel [--auto] [--status-every S] [common options]
ergo_example_lesson_action_feel --play [--endless | --autopilot] [--debug-vectors] [common options]
ergo_example_lesson_action_feel --check
ergo_example_lesson_action_feel --help | -h
common options: --host H --port P --no-studio --plain --all-effects --effect NAME=on|off --boss --seconds S
                and the combat options (--enemy-flinch on|off ... --knockback-dir away|backward)
```

- **`--auto` (既定、窓なし)**: 台本の操作 (`core/autopilot`: いちばん近い敵へ歩き、届けば攻撃し続ける。ボスが咆哮・ためを構えたら離れる)
  が終わらない波と戦う。60 Hz の実時間。切り替えのたび・Studio との接続の状態・`--status-every` 秒ごとに 3 行を標準出力に出す。
  負けたら記録して遊び直す。授業の課題の前半と CI のない確認はこれを使う。以前の「0.5 秒ごとに叩き続けるシミュレーション」はこのモードに
  置き換えた (実行ファイル名・`--host` `--port` `--plain` `--seconds` `--status-every` `--check` `--help` と `feel.hit_stop` /
  `feel.knockback` / `feel.camera_shake` とその観測値の名前と意味はそのまま。`--plain` は「演出を全部 OFF から」で、一覧が増えた分も切る)。

  ```text
  [lesson_action_feel] real=2.0s world=2.0s hits=5 defeated=1 wave=1 hp=100 hit_stop=OFF knockback=OFF camera_shake=OFF super_armor=off hit_stop_last_seconds=0.000 knockback_last_impulse=0.000 knockback_travel=0.00 shake=0.000
  [lesson_action_feel] combat enemy: flinches=4 interrupted=1 armor_absorbed=0 cancels=0 | player: flinches=0 interrupted=0 armor_absorbed=0 cancels=5
  [lesson_action_feel] fx all=OFF camera_shake_last_amplitude=0.000 hit_spark_count=0.000 damage_numbers_shown=0.000 hp_gauge_last_drain=0.000 telegraph_count=0.000 defeat_mark_count=0.000 boss_blur_last_peak=0.000 motion_blur_last_length=0.000 boss_hp=0
  ```

  `knockback_travel` は押されて敵が動いた距離の合計。
- **`--play`**: 端末で遊ぶ。`--autopilot` を付けると台本の操作が遊び (終わらない波、負けたら遊び直し)、キーは切り替えだけに使う
  (授業の実演用)。`--debug-vectors` でモーションベクターの表示から始める。どちらも Studio につなぎ、変数は `--auto` と同じものを出す。
- **`--check`**: Studio なしで固定の 60 Hz のフレームを回し、一覧の全部の観測値が OFF の間 0 であること、ON で起きること (ボスの
  ブラーはボスの波で)、敵がひるむこと、敵のスーパーアーマーで吸収が起きることを確かめる (ctest `example_lesson_action_feel`)。
- 知らないオプション・範囲外の値・値の無いオプション・知らない演出名・2 つのモード・`--play` なしの `--autopilot` / `--debug-vectors` は
  名指しで 2 で終わる。

## 端末で遊ぶ (`--play`) {#SPEC-ergo-lesson-game-terminal}

- キー: WASD / 矢印で移動、J / Space で攻撃、K で武器、1 2 3 で手触りの 3 つ、0 で演出を全部 (上)、O で演出の画面 (ゲームは止まり、
  a〜j で一覧の 1 つずつ、O で閉じる)、4 5 6 で敵の、7 8 9 で自分のひるみ・スーパーアーマー・モーションキャンセル、V でモーション
  ベクターの表示、P で一時停止、R で遊び直し、H でキーの説明の表示 / 非表示、Q か Ctrl+C で終わる。
- 端末はキーを離したことを送らない。方向キーは最後に押されてから 0.12 秒 (実時間) 歩き、押し続けるとキーボードの自動の繰り返しで
  歩き続ける (繰り返しが始まるまで (たいてい 0.25〜0.5 秒) は一歩で止まる)。向きを変えるには一歩歩く。
- 固定の 60 Hz でゲームを進め、毎フレーム画面全体を描き直す (カーソルを左上に戻して上書きし、消さない)。80 文字 × 24 行に収まる
  (どの行も 80 文字で切る)。
- 画面 (上から): HUD 6 行 (体力のバー・武器・波・倒した数 / 得点・連続・`time` (実時間)・`world` (世界時間)・`HIT STOP`・
  `BOSS ROAR` / `BOSS CHARGE`・`blur 0.62` / 手触りの 3 つと一覧の ON の数 / 陣営ごとの設定と、ひるんだ・吸収した・打ち切った回数の表)、
  アリーナ 16 行、伝言 1 行、キーの説明 1 行 (V の間はモーションベクターの数値の行)。
- 記号: `@` 自分 (ひるみ中は `&`、止まっているときは向きを `> < ^ v`)、`+` 自分の振り、`e` 敵 (いつもアーマーなら `E`)、`B` ボス、
  `!` 構え・振り中の敵、`.` 構えている敵の届く範囲 (telegraph)、`:` 敵の振り、`~` ひるんだ敵、`x` 倒れた敵 (defeat_mark)、敵の上の数字は体力、
  `*` 火花 (hit_spark)、`-20` ダメージの数字 (damage_numbers)、体力のバーの `=` が減っていく分 (hp_gauge)。
- **ボスのブラーの代わり** (`terminal/screen_effects.cpp` の `boss_blur_glyph`。端末は画素をぼかせないので、ぼかしの代わりの漫画の
  効果線を引く): 背景 (空白) だけに、ボスを中心にした線 `- | / \` を描く。Radial (咆哮) はボスから外へ `strength × 16` 単位まで、
  Zoom (ため) は外から内へ `16 × (1 − strength)` 単位まで。0.5 未満は 8 本、0.5 以上は 16 本。0.05 未満は描かない。体・数字・HUD は
  覆わないので読める。
- **モーションブラーの代わり** (`trail_cells`): 体の後ろ、軌跡に沿った背景のセルに、近い方から `%` `;` `,` (薄くなっていく)。どの物の
  記号とも重ならない字を使う。**セルの縦横比**: 端末のセルは縦が横の約 2 倍なので、世界の 1 単位を 2 列 × 1 行で描く。軌跡の
  ベクター (単位) を列 = x × 2、行 = y にしてから、長い方の軸のセル数だけ等間隔に置く (斜めの動きは画面でも斜め)。半セル未満は描かない。
- **モーションベクターの表示** (V、`--debug-vectors`、`vector_cells`): 各物のベクターを 10 フレーム分に伸ばして、体から前へ
  `- | / \`、先に `> < ^ v`。最下行に `motion u/frame: you(+0.15,+0.00)  hit#7(+0.20,+0.00)  cam(+0.00,+0.00)`。
- raw 端末 (POSIX の termios): エコー・行バッファ・シグナルのキーを切り (Ctrl+C はバイトとして届き Q と同じ)、読みは待たない。
  終わるとき (Q、`--seconds`、SIGTERM / SIGHUP / SIGINT、`exit()`) に元の設定とカーソルを戻す。端末でない標準入出力で `--play` を
  付けると、そう言って 2 で終わる。
- **Windows**: 端末の front-end は作っていない (`ReadConsoleInput` と仮想端末の出力が要り、ここでは動かせないので)。configure が
  「--play (terminal front-end) is not available on Windows」と出し、`--play` は起動時にそう言って 2 で終わる
  (`terminal/raw_terminal_unsupported.cpp`)。`--auto` と `--check` は Windows でも同じに動く見込み (未確認、コンパイルもしていない)。

## 窓のある front-end の継ぎ目 {#SPEC-ergo-lesson-game-seam}

Pictor (Vulkan + GLFW) の窓で描く front-end はこの作業の範囲外 (この環境に無い)。作るときに満たすこと:

1. 毎フレーム、実時間の `dt` と `PlayerCommand` (歩く向き・向く向き・攻撃・武器替え) を作り、
   `GameSession::frame(dt, command, toggles, settings, ergo::world_time::Engine::instance())` を 1 回呼ぶ。固定の 1/60 秒で呼ぶと、
   `--auto` と試験と同じ決定的な進み方になる。
2. 描くのは `GameSession::view(settings, toggles)` が返す `GameView` だけ (読み取り専用)。core の状態には触らない。
   `camera_offset` だけ視点をずらし、`frozen` で止まりを、`FighterView` の `action` / `phase` / `armored_now` / `spark` / `telegraph` /
   `visible` / `reach` / `arc_cos` で構え・振り・ひるみ・アーマー・火花・予告・倒れた敵を、`popups` でダメージの数字を、
   `player_hp_trail` で体力のバーの減り方を描く。スイッチが OFF の演出は、`GameView` の該当の値がもう空 / 0 になっている。
3. **ボスのブラー**: `GameView::blur` の `strength` (0〜1) と `centre` (世界の位置 → 画面へ写す) で、後処理のブラーを掛ける。
   Radial は中心から外への放射状のブラー、Zoom は中心へのズームブラーで、どちらもサンプル数か半径を `strength` に比例させる。HUD は掛けない。
4. **モーションブラー**: `FighterView::motion` (単位/フレーム) を画面の画素/フレームに写して、物ごとの速度バッファ (velocity buffer) に
   書き、それに沿ってサンプルする後処理のモーションブラーを掛ける。長さは `motion_streak` と同じシャッター・頭打ち・閾値に合わせ、
   `feel.motion_blur` が OFF なら掛けない。カメラの揺れのずれは速度バッファに入れない (core と同じ決まり)。ヒットストップ中は
   ベクターが 0 なので、何もぼけない。端末の `% ; ,` の軌跡はこの代わり。
   Ergo のリポジトリ (include / src / spec / tools) を `motion vector` / `velocity buffer` / `motion_blur` で探したが、Pictor 向けの
   速度バッファやモーションブラーの経路は見つからなかった (2026-11-12)。Pictor 本体はこの環境に無いので、そちらにあるかは未確認。
   既存の API の名前は挙げられない。
5. スイッチは `FeelToggles` / `CombatSettings` を持ち、`LessonBindings` で公開し、`apply_pending_writes()` を毎フレーム呼ぶ。
   キーで変えるなら `terminal/key_bindings.cpp` の `apply_setting` / `toggle_effect` と同じ値を変える。
6. 端末に固有のもの (キーを離したことが分からない補い、ANSI、文字の格子、効果線と `% ; ,`) は core に入れない。窓ならキーの押下と
   離しが分かるので `MoveLatch` は要らない (`ergo_input` のキーボードから `Intent` を作る)。

## 検証 {#SPEC-ergo-lesson-game-verify}

この Linux (2026-11-11〜12)、`cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy`
(`-DERGO_WITH_CURL=ON` と OFF の両方):

- `tests/test_combat_rules.cpp`: 歩く・攻撃の相・押した攻撃の記憶、ダメージと威力の役割、演出を切るとダメージだけ、ノックバックの方向の
  2 つの決まり、振りの届く範囲と扇、ひるみ on / off、アーマー attack / always、キャンセルは当たった攻撃の隙だけ、ひるみは打ち切れない。
- `tests/test_boss.cpp`: ボスの順番 (咆哮・ため・ため)、届く距離での咆哮と待ち時間、咆哮はひるみ off でもすくませアーマーには吸収される、
  咆哮自体のアーマー、ため攻撃が止まるかの表 (ハンマー / 剣 / 敵のアーマー / 敵のひるみ off)、放った一撃 (40・撃力 54・0.375 秒)、
  重いボスは 1/4 しか押されない、ボスの波と勝ち。
- `tests/test_action_game.cpp`: ヒットストップが 威力 ÷ 24 秒止め実時間は進む、止まっている間はひるみの残りも止まる、ノックバックの観測と
  移動、倒すと次の波、台本の操作が 3 つの波とボスを倒して勝つ、立っているだけだと負ける、endless は終わらない、各概念の場面
  (構えの止まる / 止まらない、キャンセルで当たる数が増える、プレイヤーのひるみ、敵の続けての振り、プレイヤーのアーマー)、同じ入力で同じゲーム。
- `tests/test_presentation.cpp`: 一覧の整合 (順序・名前・ゲームを変えるのは 2 つだけ)、**全部 OFF の間は一覧の全観測値が 0、全部 ON で
  全部が起きる**、**見た目の演出を 1 つずつ・全部まとめて切ってもシミュレーションの状態が同じ**、**全部 ON と全部 OFF で同じ結果**
  (どちらも勝ち、倒した数・波が同じ、最初の敵は 5 発目で倒れる、当たり 1 回ごとの判定も同じ)、揺れは止まりの間も実時間で減る、ボスの
  ブラーの包絡 (行動なしで 0、ための間は単調に増えて 0.7、放った瞬間 1.0、止まったままでも減る、0 に戻る、咆哮は Radial で 0.6 まで、
  OFF なら 0 で観測もしない、決定的、ボス戦で両方の種類と 1.0 が出る)、モーションベクター (最初は 0、歩くと 0.15/フレーム、ノックバックで
  大きくなり軌跡は後ろ、ヒットストップ中は 0、遊び直しと出現で跳ばない、決定的)、軌跡の長さ・向き・閾値・頭打ち・比例。
- `tests/test_wave_plan.cpp`、`tests/test_terminal_input.cpp` (キー、演出の画面のキーが一覧の全部に届く、0 の全部切り替え)、
  `tests/test_terminal_screen.cpp` / `test_terminal_effects.cpp` (決まった `GameView` から期待どおりの行: アリーナ、揺れ、予告、火花、
  アーマーと倒れた記号、HUD、80 × 24、効果線の弱い / 強い / Zoom、軌跡、デバッグのベクター、ダメージの数字、体力のバー、演出の画面)、
  `tests/test_options.cpp` (**`--plain` で一覧の全部が OFF** (戦闘の決まりは既定のまま)、`--effect`・`--all-effects`・`--boss`・
  `--debug-vectors`、全オプションとエラー)。
- 件数: `test_lesson_action_feel` 58、`test_lesson_action_feel_frontend` 30、`--check` ok、ctest 全 119 件が通る (CURL ON / OFF)。
  ASan + UBSan (`ci-asan` と同じフラグ、Debug) でも同じ 3 つが通り、`--play` を擬似端末で 14 秒遊んでも報告なし。
- 擬似端末での実走 (Python の `pty`、100 × 30): 画面を読んで動く小さな操作役が `@` と最寄りの敵を見つけて方向キーと J を送る
  (`--play --plain`、9 秒で 1 2 3 0、18 秒で 5、27 秒で 7、31 秒で 6、40 秒で 5 5)。3 つの波は倒したが、構えを避けない操作役は
  ボスのため攻撃に倒された (46 秒、GAME OVER)。構えを見て離れる台本の操作 (`--autopilot`) はボスに当たらずに勝つ (試験と実走)。
  剣のヒットストップは 60 Hz の描画で 5 フレーム、ハンマーは 15 フレーム、敵の爪は 7 フレーム `HIT STOP` が続いた。
  `--autopilot --boss` の実走で、咆哮の Radial の線 (blur 0.59)、ための Zoom の線 (0.70)、放った瞬間の 1.00、その後の実時間での減り
  (0.2 秒で 0.61) を画面で見た。`--autopilot` + ハンマーの実走で、モーションブラー ON の 12 秒に 45 フレームで軌跡、OFF の 5.7 秒で 0、
  ヒットストップ中のフレーム (計 278) で軌跡 0、歩くだけのフレームで軌跡 0、V でベクターの線と数値の行を確かめた。
- Studio との実接続 (`ergo_studio --headless`、Node 22 の WebSocket、`NO_PROXY=127.0.0.1,localhost`): `/variable/ws` からの set で
  観測値が変わること、`feel.super_armor` の別名、範囲外の書き込みの拒否 (lesson.md の「端末から使う」に記録)。
- 授業のサーバとの通し (`odeum-lesson`、Odeum の改訂した `action-game.plot.json`、非既定のポート、CURL ON の Studio): 生徒として課題の
  自動の 3 手順 (`add-hitstop`・`add-knockback`・`super-armor`) にチェックが入り、実演として `effects` (= `feel.all`) の ON / OFF で一覧の
  10 個がそろって切り替わり、敵のひるみ、スーパーアーマー、プレイヤーのキャンセル、カメラの節の揺れ・ボスのブラー・モーションブラーの
  合図がゲームの変数に反映された (`applied`)。
- 確認していないこと: 人が実際に遊んだ手触り (擬似端末の操作役と画面の記録で見ただけ)、Windows (コンソールの front-end は無く、
  `raw_terminal_unsupported.cpp` も `--auto` もコンパイルしていない)、macOS の端末、SSH 越しの描画の速さ、窓のある front-end と本物の
  ブラーの後処理、80 × 24 より小さい端末 (崩れる)。
