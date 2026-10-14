#include "ergo/package_watch/watch_diff.h"

#include <algorithm>

namespace ergo::package_watch {

namespace {

const StateEntry* find_entry(const std::vector<StateEntry>& entries, const std::string& dependency,
                             UpdateKind kind, const std::string& subject) {
    for (const auto& e : entries) {
        if (e.update.dependency == dependency && e.update.kind == kind && e.update.subject == subject) return &e;
    }
    return nullptr;
}

/// 同じ依存の track の既知 (主体を問わない。 畳み込みの起点)。
const StateEntry* find_track(const std::vector<StateEntry>& entries, const std::string& dependency) {
    for (const auto& e : entries) {
        if (e.update.dependency == dependency && e.update.kind == UpdateKind::Track) return &e;
    }
    return nullptr;
}

std::vector<Candidate> sorted_updates(std::vector<Candidate> updates) {
    std::sort(updates.begin(), updates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.kind != b.kind) return static_cast<int>(a.kind) < static_cast<int>(b.kind);
        return a.subject < b.subject;
    });
    return updates;
}

void keep_previous(const WatchState& previous, const std::string& dependency, WatchState& next) {
    if (const auto fp = previous.fingerprints.find(dependency); fp != previous.fingerprints.end()) {
        next.fingerprints[dependency] = fp->second;
    }
    for (const auto& e : previous.entries) {
        if (e.update.dependency == dependency) next.entries.push_back(e);
    }
}

void judge(const WatchState& previous, const std::string& host, const DependencyResult& result,
           std::int64_t now, DiffResult& out) {
    const std::string& name = result.dependency.name;
    out.next.fingerprints[name] = result.observation.fingerprint;
    const StateEntry* prev_track = find_track(previous.entries, name);

    for (const auto& c : sorted_updates(result.updates)) {
        if (const auto* known = find_entry(previous.entries, name, c.kind, c.subject)) {
            StateEntry e = *known;
            e.update     = c;  // 距離などは今回の値。 時刻は前回のまま
            out.next.entries.push_back(std::move(e));
            continue;
        }
        const bool is_track = c.kind == UpdateKind::Track;
        if (is_track && prev_track && now - prev_track->last_notified < kTrackFoldSeconds) {
            // 畳む: 主体は新しい先頭に替え、 最後の通知の時刻と距離を引き継ぐ。
            out.next.entries.push_back({c, now, prev_track->last_notified, prev_track->notified_behind});
            continue;
        }
        StateEntry e{c, now, now, is_track ? c.behind : std::nullopt};
        WatchEvent ev;
        ev.id         = event_id(host, name, c.kind, c.subject);
        ev.host       = host;
        ev.dependency = result.dependency;
        ev.update     = c;
        ev.as_of      = result.observation.as_of;
        ev.at         = now;
        if (is_track && prev_track && prev_track->notified_behind && c.behind) {
            ev.since_notified = *c.behind - *prev_track->notified_behind;
        }
        out.next.entries.push_back(std::move(e));
        out.events.push_back(std::move(ev));
    }
}

} // namespace

DependencyResult make_result(const WatchDependency& dep, const Observation& obs) {
    return {dep, obs, detect_updates(dep, obs)};
}

std::string event_id(const std::string& host, const std::string& dependency, UpdateKind kind,
                     const std::string& subject) {
    return host + "/" + dependency + "/" + update_kind_name(kind) + "/" + subject;
}

DiffResult diff(const WatchState& previous, const std::string& host,
                const std::vector<DependencyResult>& results, std::int64_t now) {
    std::vector<const DependencyResult*> ordered;
    for (const auto& r : results) ordered.push_back(&r);
    std::stable_sort(ordered.begin(), ordered.end(), [](const DependencyResult* a, const DependencyResult* b) {
        return a->dependency.name < b->dependency.name;
    });

    DiffResult out;
    out.next.host = host;
    std::string last;
    for (const auto* r : ordered) {
        if (!last.empty() && r->dependency.name == last) continue;  // 同じ名前は最初の 1 つだけ
        last = r->dependency.name;
        if (!r->observation.observed) {
            keep_previous(previous, r->dependency.name, out.next);
            continue;
        }
        judge(previous, host, *r, now, out);
    }
    sort_entries(out.next.entries);
    return out;
}

} // namespace ergo::package_watch
