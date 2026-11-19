#include "sonus_test_game.h"

#include "snd/sonus.h"
#include "snd/tool/project.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#define SONUS_TEST_GETPID _getpid
#else
#include <unistd.h>
#define SONUS_TEST_GETPID getpid
#endif

namespace sonus_test {
namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::string& text) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

void put32(std::string& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<char>((v >> (8 * i)) & 0xFFu));
}

/// Mono float32 WAV of `frames` samples of `value`.
std::string dc_wav(float value, uint32_t frames) {
    std::string data;
    uint32_t    bits;
    std::memcpy(&bits, &value, sizeof bits);
    for (uint32_t i = 0; i < frames; ++i) put32(data, bits);
    std::string b = "RIFF";
    put32(b, static_cast<uint32_t>(36 + data.size()));
    b += "WAVEfmt ";
    put32(b, 16);
    put32(b, 3u | (1u << 16));      // format 3 (float), 1 channel
    put32(b, 48000);
    put32(b, 48000 * 4);
    put32(b, 4u | (32u << 16));     // block align 4, 32 bits
    b += "data";
    put32(b, static_cast<uint32_t>(data.size()));
    return b + data;
}

std::string guid(unsigned n) {
    char b[48];
    std::snprintf(b, sizeof b, "7001c000-0000-4000-8000-%012x", n);
    return b;
}

std::string event_json(unsigned g, const char* name, const char* bus, int loops) {
    return "{\"type\": \"event\", \"guid\": \"" + guid(g) + "\", \"name\": \"" + name + "\", \"bank\": \"Main\", \"bus\": \"" +
           bus + "\", \"tracks\": [{\"clips\": [{\"asset\": \"dc\", \"loopCount\": " + std::to_string(loops) + "}]}]}";
}

// ---- the null device (snd/ext.h table), pumped by Game::step -------------------------
struct NullDevice {
    snd_device_callback cb       = nullptr;
    void*               cb_user  = nullptr;
    uint32_t            rate     = 48000, channels = 2, period = 256;
    bool                started  = false;
    std::vector<float>  buffer;
    float               last_peak = 0.0f;
};

int32_t SND_CALL dev_open(void* u, const snd_device_desc* d, snd_device_callback cb, void* cb_user) {
    auto* n = static_cast<NullDevice*>(u);
    if (cb == nullptr) return 0;
    if (d->sample_rate != 0u) n->rate = d->sample_rate;
    if (d->channels != 0u) n->channels = d->channels;
    if (d->period_frames != 0u) n->period = d->period_frames;
    n->cb      = cb;
    n->cb_user = cb_user;
    n->buffer.assign(static_cast<std::size_t>(n->period) * n->channels, 0.0f);
    return 1;
}
int32_t SND_CALL dev_start(void* u) {
    static_cast<NullDevice*>(u)->started = true;
    return 1;
}
void SND_CALL dev_stop(void* u) { static_cast<NullDevice*>(u)->started = false; }
void SND_CALL dev_close(void* u) {
    auto* n    = static_cast<NullDevice*>(u);
    n->started = false;
    n->cb      = nullptr;
}
uint32_t SND_CALL dev_period(void* u) { return static_cast<NullDevice*>(u)->period; }
uint32_t SND_CALL dev_rate(void* u) { return static_cast<NullDevice*>(u)->rate; }
uint32_t SND_CALL dev_channels(void* u) { return static_cast<NullDevice*>(u)->channels; }
double SND_CALL   dev_latency(void*) { return 0.0; }
void SND_CALL     dev_listener(void*, snd_device_event_fn, void*) {}

} // namespace

void write_mix_project(const std::string& dir) {
    const fs::path p = fs::path(dir);
    write_file(p / "project.json", "{\"type\": \"project\", \"guid\": \"" + guid(1) + "\", \"name\": \"Mix\", \"formatVersion\": 1}");
    write_file(p / "banks/Main.json", "{\"type\": \"bank\", \"guid\": \"" + guid(2) + "\", \"name\": \"Main\"}");
    write_file(p / "buses/Master.json", "{\"type\": \"bus\", \"guid\": \"" + guid(3) +
                                            "\", \"name\": \"Master\", \"bank\": \"Main\", \"master\": true, \"meter\": true}");
    write_file(p / "buses/Sfx.json", "{\"type\": \"bus\", \"guid\": \"" + guid(4) +
                                         "\", \"name\": \"Sfx\", \"bank\": \"Main\", \"parent\": \"Master\", \"meter\": true}");
    write_file(p / "buses/Music.json", "{\"type\": \"bus\", \"guid\": \"" + guid(5) +
                                           "\", \"name\": \"Music\", \"bank\": \"Main\", \"parent\": \"Master\", \"meter\": true}");
    write_file(p / "audio/dc.wav", dc_wav(kDc, 4800));
    write_file(p / "assets/dc.json", "{\"type\": \"asset\", \"guid\": \"" + guid(9) +
                                         "\", \"name\": \"dc\", \"bank\": \"Main\", \"file\": \"audio/dc.wav\", "
                                         "\"loadForm\": \"pcm\", \"convert\": {\"default\": {\"codec\": \"PCMF\"}}}");
    write_file(p / "events/Tone.json", event_json(10, "Tone", "Sfx", 0));
    write_file(p / "events/Pad.json", event_json(11, "Pad", "Music", 0));
    write_file(p / "events/Blip.json", event_json(12, "Blip", "Sfx", 1));
}

std::string build_project(const std::string& dir, const std::string& out, std::string& error) {
    snd::tool::Project project;
    if (!project.Load(dir) || !project.Build(out) || project.banks().empty()) {
        error = "the test project does not build:";
        for (const auto& i : project.issues()) error += " [" + i.where + " " + i.message + "]";
        return {};
    }
    return project.banks()[0].path;
}

struct Game::Impl {
    NullDevice        device;
    snd_output_device table{};
    snd_system*       sys = nullptr;
    snd_result        bank_result = SND_OK;
};

namespace {
void SND_CALL on_bank_loaded(void* user, snd_bank_handle, snd_result result) {
    static_cast<Game::Impl*>(user)->bank_result = result;
}
} // namespace

Game::Game(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Game::~Game() {
    if (impl_ && impl_->sys) snd_destroy(impl_->sys);
}

std::unique_ptr<Game> Game::start(const std::string& bank, uint16_t port, std::string& error) {
    auto impl                        = std::make_unique<Impl>();
    impl->table.user                 = &impl->device;
    impl->table.open                 = &dev_open;
    impl->table.start                = &dev_start;
    impl->table.stop                 = &dev_stop;
    impl->table.close                = &dev_close;
    impl->table.period_frames        = &dev_period;
    impl->table.sample_rate          = &dev_rate;
    impl->table.channels             = &dev_channels;
    impl->table.output_latency_seconds = &dev_latency;
    impl->table.set_event_listener   = &dev_listener;

    snd_config cfg;
    snd_config_init(&cfg);
    cfg.output.device        = &impl->table;
    cfg.livelink.enabled     = 1;
    cfg.livelink.port        = port;
    cfg.tuning.lod_down_blocks = 0;   // no load LOD: a busy test machine must not change the mix
    const snd_result r = snd_create(&cfg, nullptr, nullptr, &impl->sys);
    if (r != SND_OK) {
        impl->sys = nullptr;
        error     = "snd_create failed: " + std::to_string(r);
        return nullptr;
    }
    snd_callbacks cb{};
    cb.user           = impl.get();
    cb.on_bank_loaded = &on_bank_loaded;
    snd_set_callbacks(impl->sys, &cb);
    std::unique_ptr<Game> g(new Game(std::move(impl)));
    // snd_create's default file system is rooted at the working directory and takes
    // '/'-separated relative paths only.
    std::error_code   ec;
    const std::string rel = fs::relative(fs::path(bank), fs::current_path(ec), ec).generic_string();
    if (ec || rel.empty() || rel.rfind("..", 0) == 0) {
        error = "the bank " + bank + " is not under the working directory";
        return nullptr;
    }
    const snd_bank_handle b = snd_load_bank_async(g->impl_->sys, rel.c_str());
    const auto            end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    for (;;) {
        g->step();
        const snd_bank_state st = snd_get_bank_state(g->impl_->sys, b);
        if (st == SND_BANK_STATE_LOADED) break;
        if (st == SND_BANK_STATE_FAILED || std::chrono::steady_clock::now() > end) {
            error = "the bank did not load (state " + std::to_string(st) + ", result " +
                    std::to_string(g->impl_->bank_result) + ")";
            return nullptr;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return g;
}

bool Game::play(const char* event) {
    const snd_playback_handle h = snd_play(impl_->sys, snd_hash_id(event), snd_emitter_handle{0}, nullptr);
    return h.value != 0;
}

void Game::step() {
    snd_update(impl_->sys);
    NullDevice& d = impl_->device;
    if (!d.started || d.cb == nullptr) return;
    d.cb(d.cb_user, d.buffer.data(), d.period, d.channels);
    float peak = 0.0f;
    for (float v : d.buffer) peak = std::fmax(peak, std::fabs(v));
    d.last_peak = peak;
}

float Game::master_peak() const {
    snd_meter m{};
    if (snd_get_meter(impl_->sys, SND_ID_NONE, &m) != SND_OK) return -1.0f;
    float peak = 0.0f;
    for (uint32_t c = 0; c < m.channels && c < SND_MAX_CHANNELS; ++c) peak = std::fmax(peak, m.peak[c]);
    return peak;
}

float Game::device_peak() const { return impl_->device.last_peak; }

uint16_t candidate_port(int attempt) {
    return static_cast<uint16_t>(30000 + (SONUS_TEST_GETPID() * 7 + attempt * 101 + 50) % 20000);
}

TempDir::TempDir(const std::string& name) {
    static int      counter = 0;
    std::error_code ec;
    // Under the working directory, not the system temp: Sonus' default file system reads
    // banks by paths relative to it and refuses absolute ones (Game::start).
    const fs::path  p = fs::current_path(ec) /
                       ("ergo-studio-sonus-" + name + "-" + std::to_string(SONUS_TEST_GETPID()) + "-" + std::to_string(++counter));
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    root_ = p.string();
}

TempDir::~TempDir() {
    std::error_code ec;
    fs::remove_all(fs::path(root_), ec);
}

std::string TempDir::path(const std::string& sub) const {
    return sub.empty() ? root_ : (fs::path(root_) / fs::path(sub)).string();
}

} // namespace sonus_test
