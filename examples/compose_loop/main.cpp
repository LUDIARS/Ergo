/// compose_loop — headless reference for wiring Ergo modules into a host's own
/// per-frame loop. Game-agnostic and render-free (no Pictor, no Vulkan, no
/// window). The numbered steps in the loop match the per-frame table in
/// spec/setup/integrate-modules.md.
///
/// A scripted 120-frame round stands in for a game: the host "presses" keys
/// (OS events -> inject*), a character walks forward, taps are judged against
/// a rhythm chart, one hit costs HP and triggers a hit-stop, and a HUD reads
/// score / combo / HP from the blackboard by name only.
///
///   ergo_example_compose_loop [--check]   120 fixed-dt frames, verify the end state (default)
///   ergo_example_compose_loop --connect [host] [port]
///                                         real time until Ctrl+C; edit player.speed in
///                                         tools/ergo http://localhost:5170/variable/
///   ... --trace <file>                    write an ergo_profile Chrome trace on exit
///   ... --verbose                         log threshold Debug (default: Warn)

#include "ergo/actor/actor.h"
#include "ergo/audio/audio_engine.h"
#include "ergo/bind/bind.h"
#include "ergo/blackboard/blackboard.h"
#include "ergo/cast/scene.h"
#include "ergo/character/controls/move_control.h"
#include "ergo/character/transform_brain.h"
#include "ergo/combo_counter/combo_counter.h"
#include "ergo/frame/frame.h"
#include "ergo/health/health.h"
#include "ergo/input/input_system.h"
#include "ergo/log/log.h"
#include "ergo/profile/profile.h"
#include "ergo/score/score.h"
#include "ergo/timing_judge/timing_judge.h"
#include "ergo/world_time/world_time.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace bb = ergo::blackboard;
namespace ch = ergo::character;
namespace tj = ergo::timing_judge;
using ergo::input::KeyCode;

// ---- Scripted round (frame indices are 0-based within a round) -------------
constexpr int          kRoundFrames    = 120;
constexpr float        kFixedDt        = 1.0f / 60.0f;  // real_dt under --check
constexpr int          kWalkFrom       = 10;            // forward key down
constexpr int          kWalkUntil      = 90;            // forward key up
constexpr int          kHitFrame       = 70;            // the character takes a hit
constexpr std::int32_t kHitDamage      = 20;
constexpr float        kHitStopSeconds = 0.04f;         // 2 frozen frames at 60 Hz
constexpr std::int64_t kNotesMs[]      = {250, 500, 750, 1000, 1250, 1500, 1750};
constexpr int          kTapFrames[]    = {15, 31, 48, 66, 75, 105};  // 1500 ms is missed

// Expected end of one --check round, derived from the script above:
//   walk : frames 10..89 = 80, minus the 2 hit-stop frames = 78 world frames
//          x 4 u/s x 1/60 s                                       -> z = 5.2
//   hp   : 100 - 20 = 80; regen 4/s heals 1 HP per 15 world frames, at
//          world frames 75 / 90 / 105 after the hit               -> 83
//   lane : taps at 0 / +17 / +50 / +100 / 0 / 0 ms and one miss
//          -> PERFECT x4, GREAT, GOOD, MISS. The hit (frame 70) and the miss
//          break the combo; with combo_factor 0.25 the score is
//          300*1.25 + 300*1.5 + 200*1.75 + 100*2 + 300*1.25 + 300*1.25 -> 2125
constexpr float        kExpectZ       = 5.2f;
constexpr std::int32_t kExpectHp      = 83;
constexpr std::int64_t kExpectScore   = 2125;
constexpr std::int32_t kExpectCombo   = 1;
constexpr std::int32_t kExpectPeak    = 4;
constexpr int          kExpectFrozen  = 2;
constexpr int          kExpectTally[] = {4, 1, 1, 1};  // PERFECT / GREAT / GOOD / MISS

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

/// The example's own status lines (stdout, always shown). Module diagnostics
/// go through ergo_log, whose threshold is Warn here.
void report(const char* fmt, ...) {
    std::printf("[compose_loop F%06llu] ",
                static_cast<unsigned long long>(ergo::frame::count()));
    std::va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
    std::fflush(stdout);
}

/// Host-side fan-out for the single-slot callbacks of Health / ComboCounter /
/// Score (`set_on_*` keeps one handler): install one FanOut per event and add
/// as many sinks as needed.
template <class... Args>
class FanOut {
public:
    void add(std::function<void(Args...)> sink) { sinks_.push_back(std::move(sink)); }
    void operator()(Args... args) const {
        for (const auto& sink : sinks_) sink(args...);
    }

private:
    std::vector<std::function<void(Args...)>> sinks_;
};

/// The controllable character: an Actor (live-tunable variables, per-actor
/// time scale) that owns a TCC-style brain and an HP component.
class Player : public ergo::actor::Actor {
public:
    Player() : Actor("player"), brain(&pose) {
        brain.add_move(&move);
        brain.add_turn(&move);
        ergo::bind::VarMeta meta;
        meta.min  = 0.0;
        meta.max  = 20.0;
        meta.step = 0.5;
        meta.unit = "u/s";
        bind_var("speed", &speed, meta);   // "player.speed" in /variable/
    }

    /// world_dt x this actor's time_scale ("player.time_scale") drives both
    /// the brain and the HP regen.
    void tick(float world_dt) {
        const float dt = scaled_dt(world_dt);
        move.set_speed(speed);
        brain.update(dt);
        hp.tick(dt);
    }

    ch::Pose             pose;
    ch::TransformBrain   brain;
    ch::MoveControl      move;
    ergo::health::Health hp{ergo::health::Config{100, 4.0f, true}};
    float                speed = 4.0f;
};

/// A fixed chart judged against REAL time since the round started. World
/// time is deliberately not used: a hit-stop must not shift judgments.
struct Lane {
    std::size_t next = 0;
    tj::Windows windows{};

    template <class OnJudge>
    void update(std::int64_t now_ms, bool tapped, OnJudge&& on_judge) {
        const std::size_t count = std::size(kNotesMs);
        while (next < count && now_ms > kNotesMs[next] + windows.good_ms) {
            on_judge(tj::Judgment::Miss);   // window passed without a tap
            ++next;
        }
        if (!tapped || next >= count) return;
        const tj::Judgment j = tj::judge(kNotesMs[next], now_ms, windows);
        if (j == tj::Judgment::Miss) return;   // far too early: ignore the tap
        on_judge(j);
        ++next;
    }
};

std::int64_t points(tj::Judgment j) {
    switch (j) {
        case tj::Judgment::Perfect: return 300;
        case tj::Judgment::Great:   return 200;
        case tj::Judgment::Good:    return 100;
        case tj::Judgment::Miss:    return 0;
    }
    return 0;
}

/// 3. Host OS events -> inject*. ergo_input's poll() is a no-op, so a host
/// forwards its own key events (Win32 / evdev / SDL ...); here a script
/// replays them: walk forward, and tap Space on the chart.
void feed_os_events(ergo::input::KeyboardDevice& kb, int f) {
    if (f == kWalkFrom)  kb.injectKeyState(KeyCode::W, true);
    if (f == kWalkUntil) kb.injectKeyState(KeyCode::W, false);
    for (int tap : kTapFrames) {
        if (f == tap)     kb.injectKeyState(KeyCode::Space, true);
        if (f == tap + 1) kb.injectKeyState(KeyCode::Space, false);
    }
}

/// Keys -> stick-style input::Vec2f (x = right, y = forward). A gamepad's
/// leftStick() yields the same shape.
ergo::input::Vec2f read_move_stick(const ergo::input::KeyboardDevice& kb) {
    const auto axis = [&kb](KeyCode pos, KeyCode neg) {
        return (kb.isKeyDown(pos) ? 1.0f : 0.0f) - (kb.isKeyDown(neg) ? 1.0f : 0.0f);
    };
    return {axis(KeyCode::D, KeyCode::A), axis(KeyCode::W, KeyCode::S)};
}

/// input::Vec2f -> character::Vec3 on the XZ ground plane (Y up, Z+ forward =
/// yaw 0). MoveControl takes 0..1 stick values, so clamp the length: two keys
/// at once would otherwise give sqrt(2).
ch::Vec3 to_ground_xz(ergo::input::Vec2f stick) {
    const ch::Vec3 v{stick.x, 0.0f, stick.y};
    const float    len = v.length_xz();
    return len > 1.0f ? v * (1.0f / len) : v;
}

/// Producer side: gameplay owns these Properties and registers them by name.
struct GameplayFeed {
    bb::Property<std::int64_t> score{0};
    bb::Property<std::int32_t> combo{0};
    bb::Property<std::int32_t> hp{0};

    void publish(bb::Engine& board) {
        board.register_property("score", &score, "stage");
        board.register_property("combo", &combo, "stage");
        board.register_property("hp",    &hp,    "stage");
    }
};

/// Consumer side (presentation): knows names and value types only and never
/// includes the gameplay structs. Its subscriptions sit in category "hud" and
/// close by themselves when the "stage" registrations go away.
struct Hud {
    std::int64_t                  score = 0;
    std::int32_t                  combo = 0;
    std::int32_t                  hp    = 0;
    std::vector<bb::Subscription> subs;

    void attach(bb::Engine& board) {
        if (const auto* p = board.find<std::int32_t>("hp")) hp = p->get();  // initial pull
        subs.push_back(board.subscribe<std::int64_t>(
            "score", [this](const std::int64_t& v) { score = v; }, "hud"));
        subs.push_back(board.subscribe<std::int32_t>(
            "combo", [this](const std::int32_t& v) { combo = v; }, "hud"));
        subs.push_back(board.subscribe<std::int32_t>(
            "hp", [this](const std::int32_t& v) { hp = v; }, "hud"));
    }

    bool attached() const {
        return std::any_of(subs.begin(), subs.end(),
                           [](const bb::Subscription& s) { return s.active(); });
    }
};

struct Options {
    bool          check   = false;   // --check (the default mode)
    bool          live    = false;   // --connect
    std::string   host    = "127.0.0.1";
    std::uint16_t port    = 5170;
    std::string   trace;             // --trace <file>
    bool          verbose = false;
};

bool parse_options(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto has_value = [&] { return i + 1 < argc && argv[i + 1][0] != '-'; };
        if (a == "--check") {
            o.check = true;
        } else if (a == "--connect") {
            o.live = true;
            if (has_value()) o.host = argv[++i];
            if (has_value()) {
                char*      end  = nullptr;
                const long port = std::strtol(argv[++i], &end, 10);
                if (*end != '\0' || port <= 0 || port > 65535) return false;
                o.port = static_cast<std::uint16_t>(port);
            }
        } else if (a == "--trace" && has_value()) {
            o.trace = argv[++i];
        } else if (a == "--verbose") {
            o.verbose = true;
        } else {
            return false;
        }
    }
    return !(o.check && o.live);
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_options(argc, argv, opt)) {
        std::fprintf(stderr,
                     "usage: %s [--check | --connect [host] [port]] [--trace <file>] [--verbose]\n",
                     argv[0]);
        return 2;
    }

    // ---- Setup (once) -----------------------------------------------------
    // ergo_input logs every key at Info; stay quiet unless asked.
    ergo::log::set_level(opt.verbose ? ergo::log::Level::Debug : ergo::log::Level::Warn);
    ergo::log::set_frame_provider(&ergo::frame::count);
    ergo::profile::set_enabled(!opt.trace.empty());
    ergo::profile::begin_session();
    ERGO_PROFILE_THREAD("main");
    if (!ergo::audio::initialize()) {
        ERGO_LOG_WARN("audio: initialize failed, continuing without sound");
    }

    ergo::input::InputSystem input;
    ergo::input::InputConfig input_cfg;
    input_cfg.enableMouse   = false;
    input_cfg.enableGamepad = false;
    input.initialize(input_cfg);
    ergo::input::KeyboardDevice& kb = *input.keyboard();
    kb.setConnected(true);   // the host reports the devices it found

    auto& world_time = ergo::world_time::Engine::instance();
    auto& board      = bb::Engine::instance();
    auto& bind       = ergo::bind::Engine::instance();

    Player            player;
    ergo::cast::Scene stage("stage");
    stage.add(&player);
    stage.activate();

    ergo::combo_counter::ComboCounter combo;
    ergo::score::Score score{ergo::score::Config{true, 0.25f, 0.0f}};
    Lane lane;
    int  tally[4]      = {};
    int  frozen_frames = 0;

    GameplayFeed feed;
    feed.hp.set(player.hp.hp());
    feed.publish(board);
    Hud hud;
    hud.attach(board);

    FanOut<std::int32_t, std::int32_t> on_damage;
    on_damage.add([&](std::int32_t, std::int32_t) { combo.break_(); });
    on_damage.add([&](std::int32_t, std::int32_t) { world_time.hit_stop(kHitStopSeconds); });
    on_damage.add([&](std::int32_t, std::int32_t after) { feed.hp.set(after); });
    on_damage.add([](std::int32_t amount, std::int32_t after) {
        ERGO_LOG_DEBUG("hit: -%d -> hp %d", amount, after);
    });
    player.hp.set_on_damage([&](std::int32_t amount, std::int32_t after) { on_damage(amount, after); });
    player.hp.set_on_heal([&](std::int32_t, std::int32_t after) { feed.hp.set(after); });
    combo.set_on_change([&](std::int32_t n) { feed.combo.set(n); });
    score.set_on_change([&](std::int64_t s) { feed.score.set(s); });

    const auto on_judge = [&](tj::Judgment j) {
        ++tally[static_cast<int>(j)];
        ERGO_LOG_DEBUG("judge: %s", tj::name(j));
        if (tj::breaks_combo(j, tj::Judgment::Good)) {
            combo.break_();
            return;
        }
        combo.hit();
        score.add(points(j), combo.count());
    };

    double round_time_s = 0.0;   // real seconds since the round started
    const auto start_round = [&] {
        lane = Lane{};
        combo.reset();
        score.reset();
        player.hp.revive();
        feed.hp.set(player.hp.hp());
        player.brain.warp(ch::Vec3{}, 0.0f);
        round_time_s = 0.0;
    };

    if (opt.live) {
        bind.set_app_name("compose_loop");
        bind.connect(opt.host, opt.port);
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        report("live: edit player.speed at http://%s:%u/variable/ (Ctrl+C to stop)",
               opt.host.c_str(), static_cast<unsigned>(opt.port));
    }

    // ---- Per-frame loop ---------------------------------------------------
    using Clock = std::chrono::steady_clock;
    Clock::time_point next_tick = Clock::now();
    float seen_speed     = player.speed;
    int   seen_connected = -1;   // live mode: report the link state on change

    for (int i = 0; opt.live ? g_stop == 0 : i < kRoundFrames; ++i) {
        const int f = i % kRoundFrames;   // live mode replays the round
        if (f == 0) start_round();
        {
            ERGO_PROFILE_SCOPE("frame");

            // 1. Frame counter (log prefix; live mode also reads the real dt).
            ergo::frame::tick();
            const float real_dt =
                opt.live ? std::min(ergo::frame::dt_seconds(), 0.1f) : kFixedDt;

            // 2. Apply edits queued by tools/ergo, at the start of the frame.
            bind.apply_pending_writes();
            if (player.speed != seen_speed) {
                report("player.speed %.2f -> %.2f", seen_speed, player.speed);
                seen_speed = player.speed;
            }
            if (opt.live && static_cast<int>(bind.is_connected()) != seen_connected) {
                seen_connected = static_cast<int>(bind.is_connected());
                report(seen_connected ? "connected to tools/ergo"
                                      : "waiting for tools/ergo (npm run dev, same PORT)");
            }

            // 3. Host OS events -> inject*.
            feed_os_events(kb, f);

            // 4. Latch input for this frame (poll + double-buffer swap).
            input.beginFrame();

            // 5. World time: hit-stop / hit-slow scale the simulation dt.
            const float world_dt = world_time.update(real_dt);
            if (world_dt == 0.0f && real_dt > 0.0f) ++frozen_frames;

            // 6. Actors: input -> character; Actor::scaled_dt feeds brain + HP.
            player.move.set_move_input(to_ground_xz(read_move_stick(kb)));
            if (stage.is_active(&player)) player.tick(world_dt);

            // 7. Gameplay rules: judge -> combo -> score -> blackboard -> HUD;
            //    a hit breaks the combo and starts a hit-stop (see on_damage).
            {
                ERGO_PROFILE_SCOPE("gameplay");
                const auto now_ms = static_cast<std::int64_t>(std::llround(round_time_s * 1000.0));
                lane.update(now_ms, kb.isKeyPressed(KeyCode::Space), on_judge);
                if (f == kHitFrame) player.hp.apply_damage(kHitDamage);
            }

            // 8. Profile counters (markers are free when profiling is off).
            ERGO_PROFILE_COUNTER("player.z", player.pose.position.z);
            ERGO_PROFILE_COUNTER("score", score.score());

            // 9. Audio pump.
            ergo::audio::update();

            // 10. Close the input frame.
            input.endFrame();
            round_time_s += real_dt;
        }
        if (opt.live) {
            next_tick += std::chrono::microseconds(16667);
            const auto now = Clock::now();
            if (next_tick < now) next_tick = now;   // after a stall: do not burst
            else std::this_thread::sleep_until(next_tick);
        }
    }

    // ---- Shutdown ---------------------------------------------------------
    if (opt.live) bind.disconnect();

    // Unloading the stage releases the producer's category; the HUD's by-name
    // subscriptions (category "hud") are closed with it.
    board.release("stage");
    const bool hud_detached = !hud.attached();

    report("frames=%llu z=%.3f hp=%d score=%lld combo=%d peak=%d judged=%d/%d/%d/%d frozen=%d",
           static_cast<unsigned long long>(ergo::frame::count()), player.pose.position.z,
           player.hp.hp(), static_cast<long long>(score.score()), combo.count(), combo.peak(),
           tally[0], tally[1], tally[2], tally[3], frozen_frames);

    int        failures = 0;
    const auto expect   = [&](const char* what, long long got, long long want) {
        if (got == want) return;
        ERGO_LOG_ERROR("check: %s = %lld, expected %lld", what, got, want);
        ++failures;
    };
    if (!opt.live) {
        expect("frames", static_cast<long long>(ergo::frame::count()), kRoundFrames);
        expect("frozen frames", frozen_frames, kExpectFrozen);
        expect("hp", player.hp.hp(), kExpectHp);
        expect("score", score.score(), kExpectScore);
        expect("combo", combo.count(), kExpectCombo);
        expect("combo peak", combo.peak(), kExpectPeak);
        for (int j = 0; j < 4; ++j) {
            expect(tj::name(static_cast<tj::Judgment>(j)), tally[j], kExpectTally[j]);
        }
        expect("hud.score", hud.score, kExpectScore);
        expect("hud.combo", hud.combo, kExpectCombo);
        expect("hud.hp", hud.hp, kExpectHp);
        expect("hud detached by release(stage)", hud_detached ? 1 : 0, 1);
        if (std::fabs(player.pose.position.z - kExpectZ) > 1e-3f) {
            ERGO_LOG_ERROR("check: pose.z = %.6f, expected %.3f (+-1e-3)",
                           static_cast<double>(player.pose.position.z),
                           static_cast<double>(kExpectZ));
            ++failures;
        }
    }

    if (!opt.trace.empty()) {
        if (ergo::profile::dump(opt.trace)) {
            report("trace: %s (drop it on http://localhost:%u/profile/)", opt.trace.c_str(),
                   static_cast<unsigned>(opt.port));
        } else {
            ERGO_LOG_ERROR("trace: cannot write %s", opt.trace.c_str());
            ++failures;
        }
    }

    board.release("hud");
    input.shutdown();
    ergo::audio::shutdown();
    if (failures != 0) report("check FAILED (%d mismatches)", failures);
    return failures == 0 ? 0 : 1;
}
