#pragma once
/// Settings of the "sonus" plugin (spec/feature/tool/sonus.md), read once from
/// the environment by the Studio's settings (tools/studio/studio_settings).
/// Plain C++17 and Sonus-free: it is read in every build, the plugin itself is
/// only registered in builds with Sonus.
///
/// Where the game is: ERGO_SONUS_HOST / ERGO_SONUS_PORT. The port has one
/// default, Sonus' own live link default (kSonusDefaultPort; sonus_link.cpp
/// checks it against Sonus at compile time); nothing else is tried.
#include <cstdint>
#include <functional>
#include <string>

namespace ergo::studio::plugins::sonus {

using EnvFn = std::function<std::string(const std::string&)>;

/// Sonus livelink::kDefaultPort (snd/format/livelink_protocol.hpp).
constexpr uint16_t kSonusDefaultPort = 27460;

struct SonusConfig {
    std::string host = "127.0.0.1";        ///< ERGO_SONUS_HOST: the game's live link address
    uint16_t    port = kSonusDefaultPort;  ///< ERGO_SONUS_PORT
    std::string project_dir;               ///< ERGO_SONUS_PROJECT: the Sonus project (bus names, mixer); "" = none
    std::string build_dir;                 ///< ERGO_SONUS_BUILD_DIR: build the project there (its build hash); "" = load only
    std::string recording_dir;             ///< ERGO_SONUS_RECORDING_DIR: where 保存 writes recordings; "" = saving is off
    int         tick_ms = 50;              ///< ERGO_SONUS_TICK_MS (10..1000): how often the plugin polls the session
};

/// Reads the variables above. A value that cannot be used sets `error` (and the
/// field keeps its default); `error` is "" otherwise.
SonusConfig load_sonus_config(const EnvFn& env, std::string& error);

} // namespace ergo::studio::plugins::sonus
