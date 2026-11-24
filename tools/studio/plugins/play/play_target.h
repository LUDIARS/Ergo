#pragma once
/// A game the play plugin builds, runs and streams: one entry of a workspace
/// profile's "play" list (spec/feature/tool/play.md "プロフィールの play").
/// Commands come only from here (PLAY-INV-01); a request names the target.
#include "../common/json_util.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::play {

struct PlayCommand {
    std::string              cmd;
    std::vector<std::string> args;
    int                      timeout_sec = 0;   ///< 0 = none (run); build defaults to 3600
};

struct PlayButton {
    std::string label;
    std::string key;   ///< a key name of the remote play input (Odeum's table; checked when sent)
};

struct PlayTarget {
    std::string                name;
    std::string                label;
    std::string                profile;    ///< the workspace profile it came from
    std::filesystem::path      root;       ///< working directory of every command
    std::optional<PlayCommand> build;
    PlayCommand                run;
    std::string                window;     ///< exact title of the game's window
    int                        fps     = 30;
    long long                  bitrate = 4'000'000;
    std::vector<PlayButton>    buttons;
    std::string scene_app, scene_id;   ///< editor publisher and scene for an optional preview target
    std::string preview_mode;          ///< "driver" or "lighting"
    std::string progress_state;        ///< lighting variant supplied by the title
};

constexpr int       kMinFps = 5, kMaxFps = 60;
constexpr long long kMinBitrate = 250'000, kMaxBitrate = 20'000'000;
constexpr int       kMaxBuildSec = 6 * 3600;
constexpr std::size_t kMaxButtons = 24;

/// A profile's "play" value -> its targets. False (with `err`) when any entry
/// is unusable; the profile then offers none.
bool parse_play_targets(const JsonValue& list, const std::string& profile, const std::filesystem::path& root,
                        std::vector<PlayTarget>& out, std::string& err);

/// `[a-z0-9-]{1,40}`
bool is_valid_target_name(const std::string& name);

/// The shape of a key name (`[A-Za-z0-9]{1,16}`); whether it is a key is up to the input.
bool is_key_name(const std::string& name);

} // namespace ergo::studio::plugins::play
