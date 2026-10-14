#include "ergo/package_watch/detect.h"

namespace ergo::package_watch {

namespace {

/// 追跡ブランチの先頭へ上げる操作。 update が名前で受けるのは main だけなので、 他のブランチは SHA で渡す。
std::string next_for_branch(const WatchDependency& dep, const std::string& head) {
    const std::string to = dep.track_branch == "main" ? std::string("main") : head;
    return "ergo_pkg update " + dep.name + " --to " + to;
}

Candidate base(const WatchDependency& dep, UpdateKind kind) {
    Candidate c;
    c.dependency = dep.name;
    c.kind       = kind;
    c.from_rev   = dep.rev;
    c.from_tag   = dep.expect_tag;
    return c;
}

} // namespace

const char* update_kind_name(UpdateKind kind) {
    switch (kind) {
    case UpdateKind::Tag:     return "tag";
    case UpdateKind::Track:   return "track";
    case UpdateKind::OffMain: return "off-main";
    case UpdateKind::Run:     return "run";
    case UpdateKind::Review:  return "review";
    case UpdateKind::Merged:  return "merged";
    }
    return "tag";
}

bool parse_update_kind(const std::string& text, UpdateKind& out) {
    for (const UpdateKind k : {UpdateKind::Tag, UpdateKind::Track, UpdateKind::OffMain, UpdateKind::Run,
                               UpdateKind::Review, UpdateKind::Merged}) {
        if (text == update_kind_name(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

bool is_provenance_kind(UpdateKind kind) {
    return kind == UpdateKind::Run || kind == UpdateKind::Review || kind == UpdateKind::Merged;
}

std::vector<Candidate> detect_updates(const WatchDependency& dep, const Observation& obs) {
    std::vector<Candidate> out;
    if (!obs.observed) return out;

    if (obs.latest_tag) {
        Candidate c  = base(dep, UpdateKind::Tag);
        c.subject    = obs.latest_tag->name;
        c.to_rev     = obs.latest_tag->rev;
        c.to_tag     = obs.latest_tag->name;
        c.behind     = obs.latest_tag->distance;
        c.strict_ok  = true;
        c.next       = "ergo_pkg update " + dep.name + " --to " + obs.latest_tag->name;
        out.push_back(std::move(c));
    }
    if (obs.track_head.empty() || obs.track_head == dep.rev) return out;

    if (obs.pin_on_track.has_value() && !*obs.pin_on_track) {
        Candidate c = base(dep, UpdateKind::OffMain);
        c.subject   = dep.rev;
        c.to_rev    = obs.track_head;
        c.warning   = true;
        c.next      = next_for_branch(dep, obs.track_head);
        out.push_back(std::move(c));
        return out;
    }
    // 祖先判定ができない (ミラーが無い) ときも先頭が動いたことは出す。 距離は空欄。
    Candidate c = base(dep, UpdateKind::Track);
    c.subject   = obs.track_head;
    c.to_rev    = obs.track_head;
    c.behind    = obs.behind;
    c.next      = next_for_branch(dep, obs.track_head);
    out.push_back(std::move(c));
    return out;
}

} // namespace ergo::package_watch
