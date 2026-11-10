#pragma once
/// Thin platform socket layer for the Studio server (Winsock / POSIX).
/// Internal to ergo_studio_net.
#include <cstddef>
#include <cstdint>
#include <string>

namespace ergo::studio::net::sock {

using Handle = std::intptr_t;
constexpr Handle kInvalid = -1;

/// Initialise the socket library once per process (WSAStartup on Windows).
bool init();

/// Listening TCP socket on host:port. `bound_port` receives the real port.
Handle listen_tcp(const std::string& host, uint16_t port, uint16_t& bound_port, std::string& err);

/// Accept one connection; kInvalid when the listener was closed.
Handle accept_one(Handle listener);

/// Read up to len bytes; <=0 on close / error.
long recv_some(Handle s, char* buf, std::size_t len);

/// Write all bytes; false on error.
bool send_all(Handle s, const char* data, std::size_t len);

/// Stop both directions so a blocked recv returns.
void shutdown_both(Handle s);

void close(Handle s);

} // namespace ergo::studio::net::sock
