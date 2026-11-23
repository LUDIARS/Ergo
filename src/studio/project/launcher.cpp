#include "ergo/studio/project/launcher.h"

#include <algorithm>

namespace ergo::studio::project {

namespace fs = std::filesystem;

Launcher::Launcher(ProjectCatalog catalog, OpenProject& open, LauncherDeps deps)
    : catalog_(std::move(catalog)), open_(open), deps_(std::move(deps)) {
    std::string err;
    if (!catalog_.load(err)) notice_ = err;
}

std::string Launcher::normalize(const std::string& source) {
    if (source.empty()) return {};
    std::error_code ec;
    fs::path p = fs::absolute(fs::u8path(source), ec);
    if (ec) p = fs::u8path(source);
    return p.lexically_normal().generic_u8string();
}

std::vector<LauncherRow> Launcher::rows() const {
    std::vector<ProjectRef> found = deps_.discover ? deps_.discover() : std::vector<ProjectRef>{};
    const auto current = open_.current();
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LauncherRow> out;
    for (const RecentProject& p : catalog_.projects()) {
        std::error_code ec;
        LauncherRow r{p.source, p.name, p.root, p.style, p.domain, p.last_opened, true,
                      !fs::is_regular_file(fs::u8path(p.source), ec), false};
        r.open = current && current->source == p.source;
        out.push_back(std::move(r));
    }
    for (ProjectRef& f : found) {
        f.source = normalize(f.source);
        const bool listed = std::any_of(out.begin(), out.end(), [&](const LauncherRow& r) { return r.source == f.source; });
        if (listed) continue;
        LauncherRow r{f.source, f.name, f.root, f.style, f.domain, "", false, false, false};
        r.open = current && current->source == f.source;
        out.push_back(std::move(r));
    }
    return out;
}

bool Launcher::open(const std::string& source, std::string& err) {
    const std::string key = normalize(source);
    std::error_code ec;
    if (key.empty() || !fs::is_regular_file(fs::u8path(key), ec)) {
        err = "profile not found: " + source;
        return false;
    }
    if (!deps_.probe) {
        err = "no profile reader";
        return false;
    }
    auto ref = deps_.probe(fs::u8path(key), err);
    if (!ref) return false;
    ref->source = key;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        catalog_.remember({key, ref->name, ref->root, ref->style, ref->domain, deps_.now ? deps_.now() : std::string()});
        save_locked();
    }
    open_.open(*ref);
    ++revision_;
    return true;
}

bool Launcher::close() {
    return open_.close();
}

bool Launcher::add(const std::string& source, std::string& err) {
    const std::string key = normalize(source);
    std::error_code ec;
    if (key.empty() || !fs::is_regular_file(fs::u8path(key), ec)) {
        err = "profile not found: " + source;
        return false;
    }
    if (!deps_.probe) {
        err = "no profile reader";
        return false;
    }
    const auto ref = deps_.probe(fs::u8path(key), err);
    if (!ref) return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string last;
        for (const RecentProject& p : catalog_.projects()) {
            if (p.source == key) last = p.last_opened;   // adding again keeps when it was opened
        }
        catalog_.remember({key, ref->name, ref->root, ref->style, ref->domain, last});
        save_locked();
    }
    ++revision_;
    return true;
}

bool Launcher::remove(const std::string& source, std::string& err) {
    const std::string key = normalize(source);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!catalog_.forget(key)) {
            err = "not in the list: " + source;
            return false;
        }
        save_locked();
    }
    const auto current = open_.current();
    if (current && current->source == key) open_.close();
    ++revision_;
    return true;
}

std::string Launcher::notice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return notice_;
}

void Launcher::save_locked() {
    std::string err;
    if (!catalog_.save(err)) notice_ = err;
}

} // namespace ergo::studio::project
