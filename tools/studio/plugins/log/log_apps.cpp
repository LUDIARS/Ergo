#include "log_apps.h"

#include <algorithm>

namespace ergo::studio::plugins::log {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

} // namespace

JsonValue to_json_value(const AppInfo& a) {
    return jobj({{"app", jstr(a.app)}, {"connected", jbool(a.connected)},
                 {"lines", jnum(static_cast<double>(a.lines))}, {"last_seq", jnum(static_cast<double>(a.last_seq))},
                 {"dropped", jnum(static_cast<double>(a.dropped))}});
}

std::string LogApps::claim(const std::string& requested, uint64_t owner) {
    std::string base = trim(requested);
    if (base.empty()) base = "anonymous";
    for (int n = 1;; ++n) {
        const std::string name = n == 1 ? base : base + "#" + std::to_string(n);
        auto it = apps_.find(name);
        if (it != apps_.end() && it->second.owner != 0) continue;
        if (it == apps_.end()) {
            it = apps_.emplace(name, App{LineRing(ring_capacity_)}).first;
            order_.push_back(name);
        }
        it->second.owner       = owner;
        it->second.last_active = ++tick_;
        evict_if_needed();
        return name;
    }
}

bool LogApps::release(const std::string& app, uint64_t owner) {
    const auto it = apps_.find(app);
    if (it == apps_.end() || it->second.owner != owner) return false;
    it->second.owner       = 0;
    it->second.last_active = ++tick_;
    return true;
}

std::vector<StoredLine> LogApps::ingest(const std::string& app, uint64_t dropped, const std::vector<LogLine>& lines) {
    std::vector<StoredLine> stored;
    const auto it = apps_.find(app);
    if (it == apps_.end()) return stored;
    App& a = it->second;
    const std::size_t skip = lines.size() > a.ring.capacity() ? lines.size() - a.ring.capacity() : 0;
    for (std::size_t i = skip; i < lines.size(); ++i) stored.push_back(a.ring.push(lines[i]));
    a.dropped += dropped;
    a.last_active = ++tick_;
    return stored;
}

std::vector<AppInfo> LogApps::list() const {
    std::vector<AppInfo> out;
    for (const auto& name : order_) {
        const App& a = apps_.at(name);
        out.push_back({name, a.owner != 0, a.ring.size(), a.ring.last_seq(), a.dropped});
    }
    return out;
}

std::optional<LogApps::Lines> LogApps::query(const std::string& app, const LineFilter& f, std::size_t limit) const {
    const auto it = apps_.find(app);
    if (it == apps_.end()) return std::nullopt;
    return Lines{app, it->second.ring.last_seq(), it->second.dropped, it->second.ring.query(f, limit)};
}

void LogApps::evict_if_needed() {
    while (apps_.size() > kMaxApps) {
        const std::string* victim = nullptr;
        for (const auto& name : order_) {
            const App& a = apps_.at(name);
            if (a.owner == 0 && (!victim || a.last_active < apps_.at(*victim).last_active)) victim = &name;
        }
        if (!victim) return;   // every app has a live engine
        const std::string gone = *victim;
        apps_.erase(gone);
        order_.erase(std::find(order_.begin(), order_.end(), gone));
    }
}

} // namespace ergo::studio::plugins::log
