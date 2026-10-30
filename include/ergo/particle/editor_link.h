#pragma once

/// EditorLink — engine-side feeder for the unified ergo tool's `particle`
/// plugin (tools/ergo/src/plugins/particle/). Target `ergo_particle_link`.
///
/// Connects out to `ws://host:5170/particle/ws` over the shared transport
/// (`ergo_common_ws`), receives the plugin's `state` frames and hands the
/// decoded `ParticleEffectConfig` to the host on its own thread via poll() /
/// apply_to(). Receive-only: the browser UI is the only writer.
///
/// Typical host loop:
///
///     ergo::particle::EditorLink link;
///     link.connect();                    // async; retries until disconnect()
///     ...
///     link.apply_to(system);             // once per frame, main thread
///     system.update(dt);
///
/// Release builds simply do not link this target (there is no dummy); the
/// core `ergo_particle` library stays socket-free.

#include "ergo/particle/effect_config.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ergo::particle {

class ParticleSystem;

/// Decode one `particle` plugin frame. Accepts only
/// `{ "op": "state", "config": { ... } }`: the `config` object is parsed onto
/// a default-constructed config (fields missing from it keep their defaults)
/// and written to `out`. Returns false — leaving `out` untouched — for any
/// other op (e.g. `ack`), malformed JSON, or a missing / non-object `config`.
bool decode_state_frame(const std::string& frame, ParticleEffectConfig& out);

class EditorLink {
public:
    EditorLink();
    ~EditorLink();

    EditorLink(const EditorLink&)            = delete;
    EditorLink& operator=(const EditorLink&) = delete;

    /// Start connecting in the background. The link reconnects on its own
    /// whenever the tool restarts. Returns false if already started (call
    /// disconnect() first) or if host / port is empty.
    bool connect(const std::string& host = "127.0.0.1",
                 uint16_t           port = 5170,
                 const std::string& path = "/particle/ws");

    /// Stop the worker and close the socket. Safe to call repeatedly.
    void disconnect();

    /// True while the WebSocket session is open.
    bool is_connected() const;

    /// Copy the latest received config into `out`. Returns true only when a
    /// config newer than the one returned by the previous poll() arrived
    /// (frames re-sending an identical config do not count). Thread-safe;
    /// meant to be called once per frame from the host thread.
    bool poll(ParticleEffectConfig& out);

    /// poll() + ParticleSystem::set_config(). Returns true when a new config
    /// was applied.
    bool apply_to(ParticleSystem& system);

    /// Process one raw frame as if it had arrived from the tool (the socket
    /// worker calls this). Lets tests — or a host that already owns a
    /// transport — drive poll() without a connection. Returns true when the
    /// frame carried a config different from the latest one.
    bool feed(const std::string& frame);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::particle
