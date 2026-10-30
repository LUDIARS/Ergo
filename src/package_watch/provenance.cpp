#include "ergo/package_watch/provenance.h"

#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_watch/upm_dependency.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace ergo::package_watch {

const char* const kPraeformaTrigger = "praeforma";

const char* const kOriginPf      = "pf";
const char* const kOriginAi      = "ai";
const char* const kOriginHuman   = "human";
const char* const kOriginUnknown = "unknown";

const char* const kSourceOk          = "ok";
const char* const kSourceUnavailable = "unavailable";
const char* const kSourceOff         = "off";

namespace {

std::string short_sha(const std::string& sha) { return sha.substr(0, 7); }

const DelegationRun* find_run(const LocalPr& pr, const RunListing& runs) {
    if (!pr.session_id.empty()) {
        for (const auto& r : runs.runs) {
            if (r.child_session_id == pr.session_id) return &r;
        }
    }
    if (pr.head_ref.empty()) return nullptr;
    const std::string repo = repo_key(pr.repository);
    for (const auto& r : runs.runs) {
        if (r.spawn_branch == pr.head_ref && r.repository == repo) return &r;
    }
    return nullptr;
}

OriginResult origin_of_run(const DelegationRun& run) {
    if (run.triggered_by == kPraeformaTrigger) return {kOriginPf, ""};
    return {kOriginAi, run.triggered_by};
}

bool same_repo(const LocalPr& pr, const std::string& dep_key) {
    return !dep_key.empty() && repo_key(pr.repository) == dep_key;
}

void add_runs(const WatchDependency& dep, const std::string& key, const RunListing& runs,
              std::vector<Candidate>& out) {
    std::vector<const DelegationRun*> active;
    for (const auto& r : runs.runs) {
        if (r.repository == key && is_run_active(r.status)) active.push_back(&r);
    }
    std::sort(active.begin(), active.end(), [](const DelegationRun* a, const DelegationRun* b) { return a->id < b->id; });
    for (const auto* r : active) {
        Candidate c;
        c.dependency = dep.name;
        c.kind       = UpdateKind::Run;
        c.subject    = r->id;
        c.from_rev   = dep.rev;
        c.from_tag   = dep.expect_tag;
        const auto o = origin_of_run(*r);
        c.origin     = o.origin;
        c.trigger    = o.trigger;
        c.next       = "wait for the delegation run to submit a local PR";
        out.push_back(std::move(c));
    }
}

void add_reviews(const WatchDependency& dep, const std::string& key, const ReviewListing& reviews,
                 const RunListing* runs, std::vector<Candidate>& out) {
    std::vector<const LocalPr*> open;
    for (const auto& pr : reviews.open) {
        if (same_repo(pr, key) && pr.base_ref == dep.track_branch) open.push_back(&pr);
    }
    std::sort(open.begin(), open.end(), [](const LocalPr* a, const LocalPr* b) { return a->number < b->number; });
    for (const auto* pr : open) {
        Candidate c;
        c.dependency = dep.name;
        c.kind       = UpdateKind::Review;
        c.review     = pr->number;
        c.head       = short_sha(pr->head_sha);
        c.subject    = "#" + std::to_string(pr->number) + "@" + c.head;
        c.from_rev   = dep.rev;
        c.from_tag   = dep.expect_tag;
        const auto o = classify_review(*pr, runs);
        c.origin     = o.origin;
        c.trigger    = o.trigger;
        c.next       = "wait for local PR #" + std::to_string(pr->number) + " to merge (it comes back as a " +
                 dep.track_branch + " update)";
        out.push_back(std::move(c));
    }
}

void add_merged(const WatchDependency& dep, const std::string& key, const ReviewListing& reviews,
                const std::vector<Candidate>& previous, std::vector<Candidate>& out) {
    std::set<int> done;
    for (const auto& prev : previous) {
        if (!prev.review || done.count(*prev.review)) continue;
        const auto it = std::find_if(reviews.merged.begin(), reviews.merged.end(), [&](const LocalPr& pr) {
            return pr.number == *prev.review && same_repo(pr, key);
        });
        if (it == reviews.merged.end()) continue;
        done.insert(*prev.review);
        Candidate c;
        c.dependency = dep.name;
        c.kind       = UpdateKind::Merged;
        c.review     = prev.review;
        c.subject    = "#" + std::to_string(*prev.review);
        c.head       = prev.head;
        c.from_rev   = dep.rev;
        c.from_tag   = dep.expect_tag;
        c.to_rev     = ergo::package_manager::is_full_sha(it->merge_sha) ? it->merge_sha : std::string();
        c.origin     = prev.origin;
        c.trigger    = prev.trigger;
        c.next       = dep.owner == kUpmOwner  // UPM の pin は Unity の窓が書く (ergo_pkg update は触らない)
                           ? next_for_upm(dep, c.to_rev)
                           : "ergo_pkg update " + dep.name + " --to " +
                                 (dep.track_branch == "main" ? std::string("main") : c.to_rev);
        out.push_back(std::move(c));
    }
}

} // namespace

std::string repo_key(const std::string& text) {
    std::string s = text;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    const std::size_t cut = s.find_last_of("/\\:");
    if (cut != std::string::npos) s = s.substr(cut + 1);
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".git") == 0) s.resize(s.size() - 4);
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool is_run_active(const std::string& status) {
    static const std::set<std::string> kActive = {"queued", "launching", "pending", "spawned", "running", "blocked"};
    return kActive.count(status) != 0;
}

OriginResult classify_review(const LocalPr& pr, const RunListing* runs) {
    if (!runs || !runs->available) return {kOriginUnknown, ""};
    if (const auto* run = find_run(pr, *runs)) return origin_of_run(*run);
    return {kOriginHuman, ""};
}

WatchSources source_states(const ProvenanceInputs& in) {
    WatchSources s;
    if (in.reviews) s.revisor = in.reviews->available ? kSourceOk : kSourceUnavailable;
    if (in.runs) s.concordia = in.runs->available ? kSourceOk : kSourceUnavailable;
    return s;
}

std::vector<Candidate> previous_reviews(const WatchState& previous, const std::string& dependency) {
    std::vector<Candidate> out;
    for (const auto& e : previous.entries) {
        if (e.update.dependency == dependency && e.update.kind == UpdateKind::Review) out.push_back(e.update);
    }
    return out;
}

ProvenanceResult detect_provenance(const WatchDependency& dep, const ProvenanceInputs& in,
                                   const std::vector<Candidate>& previous) {
    ProvenanceResult out;
    const std::string key = repo_key(dep.repository);
    const RunListing* runs = in.runs && in.runs->available ? in.runs : nullptr;
    if (runs) {
        out.runs_observed = true;
        add_runs(dep, key, *runs, out.updates);
    }
    if (in.reviews && in.reviews->available) {
        out.reviews_observed = true;
        add_reviews(dep, key, *in.reviews, runs, out.updates);
        add_merged(dep, key, *in.reviews, previous, out.updates);
    }
    std::stable_sort(out.updates.begin(), out.updates.end(), [](const Candidate& a, const Candidate& b) {
        return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    });
    return out;
}

void attach_provenance(std::vector<DependencyResult>& results, const ProvenanceInputs& in,
                       const WatchState& previous) {
    for (auto& r : results) {
        auto p = detect_provenance(r.dependency, in, previous_reviews(previous, r.dependency.name));
        r.reviews_observed = p.reviews_observed;
        r.runs_observed    = p.runs_observed;
        for (auto& c : p.updates) r.updates.push_back(std::move(c));
    }
}

} // namespace ergo::package_watch
