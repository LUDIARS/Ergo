#include "ergo/blackboard/blackboard.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <tuple>

namespace ergo::blackboard {

namespace {

constexpr const char* kDefaultCategory = "Default";

std::string normalize(const std::string& category) {
    return category.empty() ? std::string(kDefaultCategory) : category;
}

struct CleanupRecord {
    std::shared_ptr<bool> fired;
    std::function<void()> cleanup;

    void run_once() {
        if (fired && !*fired) {
            *fired = true;
            if (cleanup) cleanup();
        }
    }

    bool done() const { return !fired || *fired; }
};

struct Entry {
    void*           ptr;
    std::type_index type;
    std::string     category;
    /// Cleanups of by-name subscriptions on this registration. They share
    /// the `fired` flag with the category record and the Subscription
    /// handle, so whichever path runs first wins and the others no-op.
    std::vector<CleanupRecord> name_subs;

    Entry(void* p, std::type_index t, std::string c)
        : ptr(p), type(t), category(std::move(c)) {}
};

/// Close every by-name subscription bound to `e`. Called while the
/// registered Property is still alive (the registration contract), right
/// before the entry is replaced or erased.
void close_name_subs(Entry& e) {
    auto subs = std::move(e.name_subs);
    e.name_subs.clear();
    for (auto rit = subs.rbegin(); rit != subs.rend(); ++rit) {
        rit->run_once();
    }
}

} // namespace

struct Engine::Impl {
    std::unordered_map<std::string, Entry>                       entries;
    std::unordered_map<std::string, std::vector<CleanupRecord>>  category_cleanups;
    std::unordered_map<std::string, std::size_t>                 sub_counts;
};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(new Impl()) {}
Engine::~Engine() { delete impl_; }

void Engine::register_erased(std::string name, void* property,
                             std::type_index type, std::string category) {
    auto cat = normalize(category);
    auto it  = impl_->entries.find(name);
    if (it != impl_->entries.end()) {
        warn("register_property: replacing existing '" + name + "'");
        close_name_subs(it->second);
        it->second = Entry(property, type, cat);
    } else {
        impl_->entries.emplace(std::move(name), Entry(property, type, std::move(cat)));
    }
}

void Engine::unregister(const std::string& name) {
    auto it = impl_->entries.find(name);
    if (it == impl_->entries.end()) return;
    close_name_subs(it->second);
    impl_->entries.erase(it);
}

void* Engine::find_erased(const std::string& name, std::type_index type,
                          bool warn_missing) const {
    auto it = impl_->entries.find(name);
    if (it == impl_->entries.end()) {
        if (warn_missing) {
            warn("lookup: '" + name + "' is not registered "
                 "(register it before subscribing by name)");
        }
        return nullptr;
    }
    if (it->second.type != type) {
        warn(std::string("lookup: '") + name + "' is registered as "
             + it->second.type.name() + " but requested as " + type.name());
        return nullptr;
    }
    return it->second.ptr;
}

void Engine::attach_to_entry(const std::string&    name,
                             std::shared_ptr<bool> fired,
                             std::function<void()> cleanup) {
    auto it = impl_->entries.find(name);
    if (it == impl_->entries.end()) return;
    auto& subs = it->second.name_subs;
    // Drop records already closed through their handle / category so a
    // subscribe-reset churn does not grow the list without bound.
    subs.erase(std::remove_if(subs.begin(), subs.end(),
                              [](const CleanupRecord& r) { return r.done(); }),
               subs.end());
    subs.push_back(CleanupRecord{ std::move(fired), std::move(cleanup) });
}

void Engine::visit(const std::function<void(const std::string&, std::type_index,
                                            const void*)>& fn) const {
    if (!fn) return;
    // Snapshot first so a misbehaving callback cannot invalidate the
    // iteration (mutating the registry from `fn` is still unsupported).
    std::vector<std::tuple<std::string, std::type_index, const void*>> rows;
    rows.reserve(impl_->entries.size());
    for (const auto& [name, entry] : impl_->entries) {
        rows.emplace_back(name, entry.type, entry.ptr);
    }
    for (const auto& [name, type, ptr] : rows) fn(name, type, ptr);
}

void Engine::release(const std::string& category) {
    auto cat = normalize(category);
    auto it  = impl_->category_cleanups.find(cat);
    if (it != impl_->category_cleanups.end()) {
        // Run in reverse to mirror destructor semantics.
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            rit->run_once();
        }
        impl_->category_cleanups.erase(it);
    }

    // Drop entries tagged with this category, closing their by-name
    // subscriptions (which may live in other categories) first.
    for (auto eit = impl_->entries.begin(); eit != impl_->entries.end(); ) {
        if (eit->second.category == cat) {
            close_name_subs(eit->second);
            eit = impl_->entries.erase(eit);
        } else {
            ++eit;
        }
    }
}

void Engine::release_all() {
    for (auto& [_name, entry] : impl_->entries) close_name_subs(entry);
    for (auto& [_cat, vec] : impl_->category_cleanups) {
        for (auto rit = vec.rbegin(); rit != vec.rend(); ++rit) {
            rit->run_once();
        }
    }
    impl_->category_cleanups.clear();
    impl_->entries.clear();
    impl_->sub_counts.clear();
}

void Engine::bump_count(const std::string& name) {
    ++impl_->sub_counts[name];
}

void Engine::drop_count(const std::string& name) {
    auto it = impl_->sub_counts.find(name);
    if (it == impl_->sub_counts.end()) return;
    if (it->second <= 1) impl_->sub_counts.erase(it);
    else                 --it->second;
}

void Engine::store_in_category(const std::string&    category,
                               std::shared_ptr<bool> fired,
                               std::function<void()> cleanup) {
    auto& vec = impl_->category_cleanups[category];
    vec.push_back(CleanupRecord{ std::move(fired), std::move(cleanup) });
}

std::vector<std::string> Engine::registered_property_names() const {
    std::vector<std::string> out;
    out.reserve(impl_->entries.size());
    for (const auto& [name, _entry] : impl_->entries) out.push_back(name);
    return out;
}

std::size_t Engine::subscription_count(const std::string& name) const {
    auto it = impl_->sub_counts.find(name);
    return (it == impl_->sub_counts.end()) ? 0 : it->second;
}

std::size_t Engine::category_count() const {
    return impl_->category_cleanups.size();
}

std::string Engine::debug_info() const {
    std::ostringstream os;
    os << "=== Blackboard Debug Info ===\n\n[Registered Properties]\n";
    for (const auto& [name, entry] : impl_->entries) {
        auto cnt_it = impl_->sub_counts.find(name);
        std::size_t cnt = (cnt_it == impl_->sub_counts.end()) ? 0 : cnt_it->second;
        os << "  " << name << " (" << entry.category << "): " << cnt << " subscribers\n";
    }
    os << "\n[Categories]\n";
    for (const auto& [cat, vec] : impl_->category_cleanups) {
        os << "  " << cat << ": " << vec.size() << " cleanups\n";
    }
    return os.str();
}

void Engine::warn(const std::string& msg) const {
    std::cerr << "[ergo_blackboard] " << msg << '\n';
}

} // namespace ergo::blackboard
