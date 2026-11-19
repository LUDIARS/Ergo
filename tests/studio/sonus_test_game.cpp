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
    snd_system*        sys = nullptr;
    snd_result         bank_result = SND_OK;
    std::string        root;            // the file system root (kept alive for the config)
    std::vector<float> period;          // one device period, interleaved
    float              last_peak = 0.0f;
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
    auto impl = std::make_unique<Impl>();
    // Sonus' built-in file system rooted at the bank's directory (D-103): the bank is
    // loaded by its name relative to that root.
    const fs::path bank_path = fs::path(bank);
    impl->root               = bank_path.parent_path().string();

    snd_config cfg;
    snd_config_init(&cfg);
    cfg.output_kind            = SND_OUTPUT_MANUAL;   // Sonus' manual device (D-102): step() advances it
    cfg.file_system_root       = impl->root.c_str();
    cfg.livelink.enabled       = 1;
    cfg.livelink.port          = port;
    cfg.tuning.lod_down_blocks = 0;   // no load LOD: a busy test machine must not change the mix
    const snd_result r = snd_create(&cfg, nullptr, nullptr, &impl->sys);
    if (r != SND_OK) {
        impl->sys = nullptr;
        error     = "snd_create failed: " + std::to_string(r);
        return nullptr;
    }
    impl->period.assign(static_cast<std::size_t>(cfg.block_size) * (cfg.output_channels ? cfg.output_channels : 2u), 0.0f);
    snd_callbacks cb{};
    cb.user           = impl.get();
    cb.on_bank_loaded = &on_bank_loaded;
    snd_set_callbacks(impl->sys, &cb);
    std::unique_ptr<Game> g(new Game(std::move(impl)));
    const std::string     name = bank_path.filename().generic_string();
    const snd_bank_handle b    = snd_load_bank_async(g->impl_->sys, name.c_str());
    for (int i = 0; i < 20000; ++i) {
        g->step();
        const snd_bank_state st = snd_get_bank_state(g->impl_->sys, b);
        if (st == SND_BANK_STATE_LOADED) return g;
        if (st == SND_BANK_STATE_FAILED) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    error = "the bank did not load (state " + std::to_string(snd_get_bank_state(g->impl_->sys, b)) + ", result " +
            std::to_string(g->impl_->bank_result) + ")";
    return nullptr;
}

bool Game::play(const char* event) {
    const snd_playback_handle h = snd_play(impl_->sys, snd_hash_id(event), snd_emitter_handle{0}, nullptr);
    return h.value != 0;
}

void Game::step() {
    snd_update(impl_->sys);
    const uint32_t frames = static_cast<uint32_t>(impl_->period.size() / 2u);
    if (snd_advance_output(impl_->sys, 1, impl_->period.data(), frames) != SND_OK) return;
    float peak = 0.0f;
    for (float v : impl_->period) peak = std::fmax(peak, std::fabs(v));
    impl_->last_peak = peak;
}

float Game::master_peak() const {
    snd_meter m{};
    if (snd_get_meter(impl_->sys, SND_ID_NONE, &m) != SND_OK) return -1.0f;
    float peak = 0.0f;
    for (uint32_t c = 0; c < m.channels && c < SND_MAX_CHANNELS; ++c) peak = std::fmax(peak, m.peak[c]);
    return peak;
}

float Game::device_peak() const { return impl_->last_peak; }

uint16_t candidate_port(int attempt) {
    return static_cast<uint16_t>(30000 + (SONUS_TEST_GETPID() * 7 + attempt * 101 + 50) % 20000);
}

TempDir::TempDir(const std::string& name) {
    static int      counter = 0;
    std::error_code ec;
    const fs::path  p = fs::temp_directory_path(ec) /
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
