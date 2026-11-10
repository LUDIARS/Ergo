#pragma once
/// Per-app rings of the "log" plugin, independent of sockets. An app keeps
/// its ring after its engine disconnects (a crashed run stays readable);
/// beyond kMaxApps the least recently active disconnected app is evicted.
#include "line_ring.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::log {

constexpr std::size_t kMaxApps    = 16;
constexpr std::size_t kMaxEngines = kMaxApps;

struct AppInfo {
    std::string app;
    bool        connected = false;
    std::size_t lines     = 0;
    uint64_t    last_seq  = 0;
    uint64_t    dropped   = 0;
};
JsonValue to_json_value(const AppInfo& a);

class LogApps {
public:
    explicit LogApps(std::size_t ring_capacity = kRingCapacity) : ring_capacity_(ring_capacity) {}

    /// Bind engine connection `owner` to an app named `requested` (blank =
    /// "anonymous"). A name held by another live engine gets a suffix
    /// (game#2, game#3, ...); a name whose engine left is taken over.
    std::string claim(const std::string& requested, uint64_t owner);
    /// The engine left. False when `owner` no longer feeds `app`.
    bool        release(const std::string& app, uint64_t owner);

    /// Store a batch (only its tail when larger than the ring). Returns the
    /// stored lines.
    std::vector<StoredLine> ingest(const std::string& app, uint64_t dropped, const std::vector<LogLine>& lines);

    std::vector<AppInfo>     list() const;      ///< first-seen order
    std::size_t              size() const { return order_.size(); }
    bool                     has(const std::string& app) const { return apps_.count(app) > 0; }
    std::vector<std::string> names() const { return order_; }
    /// Lines of one app plus its counters; nullopt for an unknown app.
    struct Lines {
        std::string             app;
        uint64_t                last_seq = 0;
        uint64_t                dropped  = 0;
        std::vector<StoredLine> lines;
    };
    std::optional<Lines> query(const std::string& app, const LineFilter& f = {}, std::size_t limit = SIZE_MAX) const;

private:
    struct App {
        LineRing ring;
        uint64_t owner       = 0;   // 0 = no live engine
        uint64_t dropped     = 0;
        uint64_t last_active = 0;
    };
    void evict_if_needed();

    std::size_t                ring_capacity_;
    std::map<std::string, App> apps_;
    std::vector<std::string>   order_;
    uint64_t                   tick_ = 0;
};

} // namespace ergo::studio::plugins::log
