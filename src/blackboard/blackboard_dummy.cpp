/// Dummy plug — no-op implementation of every ergo_blackboard symbol.
/// Hosts that want to disable the feature at link time can swap to this lib.

#include "ergo/blackboard/blackboard.h"

namespace ergo::blackboard {

struct Engine::Impl {};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(nullptr) {}
Engine::~Engine() {}

void Engine::register_erased(std::string, void*, std::type_index, std::string) {}
void Engine::unregister(const std::string&)                                   {}
void Engine::release(const std::string&)                                       {}
void Engine::release_all()                                                     {}
void Engine::bump_count(const std::string&)                                    {}
void Engine::drop_count(const std::string&)                                    {}
void Engine::store_in_category(const std::string&, std::shared_ptr<bool>,
                               std::function<void()>)                          {}

std::vector<std::string> Engine::registered_property_names() const { return {}; }
std::size_t              Engine::subscription_count(const std::string&) const { return 0; }
std::size_t              Engine::category_count() const                        { return 0; }
std::string              Engine::debug_info() const  { return "(blackboard disabled)\n"; }
void                     Engine::warn(const std::string&) const                {}

} // namespace ergo::blackboard
