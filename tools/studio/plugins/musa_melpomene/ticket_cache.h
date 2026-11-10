#pragma once
/// Ticket cache with expiry and scene / object / position lookups (port of
/// services/cache.rs). Not thread-safe; MelpomenePlugin holds the lock.
#include "melpomene_models.h"

#include <chrono>
#include <optional>

namespace ergo::studio::plugins::musa_melpomene {

struct CacheStats {
    std::size_t total = 0, open = 0, closed = 0;
    bool        is_expired = true;
};
JsonValue to_json_value(const CacheStats& s);

class TicketCache {
public:
    using Clock = std::chrono::steady_clock;

    explicit TicketCache(uint64_t duration_minutes) : duration_(std::chrono::minutes(duration_minutes)) {}

    bool is_expired(Clock::time_point now = Clock::now()) const;
    void update(std::vector<Ticket> tickets, Clock::time_point now = Clock::now());
    void clear();
    void set_duration(uint64_t minutes) { duration_ = std::chrono::minutes(minutes); }

    const std::vector<Ticket>& all() const { return tickets_; }
    std::vector<Ticket>        by_scene(const std::string& scene) const;
    std::vector<Ticket>        by_object_path(const std::string& path) const;
    /// Euclidean distance <= radius; tickets without a position never match.
    std::vector<Ticket>        near(const std::array<double, 3>& pos, double radius) const;
    std::optional<Ticket>      find_by_issue_number(uint64_t n) const;
    CacheStats                 stats(Clock::time_point now = Clock::now()) const;

private:
    std::vector<Ticket>              tickets_;
    std::optional<Clock::time_point> last_updated_;
    Clock::duration                  duration_;
};

} // namespace ergo::studio::plugins::musa_melpomene
