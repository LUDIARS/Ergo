#include "ticket_cache.h"

#include <cmath>

namespace ergo::studio::plugins::musa_melpomene {

JsonValue to_json_value(const CacheStats& s) {
    return jobj({{"total", jnum(static_cast<double>(s.total))}, {"open", jnum(static_cast<double>(s.open))},
                 {"closed", jnum(static_cast<double>(s.closed))}, {"is_expired", jbool(s.is_expired)}});
}

bool TicketCache::is_expired(Clock::time_point now) const { return !last_updated_ || now - *last_updated_ > duration_; }

void TicketCache::update(std::vector<Ticket> tickets, Clock::time_point now) {
    tickets_      = std::move(tickets);
    last_updated_ = now;
}

void TicketCache::clear() {
    tickets_.clear();
    last_updated_.reset();
}

std::vector<Ticket> TicketCache::by_scene(const std::string& scene) const {
    std::vector<Ticket> out;
    for (const auto& t : tickets_) {
        if (t.scene_name && *t.scene_name == scene) out.push_back(t);
    }
    return out;
}

std::vector<Ticket> TicketCache::by_object_path(const std::string& path) const {
    std::vector<Ticket> out;
    for (const auto& t : tickets_) {
        if (t.object_path && *t.object_path == path) out.push_back(t);
    }
    return out;
}

std::vector<Ticket> TicketCache::near(const std::array<double, 3>& p, double radius) const {
    std::vector<Ticket> out;
    for (const auto& t : tickets_) {
        if (!t.world_position) continue;
        const auto& w = *t.world_position;
        const double dx = w[0] - p[0], dy = w[1] - p[1], dz = w[2] - p[2];
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= radius) out.push_back(t);
    }
    return out;
}

std::optional<Ticket> TicketCache::find_by_issue_number(uint64_t n) const {
    for (const auto& t : tickets_) {
        if (t.github_issue_number && *t.github_issue_number == n) return t;
    }
    return std::nullopt;
}

CacheStats TicketCache::stats(Clock::time_point now) const {
    CacheStats s;
    s.total = tickets_.size();
    for (const auto& t : tickets_) {
        if (t.state == "open") ++s.open;
        else if (t.state == "closed") ++s.closed;
    }
    s.is_expired = is_expired(now);
    return s;
}

} // namespace ergo::studio::plugins::musa_melpomene
