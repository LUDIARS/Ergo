# 授業のサンプルゲーム (`examples/lesson_action_feel`) {#SPEC-ergo-lesson-game}

依頼 (2026-11-11): Odeum の授業「アクションゲームの作り方」(サンプル講座 `action-game`) で使う Ergo のサンプルを、
敵を倒して遊べるゲームにする。敵にスーパーアーマーを付けるかを選べるようにする。追加の依頼: モーションキャンセル・ひるみ・
スーパーアーマーを戦闘の概念として実装し、敵とプレイヤーそれぞれに設定で適用する。

授業との通信 (Studio の `lesson` プラグイン) は [lesson.md](lesson.md)。このファイルはゲームそのものの正本。
参考実装は [vtn-team/ActionGameSample](https://github.com/vtn-team/ActionGameSample) (Unity。2026-11-11 に commit
`a81d6b3` を読んだ。`Setting.cs` に演出の切り替え 6 つ、`HitCtrl.cs` はダメージで体力を減らし、威力でノックバックの撃力と
ヒットストップの長さ `威力 ÷ 24 秒` を決め、`Character.HitBack` は受けた側の後ろ向き (`-transform.forward`) に押す。音は無い)。

## 構成 {#SPEC-ergo-lesson-game-layout}

1 つのゲームの中身 (core) と、それを使う front-end の組。2 つの別々のサンプルにはしない。

| 置き場所 | 責務 | 層 (`.anatomia/layers.json`) |
|---|---|---|
| `core/` | ゲームの規則と状態。純粋 (I/O・時計・描画なし)、決定的 | domain-logic |
| `terminal/` | 端末の front-end の部品: キーの解読、キーの意味、移動の保持、画面の文字列、ANSI の出力、raw 端末 | presentation |
| 直下 | 実行ファイル: コマンドライン、`ergo_bind` への公開、切り替えの記録、3 つのモード | application |
| `tests/` | core と front-end の試験 | shared |

core が使う Ergo のモジュール: `ergo_world_time` (ヒットストップ)、`ergo_health` (体力)、`ergo_score` と
`ergo_combo_counter` (得点と連続ヒット)。`ergo_math` (C++20) と `ergo_physics2d` は使わない: サンプルは C++17 で
ビルドする (リポジトリの標準) ため、必要な 2D ベクトルは `core/vec2.h` に持つ。剛体の接触解決は要らない (体どうしは
重なった分を半分ずつ押し戻すだけ、壁は位置の制限) ので、ノックバックは「速さに撃力 ÷ 質量を足し、摩擦で減らす」で書く。
`ergo_input` は窓や OS のデバイス (キーボード・マウス・パッド) を読む作りで、端末の文字入力 (キーを離したことが届かない)
とは契約が合わないため、端末のキー読みはこの例の `terminal/` に置く。

## ルール {#SPEC-ergo-lesson-game-rules}

- アリーナ: 幅 30 × 高さ 12 (単位)。四方を壁で囲む (落ちる足場は無い)。見下ろし型。
- プレイヤー: 体力 100、速さ 9 単位/秒、半径 0.5。歩いた向きを向く。攻撃は向いている方へ出る。
- 武器 (K で持ち替え。攻撃の途中なら、終わって Ready になった時に替わる):

  | 武器 | ダメージ | 威力 | 届く距離 | 扇の半角 | 構え | 振り | 隙 |
  |---|---|---|---|---|---|---|---|
  | 剣 (sword) | 20 | 2 | 1.8 | 約 70° | 0.06 s | 0.10 s | 0.22 s |
  | ハンマー (hammer) | 35 | 6 | 2.2 | 90° | 0.28 s | 0.12 s | 0.42 s |
  | 敵の爪 (claw) | 12 | 3 | 1.4 | 60° | 0.55 s | 0.10 s | 0.50 s |

  ダメージは体力だけを減らす。威力はノックバックの撃力 (`威力 × 3`)、ヒットストップの長さ (`威力 ÷ 24 秒`、参考実装と同じ)、
  カメラの揺れ (`威力 × 0.15`、上限 1.0) を決める。
- 攻撃は 3 つの相: **構え** (当たらない。敵ではこれが予告)、**振り** (扇の中に入った相手に、1 回の攻撃につき 1 体 1 回当たる)、
  **隙** (当たらない。モーションキャンセルが無ければ最後まで動けない)。構えの前に押した攻撃は 0.15 秒 (世界時間) 覚えておく。
- 敵: 体力 100、速さ 3.5。プレイヤーへ歩き、届く距離に入ったら、待ち時間 (1.8 秒。最初は 0.6 秒 + 出現順 × 0.45 秒で
  ずらす) が明けていて、同時に構え・振りをしている敵が 2 体未満なら、プレイヤーを向いて爪を構える。構えた敵は `!` と、届く範囲が
  点で描かれる (見える攻撃)。
- 波: 2 体 → 3 体 → 4 体。全員を倒すと 1 秒 (世界時間) 後に次の波が、アリーナの縁の、プレイヤーから遠い点から出る。
  **3 つ目の波を倒しきると勝ち**、プレイヤーの体力が 0 で負け。`--endless` (と `--auto`・`--autopilot`) は波が終わらない
  (2, 3, 4, … 体、上限 6)。勝ち負けを付けたのは、授業の課題で「何体倒した」「勝てた」を目標にできるようにするため。
  ヘッドレスのモードと実演は終わらない方が授業の間ずっと数値が動くので endless にした。
- 倒された敵は `x` で 0.4 秒 (世界時間) 残ってから消える。
- 得点: 当てたダメージ + 倒すと 100。どちらも連続ヒットの倍率 (`1 + 連続数 × 0.1`、`ergo_score`) が掛かる。連続ヒット
  (`ergo_combo_counter`) は、プレイヤーが当てると増え、当てられるか 1.5 秒 (世界時間) 当てないと切れる。
- 同じフレームで両者の振りが当たったときは、プレイヤーの当たりを先に処理する。

## 手触りの 3 つ (`feel.*`) {#SPEC-ergo-lesson-game-feel}

3 つとも、どちらの陣営の当たりにも効く (参考実装の `HitCtrl` と同じ)。

- **ヒットストップ** (`feel.hit_stop`): 当たるたびに `ergo::world_time::Engine::hit_stop(威力 ÷ 24)` を呼ぶ。core は毎フレーム
  `world.update(実時間)` を 1 回だけ呼び、返った世界時間で**世界のもの** (移動、攻撃の相、ひるみ、敵の考え、ノックバック、
  波の待ち、連続ヒットの期限) を進める。止まっている間も**見る人の側** (実時間の時計 `time` / `real=`、カメラの揺れ、当たりの火花 `*`)
  は実時間で動く。止まっている間に押した攻撃は覚えておき、時間が戻ってから出る。新しい hit_stop は前の止めを上書きする
  (`ergo_world_time` の仕様)。
- **ノックバック** (`feel.knockback`): 当たった相手の速さに `方向 × 威力 × 3 ÷ 質量 (1)` を足す。速さは毎秒 `e^-7` で減る
  (剣なら約 0.86 単位動く)。壁で止まり、ほかの体にぶつかると押し合う。**方向の決まり** (`FeelTuning::knockback_direction`、
  `--knockback-dir`): 既定は `away` (攻撃した側 → 受けた側の向き。どこから当てても当てた側から遠ざかる)。`backward` は参考実装と
  同じ「受けた側の後ろ向き」で、背後から当てると攻撃した側へ寄ってくる。敵は歩いている間プレイヤーを向くので、`backward` との差は
  構えで向きが固まった敵の横や背後から当てたときに出る。授業で「向きの決め方」を考えさせるために両方を選べるようにした。
- **カメラの揺れ** (`feel.camera_shake`): 当たるたびに揺れの大きさを `min(威力 × 0.15, 1.0)` 以上にし、実時間で毎秒 `e^-6` で
  減らす。core は揺れのずれ (`GameView::camera_offset`、単位は世界の長さ) を出すだけで、描く側がその分だけ絵をずらす
  (端末では壁ごとアリーナを文字の単位でずらす。HUD は動かさない)。

観測値 (読み取り専用。課題の自動確認が見る): `feel.hit_stop_last_seconds` は `ergo_world_time` が実際に止まった最後の秒数
(`is_hit_stop()` で確かめてから記録)、`feel.knockback_last_impulse` は最後に与えた撃力。スイッチが OFF の間は 0 のまま、
ON にすると次の当たりで > 0 になり、OFF に戻しても最後の値は残る。

## 戦闘の三つの概念 {#SPEC-ergo-lesson-game-combat}

プレイヤーも敵も同じ型 (`Combatant`) と同じ行動の決まり (`core/combat_rules.cpp`) で動き、陣営ごとの設定 (`SideSettings`) だけが違う。
敵は `EnemyBrain` が、プレイヤーはキー・台本・自動操縦が、同じ `Intent` (歩く向き・向く向き・攻撃) を作る。

- **ひるみ** (`flinch`): 吸収されなかった当たりで、受けた側は `flinch_seconds` (既定: プレイヤー 0.30 秒、敵 0.45 秒、世界時間)
  の間ひるむ。ひるんでいる間は歩けない・向きを変えられない・攻撃できない (押した攻撃も捨てる)。出していた攻撃 (構え・振り・隙の
  どれでも) はそこで終わる (`interrupted`)。ひるみを OFF にした陣営は、当たっても体力が減るだけで行動は続く (ノックバックは別の
  スイッチなので押されはする)。
- **スーパーアーマー** (`super_armor`、`off` / `attack` / `always`): 付いている間に受けた当たりは**ダメージだけ**: ひるまない、
  行動が止まらない、**押し返されない**。ヒットストップとカメラの揺れは起きる (当てた側の手応えはそのまま。アクションゲームで
  アーマーの付いた相手を叩いても止まりと揺れは出るのが普通なので)。`attack` は自分の攻撃の構えと振りの間だけ (隙には付かない:
  振り終わりが反撃の機会)、`always` はいつも。付いているかは当たった瞬間の状態で決める。体力が 0 になればアーマーがあっても倒れる。
- **モーションキャンセル** (`motion_cancel`、on / off): **当たった攻撃の隙**を、新しい攻撃か移動で打ち切れる。打ち切れるのは
  隙の相だけで、構えと振りは打ち切れない。外した攻撃 (何にも当たらなかった攻撃) の隙も打ち切れない。off の陣営は構え → 振り →
  隙を最後まで出す。「いつでも打ち切れる」は隙を 0 にするのと同じで、設計の違いとして見せる意味が薄いので、当たったときだけの
  1 種類にした。敵は on のとき、当たった爪の隙を打ち切って、プレイヤーがまだ届く所にいれば 1 回だけ続けて振る
  (`enemy_chain_max`)。

**優先順位** (当たり 1 回の処理、`resolve_hit`):

1. ダメージは必ず入る。体力 0 → 倒れる (行動は終わる。ほかは見ない)。
2. スーパーアーマーが付いていれば → ダメージだけ (ひるまず、止まらず、押されない)。**アーマーはひるみより強い**。
3. そうでなく、ひるみが on → 行動が止まってひるむ。**モーションキャンセルの設定に関係なく止まる**。off → 行動は続く。
4. ノックバック (`feel.knockback` が ON で、アーマーに吸収されていなければ) で押す。
5. ヒットストップとカメラの揺れは、吸収されたかどうかに関係なく、それぞれのスイッチに従う。

**ひるみは打ち切れない** (ひるみの間はモーションキャンセルを受け付けない)。**時計**: 攻撃の相・ひるみ・押した攻撃の記憶・
敵の待ち時間はすべて世界時間なので、ヒットストップの間は止まる (ひるみが実時間では止まりの分だけ長く見える)。

既定は、初めて遊ぶときに分かりやすいもの: プレイヤーはひるみ on・アーマー off・キャンセル on (当てれば次がすぐ出せる)、
敵はひるみ on・アーマー off・キャンセル off (構えに当てれば止められ、振り切った後には隙がある)。

### 陣営ごとの設定の 3 つの入口 {#SPEC-ergo-lesson-game-settings}

| 設定 | 起動時 (CLI) | Studio の変数 | キー (`--play`) | 既定 |
|---|---|---|---|---|
| 敵のひるみ | `--enemy-flinch on\|off` | `combat.enemy.flinch` (bool) | 4 | on |
| 敵のスーパーアーマー | `--enemy-super-armor off\|attack\|always`、`--super-armor` (= attack) | `combat.enemy.super_armor` (int32 0/1/2)、`feel.super_armor` (bool) | 5 (off → attack → always → off) | off |
| 敵のモーションキャンセル | `--enemy-cancel on\|off` | `combat.enemy.motion_cancel` (bool) | 6 | off |
| プレイヤーのひるみ | `--player-flinch on\|off` | `combat.player.flinch` (bool) | 7 | on |
| プレイヤーのスーパーアーマー | `--player-super-armor off\|attack\|always` | `combat.player.super_armor` (int32 0/1/2) | 8 | off |
| プレイヤーのモーションキャンセル | `--player-cancel on\|off` | `combat.player.motion_cancel` (bool) | 9 | on |
| ひるみの長さ | (FeelTuning ではなく `CombatSettings` の既定値) | `combat.<side>.flinch_seconds` (double 0..5) | — | 0.30 / 0.45 |
| ヒットストップ / ノックバック / 揺れ | `--plain` で 3 つとも OFF から | `feel.hit_stop` / `feel.knockback` / `feel.camera_shake` | 1 / 2 / 3 | ON |

`feel.super_armor` は授業の実演 (bool しか切り替えない) のための**敵のスーパーアーマー**の別名: 読むと「off でない」、true を書くと
off のときだけ attack にする (attack / always のままなら変えない)、false を書くと off。範囲外の書き込み (`super_armor` に 7、
`flinch_seconds` に -1 や NaN) は適用せず、ログ (`--auto`) か画面の伝言 (`--play`) に「refused」と出す。そのため、この 2 つには
`VarMeta` の min / max を付けていない (付けると `ergo_bind` が黙って範囲に丸めるため)。

### 何が起きたか (観測値) {#SPEC-ergo-lesson-game-observed}

すべて読み取り専用で、起動からの合計 (R での遊び直しでも戻さない。授業の「済み」を外さないため)。

| 変数 | 内容 |
|---|---|
| `combat.<side>.flinches` | その陣営がひるんだ回数 |
| `combat.<side>.interrupted` | そのうち攻撃が止められた回数 |
| `combat.<side>.armor_absorbed` | スーパーアーマーで吸収した当たりの数 |
| `combat.<side>.cancels` | その陣営が隙を打ち切った回数 |
| `game.wave` / `game.player_hp` (int32)、`game.defeated` / `game.player_hits` / `game.score` (int64) | 今のゲームの値 (遊び直すと戻る) |

## 動かし方 {#SPEC-ergo-lesson-game-run}

```text
ergo_example_lesson_action_feel [--auto] [--host H] [--port P] [--no-studio] [--plain] [--seconds S] [--status-every S] [combat options]
ergo_example_lesson_action_feel --play [--endless | --autopilot] [--host H] [--port P] [--no-studio] [--plain] [--seconds S] [combat options]
ergo_example_lesson_action_feel --check
ergo_example_lesson_action_feel --help | -h
```

- **`--auto` (既定、窓なし)**: 台本の操作 (`core/autopilot`: いちばん近い敵へ歩き、届けば攻撃し続ける) が終わらない波と戦う。
  60 Hz の実時間。切り替えのたび・Studio との接続の状態・`--status-every` 秒ごとに 2 行を標準出力に出す。負けたら記録して遊び直す。
  授業の課題の前半と CI のない確認はこれを使う。以前の「0.5 秒ごとに叩き続けるシミュレーション」はこのモードに置き換えた
  (実行ファイル名・`--host` `--port` `--plain` `--seconds` `--status-every` `--check` `--help` と `feel.*` の名前と意味はそのまま)。

  ```text
  [lesson_action_feel] real=6.0s world=5.6s hits=10 defeated=2 wave=2 hp=100 hit_stop=ON knockback=ON camera_shake=OFF super_armor=off hit_stop_last_seconds=0.083 knockback_last_impulse=6.000 knockback_travel=0.85 shake=0.000
  [lesson_action_feel] combat enemy: flinches=8 interrupted=2 armor_absorbed=0 cancels=0 | player: flinches=0 interrupted=0 armor_absorbed=0 cancels=10
  ```

  `knockback_travel` は押されて敵が動いた距離の合計 (以前の `enemy_offset` の代わり)。
- **`--play`**: 端末で遊ぶ。`--autopilot` を付けると台本の操作が遊び (終わらない波、負けたら遊び直し)、キーは切り替えだけに使う
  (授業の実演用)。どちらも Studio につなぎ、変数は `--auto` と同じものを出す。
- **`--check`**: Studio なしで固定の 60 Hz のフレームを回し、観測値がスイッチに従うこと (OFF の間 0、ON で > 0)、敵がひるむこと、
  敵のスーパーアーマーで吸収が起きることを確かめる (ctest `example_lesson_action_feel`)。
- 知らないオプション・範囲外の値・値の無いオプション・2 つのモード・`--play` なしの `--autopilot` は名指しで 2 で終わる。

## 端末で遊ぶ (`--play`) {#SPEC-ergo-lesson-game-terminal}

- キー: WASD / 矢印で移動、J / Space で攻撃、K で武器、1 2 3 で手触りの 3 つ、4 5 6 で敵の、7 8 9 で自分のひるみ・スーパー
  アーマー・モーションキャンセル、P で一時停止、R で遊び直し、H でキーの説明の表示 / 非表示、Q か Ctrl+C で終わる。
- 端末はキーを離したことを送らない。方向キーは最後に押されてから 0.12 秒 (実時間) 歩き、押し続けるとキーボードの自動の繰り返しで
  歩き続ける (繰り返しが始まるまでの間 (たいてい 0.25〜0.5 秒) は一歩で止まる)。向きを変えるには一歩歩く。
- 固定の 60 Hz でゲームを進め、毎フレーム画面全体を描き直す (カーソルを左上に戻して上書きし、消さない)。80 文字 × 24 行に収まる。
- 画面 (上から): HUD 6 行 (体力・武器・波・倒した数 / 得点・連続・`time` (実時間)・`world` (世界時間)・止まっている間の
  `HIT STOP` / 手触りの 3 つ / 陣営ごとの設定と、ひるんだ・吸収した・打ち切った回数の表)、アリーナ 16 行、伝言 1 行 (最後の
  切り替え・波・Studio の接続)、キーの説明 1 行。
- 記号: `@` 自分 (ひるみ中は `&`、止まっているときは向きを `> < ^ v` で示す)、`+` 自分の振り、`e` 敵 (いつもアーマーなら `E`)、
  `!` 構え・振り中の敵 (構えの間は届く範囲を `.`、振りの間は `:` で描く。攻撃中のアーマーは記号では区別せず、表の absorbed で
  分かる)、`~` ひるんだ敵、`x` 倒れた敵、敵の上の数字は体力、`*` 当たりの火花 (実時間 0.15 秒。ヒットストップ中も見える)。
- raw 端末 (POSIX の termios): エコー・行バッファ・シグナルのキーを切り (Ctrl+C はバイトとして届き Q と同じ)、読みは待たない。
  終わるとき (Q、`--seconds`、SIGTERM / SIGHUP / SIGINT、`exit()`) に元の設定とカーソルを戻す。端末でない標準入出力
  (パイプ) で `--play` を付けると、そう言って 2 で終わる。
- **Windows**: 端末の front-end は作っていない (`ReadConsoleInput` と仮想端末の出力が要り、ここでは動かせないので)。
  configure が「--play (terminal front-end) is not available on Windows」と出し、`--play` は起動時にそう言って 2 で終わる
  (`terminal/raw_terminal_unsupported.cpp`)。`--auto` と `--check` は Windows でも同じに動く見込み (未確認)。

## 窓のある front-end の継ぎ目 {#SPEC-ergo-lesson-game-seam}

Pictor (Vulkan + GLFW) の窓で描く front-end はこの作業の範囲外 (この環境に無い)。作るときに満たすこと:

1. 毎フレーム、実時間の `dt` と `PlayerCommand` (歩く向き・向く向き・攻撃・武器替え) を作り、
   `ActionGame::frame(dt, command, toggles, settings, ergo::world_time::Engine::instance())` を 1 回呼ぶ。
   固定の 1/60 秒で呼ぶと、`--auto` と試験と同じ決定的な進み方になる。
2. 描くのは `ActionGame::view(settings)` が返す `GameView` だけ (読み取り専用)。core の状態には触らない。
   `camera_offset` だけ視点をずらし、`frozen` で止まりを、`FighterView` の `action` / `phase` / `armored_now` / `spark` で
   構え・振り・ひるみ・アーマー・火花を描く。`reach` と `arc_cos` で振りの範囲を描ける。
3. スイッチは `FeelToggles` / `CombatSettings` を持ち、`LessonBindings` で公開し、`apply_pending_writes()` を毎フレーム呼ぶ
   (Studio からの切り替えはここで入る)。キーで変えるなら `terminal/key_bindings.cpp` の `apply_setting` と同じ値を変える。
4. 端末に固有のもの (キーを離したことが分からない補い、ANSI、文字の格子) は core に入れない。窓ならキーの押下と離しが分かるので
   `MoveLatch` は要らない (`ergo_input` のキーボードから `Intent` を作る)。

## 検証 {#SPEC-ergo-lesson-game-verify}

この Linux (2026-11-11〜12)、`cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DERGO_BUILD_TESTS=ON -DERGO_AUDIO_BACKEND=dummy`:

- `tests/test_combat_rules.cpp`: 歩く・攻撃の相・押した攻撃の記憶、ダメージと威力の役割 (2 つの武器で体力・撃力・止まる秒数・揺れ)、
  手触りを切るとダメージだけ、ノックバックの方向の 2 つの決まり、振りの届く範囲と扇、ひるみ on で構えが止まる・off で続く (押されはする)、
  アーマー (attack) が構えへの当たりを吸収し押されず止まりと揺れは出る、attack は Ready と隙に付かない、always と倒れること、
  キャンセルは当たった攻撃の隙だけ・攻撃と移動へ、ひるみは打ち切れない。
- `tests/test_action_game.cpp`: OFF の間は観測値 0 で世界が止まらない、ヒットストップが 威力 ÷ 24 秒 (剣で 4〜5 フレーム) 止め
  実時間は進む、止まっている間はひるみの残りも止まる、揺れは止まりの間も実時間で減る、ノックバックの観測と移動、倒すと次の波、
  台本の操作が 3 つの波を倒して勝つ、立っているだけだと負ける、endless は終わらない、敵の構えはアーマーなしで止まりありで止まらない
  (そのとき爪が当たる)、プレイヤーのキャンセルで当たる数が増える、プレイヤーのひるみ on / off、敵のキャンセルの続けての振り、
  プレイヤーのアーマー、同じ入力で同じゲーム (位置・体力・得点・数え上げが一致)。`tests/test_wave_plan.cpp`: 波の数と出る位置。
- `tests/test_terminal_input.cpp` / `test_terminal_screen.cpp` / `test_options.cpp`: バイト → キー (矢印、分かれて届いた列、
  単独の ESC)、キーの表、設定キーが Studio と同じ値を変えること、移動の保持、決まった `GameView` から期待どおりの行 (アリーナ、
  揺れでのずれ、構えの予告、火花、アーマーと倒れた記号、HUD の 6 行)、既定のアリーナで 80 × 24 に収まること、
  コマンドラインの全オプションとエラー、切り替えの記録の文言。
- 件数: `test_lesson_action_feel` 34、`test_lesson_action_feel_frontend` 20、`ergo_example_lesson_action_feel --check` ok。
  ASan + UBSan (`ci-asan` と同じフラグ、Debug) でも同じ 3 つが通り、`--play` を擬似端末で 14 秒遊んでも報告なし。
- 擬似端末での実走 (Python の `pty`、100 × 30。台本は作業用の `pty_play.py`): 画面を読んで動く小さな操作役が、`@` と最寄りの
  敵を見つけて方向キーと J を送る。1 回目 (`--play --plain --no-studio`、9 秒で 1 2 3、18 秒で 5、27 秒で 7、31 秒で 6、36 秒で Q):
  27.6 秒で 3 つの波を倒して `YOU WIN` (`quit: wave 3, defeated 9, hits 45`)。剣のヒットストップは 60 Hz の描画で 5 フレーム
  続けて `HIT STOP` が出て世界が止まった (0.083 秒)。2 回目 (`--endless`、6 秒で 1 2 3、12 秒で 5、16 秒で K、19 秒で 7、22 秒で 6):
  ハンマーの当たりは 15 フレーム (0.25 秒)、敵の爪は 7 フレーム止まり、敵のアーマーを付けてからの 14 秒で HP が 88 から 40 まで
  減った (構えが止まらないため)。3 回目 (`--autopilot`、操作役なし、授業の実演と同じ順でキーを送る): 敵のひるみ OFF の 6 秒に
  enemy の flinched が増えず HP が 88 → 76、アーマー attack の 8 秒に absorbed 0 → 4 と HP 76 → 52、自分のキャンセル OFF の
  6 秒に cancelled が増えないことを画面の表で確かめた。
- Studio との実接続 (`ergo_studio --headless`、`PORT=5591`、Node 22 の WebSocket、`NO_PROXY=127.0.0.1,localhost`): `--plain` の
  `--auto` に `/variable/ws` から `feel.hit_stop` → `feel.hit_stop_last_seconds` が 0.0833、`feel.knockback` → 撃力 6、
  `feel.super_armor = true` → `combat.enemy.super_armor` が 1 になり `combat.enemy.armor_absorbed` が増えた。
  `combat.enemy.super_armor = 2` は反映、`= 7` は返事が無く値は 2 のまま (ログに refused。min / max を外す前は `ergo_bind` が
  黙って 2 に丸めていた)、`feel.super_armor = false` で 0。公開された変数は 27。
- 授業のサーバとの通し (`odeum-lesson`、Odeum の `native/lesson/courses/action-game.plot.json` の改訂版、非既定のポート、
  `ERGO_WITH_CURL=ON` の Studio): 生徒として課題の自動の 3 手順 (`add-hitstop`・`add-knockback`・`super-armor`) にチェックが入り、
  実演として開始の 3 つの ON、敵のひるみ OFF、アーマー ON (ひるみを戻す)、アーマー OFF とプレイヤーのキャンセル OFF、
  キャンセルを戻す、の各合図がゲームの変数に反映された (`applied`)。
- 確認していないこと: 人が実際に遊んだ手触り (擬似端末の操作役と画面の記録で見ただけ)、Windows (コンソールの front-end は
  無く、`raw_terminal_unsupported.cpp` と `--auto` もコンパイルしていない)、macOS の端末、SSH 越しの描画の速さ、窓のある
  front-end、80 × 24 より小さい端末 (崩れる。広げるように案内する)。
