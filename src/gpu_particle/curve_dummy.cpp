// Dummy plug for curve.cpp — the whole file is a single translation unit
// that pulls in only headers. Links faster than curve.cpp when the host
// project only needs symbol presence (e.g. incremental builds).
#include "ergo/gpu_particle/curve.h"

namespace ergo::gpu_particle {
// Intentionally empty.
} // namespace ergo::gpu_particle
