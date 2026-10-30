# 複数モジュールを自分のフレームループにつなぐ

## 目的

`ergo_input` / `ergo_character` / `ergo_world_time` / `ergo_health` など、
単体では動くモジュールを **ホストの 1 本のフレームループ** に組み込むときの
呼び出し順・時間の種類・型の変換・コールバックのつなぎ方をまとめる。
モジュールごとの API は各定義書 (`spec/feature/module/<名>.md`) を正とし、
ここでは「組み合わせたときに決めること」だけを書く。

参照実装は [`examples/compose_loop/main.cpp`](../../examples/compose_loop/main.cpp)。
ループ内のコメント `1.` 〜 `10.` は下の表の番号と一致する。CI は
`ctest` の `example_compose_loop` でこのファイルをビルド・実行し、
決まった終状態になることを確かめている (ゲーム固有の名前・アセットは無く、
Pictor / Vulkan / ウィンドウも使わない)。

```bash
cmake -S . -B build
cmake --build build --target ergo_example_compose_loop
build/examples/ergo_example_compose_loop            # --check: 120 フレームを固定 dt で回して検証
build/examples/ergo_example_compose_loop --connect  # tools/ergo とつないで実時間で回す (Ctrl+C で終了)
build/examples/ergo_example_compose_loop --trace compose_loop.trace.json
```

(マルチコンフィグ生成器では `build/examples/Release/` の下に出る。)
`ERGO_BUILD_EXAMPLES` は Ergo がトップレベルのときだけ既定 ON で、
`add_subdirectory(<ergo>)` するホストでは既定でビルドされない
(`-DERGO_BUILD_EXAMPLES=ON` で明示すればビルドできる)。

## 1 フレームの順序

| # | モジュール | 呼び出し | フレーム内の位置 | dt の種類 | 単位 | スレッド |
|---|---|---|---|---|---|---|
| 1 | `ergo_frame` | `ergo::frame::tick()` | 先頭 | 実時間を計測 (`frame::dt_seconds()`) | 秒 | メイン |
| 2 | `ergo_bind` | `bind::Engine::instance().apply_pending_writes()` | 先頭 (シミュレーションより前) | — | — | メイン (WS 受信は worker スレッド) |
| 3 | `ergo_input` | `keyboard()->injectKeyState(...)` などの `inject*` | `beginFrame` より前 | — | — | メイン (`ThreadMode::MainSync`) |
| 4 | `ergo_input` | `InputSystem::beginFrame()` | 入力を読む前 | — | — | メイン |
| 5 | `ergo_world_time` | `world_dt = world_time::Engine::instance().update(real_dt)` | シミュレーションの直前 | real → world | 秒 | メイン |
| 6 | `ergo_actor` / `ergo_character` / `ergo_health` | `dt = actor.scaled_dt(world_dt)` → `BrainBase::update(dt)` / `Health::tick(dt)` | シミュレーション | world × actor の `time_scale` | 秒 | メイン |
| 7 | `ergo_timing_judge` → `ergo_combo_counter` → `ergo_score` → `ergo_blackboard` | `judge(target_ms, now_ms, windows)` → `hit()` / `break_()` → `add(base, combo)` → `Property::set` | ゲームルール | 判定は **real** | ms (`int64_t`) | メイン |
| 8 | `ergo_profile` | `ERGO_PROFILE_SCOPE("...")` / `ERGO_PROFILE_COUNTER("...", v)` | 任意 | — | µs | 任意 (スレッドローカル記録) |
| 9 | `ergo_audio` | `ergo::audio::update()` | 末尾近く | — | — | メイン |
| 10 | `ergo_input` | `InputSystem::endFrame()` | 末尾 | — | — | メイン |

- `ergo_log` はどのスレッドからでも呼べる (行単位で排他)。行頭のフレーム番号は
  初期化時に `ergo::log::set_frame_provider(&ergo::frame::count)` でつなぐ。
- `ergo_blackboard` / `ergo_world_time` / `ergo_frame` は同期を持たないので
  メインスレッド専用として扱う。
- `ergo::cast::Scene` (`ergo_cast`) は毎フレーム呼ぶものではない。`activate()` /
  `deactivate()` で Actor 群をまとめて止め、ループ側は
  `stage.is_active(&actor)` を見て `tick` するかを決める。
- `apply_pending_writes()` はフレーム先頭で呼ぶ (`bind.md` のテスト項目
  「フレーム頭で呼べば…」)。前フレームまでに届いた tools/ergo の編集が、
  そのフレームのシミュレーションから効く。

## 時間の流れ: real_dt → world_time → Actor::scaled_dt

```
real_dt (ホストの時計 / frame::dt_seconds())
  │  ├─ UI アニメ・リズム判定・オーディオ同期 … real のまま使う
  ▼
world_dt = world_time::Engine::update(real_dt)   … HitStop 中は 0、HitSlow 中は縮む
  ▼
dt = actor.scaled_dt(world_dt)                    … Actor ごとの time_scale (0 で個別停止)
  ├─ BrainBase::update(dt)   (位置 += 速度 × dt)
  └─ Health::tick(dt)        (自然回復)
```

- `world_time::Engine::update` は **1 フレームに 1 回だけ** 呼ぶ。2 回呼ぶと
  HitStop の残り時間が 2 倍速で減る。
- リズム判定 (`timing_judge::judge`) には **world 時間を使わない**。
  HitStop が判定窓をずらしてしまうため、曲頭からの実時間 (ms) を渡す。
  例では `round_time_s += real_dt` を ms に丸めている (オーディオ再生位置が
  取れるホストはそちらを使う)。
- 実時間モードでは、デバッガ停止などで dt が跳ねるのを上限で抑える
  (例: `std::min(frame::dt_seconds(), 0.1f)`)。
- `Actor` は自分の `time_scale` を `"<actor>.time_scale"` として
  `ergo_bind` に公開するので、tools/ergo からアクター単位で止められる。

## OS イベント → `inject*` の橋渡し

`ergo_input` の `poll()` は現状どの OS でも空実装で、状態は `inject*` API で
入れる (`input.md`「プラットフォーム別タスク」)。ホストは自分のウィンドウ /
OS のイベント (Win32 メッセージ、evdev、SDL など) を受けたところで:

```cpp
// キー: OS のキーコードを ergo::input::KeyCode に写してから
input.keyboard()->injectKeyState(KeyCode::W, /*down=*/true);
// ゲームパッド: ボタンはビットマスク、軸は -1..1
input.gamepad()->injectButtonState(mask);
input.gamepad()->injectAxis(/*left x*/ 0, x);
```

- 注入は `beginFrame()` より前に行う。`beginFrame()` がダブルバッファを入れ替え、
  そのフレームの `isKeyDown` / `isKeyPressed` (立ち上がり) が確定する。
- デバイスは `setConnected(true)` するまで読み出しが常に false になる。
  見つけたデバイスはホストが接続済みにする。

## 型の変換

| 変換 | どうするか |
|---|---|
| `input::Vec2f` (x = 右, y = 前) → `character::Vec3` | **XZ 平面に写す**: `Vec3{s.x, 0, s.y}`。`MoveControl` は長さ 0..1 のスティック値を受けるので、長さが 1 を超えたら正規化する (WASD の斜めは √2 になる)。Z+ が yaw 0 |
| キー → スティック値 | `x = D − A`, `y = W − S`。ゲームパッドなら `gamepad()->leftStick()` がそのまま `Vec2f` |
| 秒 (`float`) → ms (`int64_t`) | 判定用の時刻は `std::llround(seconds * 1000.0)`。`timing_judge` の窓も ms |
| `ComboCounter::count()` (`int32_t`) → `Score::add(base, combo)` | そのまま渡す。倍率は `1 + combo × combo_factor` |
| `Health` / `Score` → blackboard | `Property<T>` の `T` は **登録・`find<T>`・`subscribe<T>` で同じ型** にする (`int` と `int64_t` は別型として扱われ、不一致は警告 + 無効) |
| bind 変数 | `BIND_VAR` / `Actor::bind_var` は `bool` / `int32_t` / `int64_t` / `float` / `double` / `std::string`。それ以外は `bind_accessor` で包む |

## 単一スロットのコールバックを複数に配る

`Health::set_on_damage` / `ComboCounter::set_on_change` / `Score::set_on_change`
などはハンドラを **1 つだけ** 保持する (後から set すると上書き)。複数の
システムが同じイベントに反応するときは、ホスト側で 1 つの fan-out を差し込む:

```cpp
template <class... Args>
class FanOut {
public:
    void add(std::function<void(Args...)> sink) { sinks_.push_back(std::move(sink)); }
    void operator()(Args... args) const { for (const auto& s : sinks_) s(args...); }
private:
    std::vector<std::function<void(Args...)>> sinks_;
};

FanOut<std::int32_t, std::int32_t> on_damage;
on_damage.add([&](std::int32_t, std::int32_t) { combo.break_(); });
on_damage.add([&](std::int32_t, std::int32_t) { world_time.hit_stop(0.04f); });
on_damage.add([&](std::int32_t, std::int32_t hp) { hp_prop.set(hp); });
health.set_on_damage([&](std::int32_t a, std::int32_t hp) { on_damage(a, hp); });
```

モジュール側に購読リストを足すのではなく、配る責務はホストに置く
(モジュールは単一責務のまま、どの組み合わせでも使える)。

## HUD への値の受け渡し (blackboard を名前で引く)

ゲームルール側 (producer) は自分の `Property<T>` を名前で登録し、表示側
(consumer) は **名前と型だけ** で購読する。表示側は producer の構造体を
include しない:

```cpp
// producer (category "stage")
board.register_property("score", &feed.score, "stage");
// consumer (category "hud") — 登録より後に購読する
auto sub = board.subscribe<std::int64_t>("score",
    [&](const std::int64_t& v) { hud.score = v; }, "hud");
```

- 未登録の名前・型違い・null コールバックは警告 + 無効な `Subscription`。
  後から登録しても自動では結ばれないので、producer の登録を先に済ませる。
- 名前で結んだ購読は **登録に紐づく**。producer が `release("stage")` /
  `unregister` / 同名の再登録をすると、HUD 側 (category "hud") の購読も
  Property が生きているうちに閉じる。HUD の `Subscription` が producer より
  長生きしても安全。
- `ergo_ui_layout` の `BindContext` に流す場合は、ホスト側に小さなアダプタを
  置く (blackboard と ui_layout はお互いを include しない)。名前と型が
  決まっていれば `find<T>` で足りる:

```cpp
void fill_hud(ergo::ui_layout::BindContext& ctx) {
    auto& board = ergo::blackboard::Engine::instance();
    using ergo::ui_layout::BindValue;
    if (const auto* hp = board.find<std::int32_t>("hp")) {
        ctx["hp"] = BindValue::from_number(hp->get());
    }
    if (const auto* score = board.find<std::int64_t>("score")) {
        ctx["score"] = BindValue::from_number(static_cast<double>(score->get()));
    }
}
// 毎フレーム (7. の後): fill_hud(ctx); document.update(ctx, real_dt);
```

登録をすべて写す `visit` 版は [`blackboard.md`](../feature/module/blackboard.md)
「利用例」にある。

## ログの閾値

例は `ergo::log::set_level(Level::Warn)` で始め、`--verbose` で `Debug` まで
出す (判定・被弾を Debug で出している)。毎フレーム近く出る Info / Debug を
既定で流すと、`--check` の出力や自分のログが埋もれるため。例自身の状態表示
(`player.speed 4.00 -> 6.50` や最後のまとめ行) は閾値に左右されないよう
stdout に直接出している。

## MSVC: `/utf-8`

いくつかのモジュールのヘッダは日本語コメントを含み、MSVC は BOM なし UTF-8 を
システムのコードページで読む (C4819 や文字化けによる構文エラーになる)。
モジュール側が `/utf-8` を PUBLIC で付けていればリンクで伝わるが、取り込み方に
依らないよう、Ergo のヘッダを include するホストのターゲットにも明示して付ける
(例の `examples/CMakeLists.txt` も同じ):

```cmake
if(MSVC)
    target_compile_options(mygame PRIVATE /utf-8)
endif()
```

## tools/ergo とつなぐ

- **変数を動かしながら直す**: `tools/ergo` を起動し (`npm run dev`、既定ポート
  5170)、`--connect [host] [port]` で例を実行する。`http://localhost:5170/variable/`
  に `player.speed` と `player.time_scale` が出るので値を変えると、例が
  `player.speed 4.00 -> 6.50` のように表示する。自分のホストでは
  `bind::Engine::instance().connect(host, port)` を初期化時に 1 回呼び、
  2. の `apply_pending_writes()` を毎フレーム呼ぶ。ポートは tools/ergo の
  `PORT` と合わせる ([run-tools-ergo.md](run-tools-ergo.md))。
- **性能を見る**: `--trace <file>` で終了時に `ergo::profile::dump(file)` した
  Chrome Trace を `http://localhost:5170/profile/` にドロップする。`frame` /
  `gameplay` のスコープと `player.z` / `score` のカウンタが見える。

手順を通しで追うなら [quickstart.md](quickstart.md)。
