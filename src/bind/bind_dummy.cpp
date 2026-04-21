/// Dummy plug — no-op implementations of every ergo_bind symbol.
///
/// JSON codec dummies are intentionally not provided here: jsonm now lives
/// in `ergo_common`. Hosts that link against `ergo_bind_dummy` and also need
/// jsonm symbols should link `ergo_common` directly (or skip jsonm calls).

#define ERGO_BIND_ENABLED 1
#include "ergo/bind/bind.h"

#include <cstdint>

namespace ergo::bind {

const char* to_string(VarKind)            { return "unknown"; }
VarKind kind_from_string(const std::string&) { return VarKind::Bool; }
Value Value::of_bool(bool)                { return {}; }
Value Value::of_int32(int32_t)            { return {}; }
Value Value::of_int64(int64_t)            { return {}; }
Value Value::of_float(float)              { return {}; }
Value Value::of_double(double)            { return {}; }
Value Value::of_string(std::string)       { return {}; }
Value Value::of_color(float, float, float, float) { return {}; }
Value Value::of_vec3(float, float, float) { return {}; }
bool  Value::equals(const Value&) const   { return false; }
Value clamp_to_meta(const Value& v, const VarMeta&) { return v; }

struct Engine::Impl {};

Engine& Engine::instance() { static Engine e; return e; }
Engine::Engine() : impl_(nullptr) {}
Engine::~Engine() {}

void Engine::set_app_name(std::string) {}
const std::string& Engine::app_name() const { static const std::string s; return s; }
bool Engine::connect(const std::string&, uint16_t, const std::string&) { return false; }
void Engine::disconnect() {}
bool Engine::is_connected() const { return false; }

Handle Engine::bind_accessor(std::string, VarKind,
                             std::function<Value()>,
                             std::function<void(const Value&)>,
                             VarMeta) { return INVALID_HANDLE; }
void Engine::unbind(Handle) {}
void Engine::actor_register(uint64_t, uint64_t, const std::string&) {}
void Engine::actor_unregister(uint64_t) {}
void Engine::apply_pending_writes() {}

template<class T> Handle Engine::bind(std::string, T*, VarMeta) { return INVALID_HANDLE; }
template Handle Engine::bind<bool>       (std::string, bool*,        VarMeta);
template Handle Engine::bind<int32_t>    (std::string, int32_t*,     VarMeta);
template Handle Engine::bind<int64_t>    (std::string, int64_t*,     VarMeta);
template Handle Engine::bind<float>      (std::string, float*,       VarMeta);
template Handle Engine::bind<double>     (std::string, double*,      VarMeta);
template Handle Engine::bind<std::string>(std::string, std::string*, VarMeta);

} // namespace ergo::bind
