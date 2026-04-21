#include "ergo/blackboard/blackboard.h"

#include <iostream>
#include <sstream>

namespace ergo::blackboard {

namespace {

constexpr const char* kDefaultCategory = "Default";

std::string normalize(const std::string& category) {
    return category.empty() ? std::string(kDefaultCategory) : category;
}

struct Entry {
    void*           ptr;
    std::type_index type;
    std::string     category;

    Entry(void* p, std::type_index t, std::string c)
        : ptr(p), type(t), category(std::move(c)) {}
};

struct CleanupRecord {
    std::shared_ptr<bool> fired;
    std::function<void()> cleanup;

    void run_once() {
        if (fired && !*fired) {
            *fired = true;
            if (cleanup) cleanup();
        }
    }
};

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
        it->second = Entry(property, type, cat);
    } else {
        impl_->entries.emplace(std::move(name), Entry(property, type, std::move(cat)));
    }
}

void Engine::unregister(const std::string& name) {
    impl_->entries.erase(name);
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

    // Drop entries tagged with this category.
    for (auto eit = impl_->entries.begin(); eit != impl_->entries.end(); ) {
        if (eit->second.category == cat) {
            eit = impl_->entries.erase(eit);
        } else {
            ++eit;
        }
    }
}

void Engine::release_all() {
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
