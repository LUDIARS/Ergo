#pragma once
/// What the sonus plugin shows (tab, /sonus/api/state): a copy of the Sonus
/// tool session's models, in plain C++17 types (no Sonus header), so the plugin,
/// its routes and its view stay C++17. Filled by SonusLink::shown().
#include <cstdint>
#include <string>
#include <vector>

namespace ergo::studio::plugins::sonus {

/// snd::tool::LinkState, same order.
enum class LinkState : uint8_t { Offline = 0, Connecting = 1, Handshake = 2, Online = 3, Rejected = 4 };
const char* link_state_name(LinkState s);   ///< "offline", "connecting", ...

/// Linear peak / RMS and their dB (floored at kFloorDb: JSON has no -inf).
constexpr float kFloorDb = -144.0f;
float to_db(float linear);

struct MeterShown {
    bool  present  = false;   ///< the game sent meters for this bus
    float peak     = 0.0f;    ///< linear, loudest channel
    float rms      = 0.0f;
    float peak_db  = kFloorDb;
    float rms_db   = kFloorDb;
    float hold_db  = kFloorDb;
    bool  clipped  = false;
    bool  stale    = false;   ///< no new meter for 500 ms (falling)
};

/// One bus of the project (mixer strip).
struct BusShown {
    int         index = 0;          ///< position in the project's bus list (tree order)
    std::string key;                ///< stable key (route argument)
    std::string name;
    uint32_t    id     = 0;         ///< runtime id
    int         depth  = 0;
    bool        master = false;
    float       fader_db     = 0.0f;   ///< kFloorDb = -inf
    float       bank_db      = 0.0f;   ///< the project's value
    float       effective_db = 0.0f;   ///< what the game is told (mute / solo applied)
    bool        muted      = false;
    bool        soloed     = false;
    bool        solo_muted = false;    ///< muted by another bus' solo
    MeterShown  meter;
};

/// A meter the game sends, with or without a project bus for it.
struct MeterRow {
    uint32_t    bus_id = 0;   ///< 0 = the output (master)
    std::string name;         ///< project bus name, "output" or the id in hex
    MeterShown  meter;
};

struct OverrideShown {
    std::string key;     ///< snd::tool::OverrideItem::key (route argument)
    std::string label;
    float       bank_value = 0.0f;
    float       value      = 0.0f;
    bool        muted      = false;
};

struct VoiceShown {
    uint32_t    event_id = 0;
    std::string bus;          ///< project bus name or the id in hex
    std::string state;        ///< playback state name
    std::string source;       ///< source kind name
    float       audibility = 0.0f;
    float       gain       = 0.0f;
    bool        is_virtual = false;
};

struct TimingShown {
    uint32_t samples = 0;
    uint64_t last_block = 0;
    float    last_render_us = 0, avg_render_us = 0, max_render_us = 0;
    float    last_logic_us = 0, avg_logic_us = 0;
    float    budget_us = 0, load_percent = 0;
    uint32_t lod = 0, late_chunks = 0, underruns = 0, over_budget = 0;
};

struct MemoryShown {
    std::string tag;
    uint64_t    used = 0, budget = 0;
};

struct LogShown {
    uint64_t    clock_sample = 0;
    std::string kind;
    uint32_t    id = 0, detail = 0;
};

struct RecordingShown {
    std::string state = "idle";   ///< snd::tool::RecordingStateName, lower case
    uint64_t    bytes = 0;
    int32_t     error = 0;        ///< snd_result of a refusal
    std::string error_name;
    bool        can_save = false; ///< Ready
};

struct SonusShown {
    // connection
    LinkState   state = LinkState::Offline;
    std::string target;           ///< "host:port" while open
    bool        mismatch = false; ///< the game's banks differ from the project (the Studio's values are sent)
    uint32_t    sessions = 0;
    uint32_t    sample_rate = 0, block_size = 0;
    uint32_t    round_trip_us = 0;
    // project
    bool                     project_loaded = false;
    std::string              project_name;
    std::vector<std::string> project_errors;
    // mixer
    std::vector<BusShown>      buses;
    std::size_t                undo_count = 0, redo_count = 0;
    uint32_t                   cells_pending = 0, cells_failed = 0, cells_no_answer = 0;
    std::vector<OverrideShown> overrides;
    // profiler
    TimingShown              timing;
    std::vector<MeterRow>    meters;
    uint32_t                 real_voices = 0;
    std::vector<VoiceShown>  voices;
    bool                     has_memory = false;
    std::vector<MemoryShown> memory;
    std::vector<LogShown>    log;   ///< newest last, at most kMaxLog
    uint32_t                 stalls = 0, lod_changes = 0;
    // recording
    RecordingShown recording;
    // session messages (newest last)
    std::vector<std::string> messages;
    uint64_t                 ticks = 0;
};

constexpr std::size_t kMaxLog = 64;

} // namespace ergo::studio::plugins::sonus
