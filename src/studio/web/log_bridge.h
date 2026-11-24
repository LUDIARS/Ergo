#pragma once
/// The web server logs through a portus::Logger (fields with secret names such
/// as token / authorization / cookie are masked by Portus) whose lines end up in
/// the Studio's own log (net::LogFn: stdout / stderr of the process).
#include "ergo/studio/net/server.h"

#include <portus/log.hpp>

namespace ergo::studio::web {

portus::Logger studio_logger(net::LogFn log);

} // namespace ergo::studio::web
