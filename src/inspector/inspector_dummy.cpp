/// Dummy plug — provides the symbols of `ergo_inspector` as no-ops.
/// Link this library instead of the real one to satisfy references when
/// the host is built with ERGO_INSPECTOR_ENABLED defined but actual
/// inspection isn't wanted (e.g. headless CI, integration tests).

#define ERGO_INSPECTOR_ENABLED 1
#include "ergo/inspector/inspector.h"

#include <cstdint>

namespace ergo::inspector {

const char* to_string(VarKind)            { return "unknown"; }
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

void  CommandQueue::push(WriteCommand)         {}
std::vector<WriteCommand> CommandQueue::drain(){ return {}; }
std::size_t CommandQueue::size_approx() const  { return 0; }
void  CommandQueue::clear()                    {}

struct Inspector::ServerState {};

Inspector& Inspector::instance() { static Inspector i; return i; }
Inspector::Inspector()  = default;
Inspector::~Inspector() = default;

Handle Inspector::register_accessor(std::string, VarKind,
                                    std::function<Value()>,
                                    std::function<void(const Value&)>,
                                    VarMeta) { return INVALID_HANDLE; }
void   Inspector::unregister(Handle) {}
bool   Inspector::start_server(uint16_t)   { return false; }
void   Inspector::stop_server()            {}
bool   Inspector::server_running() const   { return false; }
void   Inspector::apply_pending_writes()   {}
std::vector<Tweakable> Inspector::snapshot() const { return {}; }
bool   Inspector::read_value(Handle, Value&) const { return false; }
Handle Inspector::find_by_name(const std::string&) const { return INVALID_HANDLE; }
void   Inspector::enqueue_write(Handle, Value) {}

template<class T>
Handle Inspector::register_value(std::string, T*, VarMeta) { return INVALID_HANDLE; }

template Handle Inspector::register_value<bool>       (std::string, bool*,        VarMeta);
template Handle Inspector::register_value<int32_t>    (std::string, int32_t*,     VarMeta);
template Handle Inspector::register_value<int64_t>    (std::string, int64_t*,     VarMeta);
template Handle Inspector::register_value<float>      (std::string, float*,       VarMeta);
template Handle Inspector::register_value<double>     (std::string, double*,      VarMeta);
template Handle Inspector::register_value<std::string>(std::string, std::string*, VarMeta);

} // namespace ergo::inspector
