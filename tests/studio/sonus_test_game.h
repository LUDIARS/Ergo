#pragma once
// A Sonus game for the sonus plugin's tests, in the test's own process: Sonus' C API
// only (what an engine links), a null output device pumped from the test's loop, the
// live link on a loopback port, one bank loaded from disk. Plus the Sonus project the
// game's bank is built from (written by the test, built with the tool's project model).
#include <cstdint>
#include <memory>
#include <string>

namespace sonus_test {

/// Project "Mix", bank "Main" (like Sonus' tool tests, tool/test/tool_fixture.hpp):
///   buses  Master (master, meter) > Sfx (meter), Music (meter)
///   asset  "dc": mono 48 kHz float, 4800 frames of kDc
///   events Tone (Sfx, loops), Pad (Music, loops), Blip (Sfx, once)
/// Tone + Pad give a steady output of two equal halves; muting a bus halves it.
constexpr float kDc = 0.25f;
void write_mix_project(const std::string& dir);

/// Builds the project in `dir` into `out` (snd::tool::Project). The bank's path, or ""
/// with `error`.
std::string build_project(const std::string& dir, const std::string& out, std::string& error);

class Game {
public:
    /// The game with its live link on `port` and `bank` loaded; nullptr with `error`.
    static std::unique_ptr<Game> start(const std::string& bank, uint16_t port, std::string& error);
    ~Game();
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    bool  play(const char* event);   ///< snd_play by name
    void  step();                    ///< one game frame: snd_update + one device period
    float master_peak() const;       ///< the master meter (largest channel), as of the last step
    float device_peak() const;       ///< largest |sample| of the last period the device played

    struct Impl;

private:
    explicit Game(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

/// A candidate port near a per-process base (attempt 0, 1, ...).
uint16_t candidate_port(int attempt);

/// A fresh, empty directory under the working directory (removed by the destructor).
class TempDir {
public:
    explicit TempDir(const std::string& name);
    ~TempDir();
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    std::string path(const std::string& sub = std::string()) const;

private:
    std::string root_;
};

} // namespace sonus_test
