#include "host_labels.h"

#include "ergo/package_viz/host_view.h"

#include <cstddef>

namespace ergo::package_viz::detail {

namespace {

using package_model::HostState;
using package_model::Package;
using package_model::WatchDependency;
using package_model::WatchUpdate;

/// 番号を札に並べる上限 (畳んだノード)。
constexpr std::size_t kFoldedNumbers = 6;

std::string or_missing(const std::string& v) {
    return v.empty() ? std::string(kHostMissing) : v;
}

std::string sha7(const std::string& sha) {
    return sha.empty() ? std::string(kHostMissing) : sha.substr(0, 7);
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (const auto& p : parts) out += (out.empty() ? "" : sep) + p;
    return out;
}

std::string tests_text(const HostState& s) {
    if (!s.has_status) return kHostMissing;
    const auto& t = s.status.tests;
    if (t.source != "junit" && t.source != "last-test-log") return "未実行";
    if (t.failed > 0) return std::to_string(t.failed) + " 失敗 (" + std::to_string(t.total) + " 中)";
    std::string text = std::to_string(t.passed) + "/" + std::to_string(t.total);
    if (t.skipped > 0) text += " (" + std::to_string(t.skipped) + " 未実行)";
    return text;
}

const WatchUpdate* first_of(const WatchDependency& d, const char* kind) {
    for (const auto& u : d.updates) {
        if (u.kind == kind) return &u;
    }
    return nullptr;
}

std::size_t count_of(const WatchDependency& d, const char* kind) {
    std::size_t n = 0;
    for (const auto& u : d.updates) n += u.kind == kind ? 1 : 0;
    return n;
}

bool off_main(const WatchDependency* d) {
    if (!d) return false;
    return first_of(*d, "off-main") != nullptr || (d->on_track.has_value() && !*d->on_track);
}

std::string number_or_missing(const std::optional<long long>& v) {
    return v ? std::to_string(*v) : std::string(kHostMissing);
}

} // namespace

std::string host_code(const HostState& s) {
    if (s.has_resolved && !s.resolved.host_id.empty()) return s.resolved.host_id;
    if (s.has_status && !s.status.host.empty()) return s.status.host;
    if (s.has_report && !s.report.host.empty()) return s.report.host;
    return kHostMissing;
}

std::string host_label(const HostState& s) {
    const std::string head = s.has_status ? sha7(s.status.head) : std::string(kHostMissing);
    const std::string mode = s.has_resolved ? or_missing(s.resolved.mode) : std::string(kHostMissing);
    return host_code(s) + " | HEAD " + head + " | モード " + mode + " | テスト " + tests_text(s);
}

std::string ergo_label(const HostState& s, const WatchDependency* ergo) {
    if (!s.has_report || !ergo) {
        return std::string("Ergo | 版 ") + kHostMissing + " | pin " + kHostMissing + " | 遅れ " + kHostMissing;
    }
    std::string lag;
    if (!ergo->observed) {
        lag = "未観測";
    } else {
        std::vector<std::string> parts;
        if (ergo->track_behind && *ergo->track_behind > 0) {
            parts.push_back(or_missing(ergo->track_branch) + " +" + std::to_string(*ergo->track_behind));
        }
        if (const auto* tag = first_of(*ergo, "tag")) parts.push_back("新タグ " + tag->to_tag);
        if (off_main(ergo)) parts.push_back("off-main");
        lag = parts.empty() ? "なし" : join(parts, " · ");
    }
    return "Ergo | 版 " + or_missing(ergo->pin_tag) + " | pin " + sha7(ergo->pin_rev) + " | 遅れ " + lag;
}

std::string dependency_label(const std::string& name, const Package* p, const WatchDependency* w) {
    std::string code = p ? p->code : std::string{};
    if (code.empty() && w) code = w->code;
    std::string label = name + (code.empty() ? "" : " (" + code + ")");

    const std::string version = p && !p->expect_tag.empty() ? p->expect_tag : (w ? w->pin_tag : "");
    const std::string pin     = p && !p->pin.empty() ? p->pin : (w ? w->pin_rev : "");
    label += " | 版 " + or_missing(version) + " · pin " + sha7(pin);
    label += " | 経路 " + or_missing(p ? p->route : "") + " · drift " + or_missing(p ? p->drift : "");
    label += " | 機能 " + (p && !p->features.empty() ? join(p->features, ",") : std::string(kHostMissing));

    std::vector<std::string> marks;
    if (p && (p->drift == "ahead" || p->drift == "behind" || p->drift == "diverged")) {
        marks.push_back("★drift");
    }
    if (p && p->dirty) marks.push_back("dirty");
    if (off_main(w)) marks.push_back("off-main");
    if (w && !w->observed) marks.push_back("未観測");
    if (w && w->is_private) marks.push_back("非公開");
    if (!marks.empty()) label += " | " + join(marks, " ");
    return label;
}

std::string update_label(const WatchDependency& w) {
    std::vector<std::string> lines;
    if (const auto* tag = first_of(w, "tag")) {
        lines.push_back(tag->to_tag + (tag->behind ? " (" + std::to_string(*tag->behind) + " 先)" : ""));
    }
    const std::size_t merged = count_of(w, "merged");
    const std::string merged_text = "merged " + std::to_string(merged);
    if (const auto* track = first_of(w, "track")) {
        std::string line = or_missing(w.track_branch) + " +" + number_or_missing(track->behind);
        if (merged > 0) line += " (うち " + merged_text + ")";
        lines.push_back(line);
    } else if (merged > 0) {
        lines.push_back(merged_text);
    }
    return join(lines, " | ");
}

std::string origin_text(const std::string& origin) {
    if (origin == "pf") return "Pf";
    if (origin == "ai") return "AI";
    if (origin == "human") return "人";
    return "不明";
}

std::string review_label(const WatchUpdate& u) {
    if (u.kind == "run") return "run " + origin_text(u.origin) + " | " + u.dependency;
    const std::string number = u.review ? "#" + std::to_string(*u.review) : u.subject;
    return number + " " + origin_text(u.origin) + " | " + u.dependency + " · head " + sha7(u.head);
}

std::string folded_review_label(const std::string& origin, const std::vector<const WatchUpdate*>& items) {
    std::size_t reviews = 0, runs = 0;
    std::vector<std::string> numbers;
    for (const auto* u : items) {
        if (u->kind == "run") {
            ++runs;
            continue;
        }
        ++reviews;
        if (numbers.size() < kFoldedNumbers && u->review) numbers.push_back("#" + std::to_string(*u->review));
    }
    std::string label = "審査中・作業中 " + origin_text(origin) + " | " + std::to_string(items.size()) +
                        " 件 (審査中 " + std::to_string(reviews) + " · 作業中 " + std::to_string(runs) + ")";
    if (!numbers.empty()) {
        label += " | " + join(numbers, " ");
        if (reviews > numbers.size()) label += " …他 " + std::to_string(reviews - numbers.size());
    }
    return label;
}

std::string check_label(const package_model::Finding& f) {
    return f.id + " | " + or_missing(f.subject);
}

} // namespace ergo::package_viz::detail
