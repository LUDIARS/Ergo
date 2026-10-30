#pragma once

/// EditorLink — engine-side client for the unified ergo tool's `scene_editor`
/// plugin (tools/ergo/src/plugins/scene_editor/). Target `ergo_scene_link`.
///
/// Connects out to `ws://host:5170/scene_editor/ws` over the shared transport
/// (`ergo_common_ws`), publishes Scene snapshots for the editor's tree and
/// inspector, and receives the editor's field-level patches, which the host
/// applies on its own thread with poll(). Wire protocol:
/// spec/feature/tool/scene_editor.md.
///
/// Typical host loop:
///
///     ergo::scene::EditorLink link;
///     link.set_app_name("lookdev");
///     link.connect();              // async; reconnects until disconnect()
///     link.publish(*scene);        // once, and again after host-side edits
///     ...
///     link.poll(*scene);           // once per frame, main thread
///
/// Every scene published so far is re-sent (after a `hello`) whenever the
/// connection (re)opens, so a restarted tools/ergo sees the host's scenes
/// again without host code. After poll() applies patches it re-publishes each
/// touched scene — also when a patch was rejected — so the editor always
/// shows the engine's actual state.

#include "ergo/scene/scene.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ergo::scene {

class EditorLink {
public:
    /// Receives each outgoing text frame (see set_transport()).
    using SendHook = std::function<void(const std::string& frame)>;

    EditorLink();
    ~EditorLink();

    EditorLink(const EditorLink&)            = delete;
    EditorLink& operator=(const EditorLink&) = delete;

    /// Start connecting in the background. Returns false if already started
    /// (call disconnect() first), if a transport hook is installed, or if
    /// host / port is empty.
    bool connect(const std::string& host = "127.0.0.1",
                 uint16_t           port = 5170,
                 const std::string& path = "/scene_editor/ws");

    /// Stop the socket worker. Published scenes are remembered and re-sent on
    /// the next connect(). Safe to call repeatedly.
    void disconnect();

    /// True while the WebSocket session is open (always true while a
    /// transport hook is installed).
    bool is_connected() const;

    /// Name shown for this engine in the editor (default "anonymous"). Sent
    /// in `hello`, so set it before connect().
    void set_app_name(std::string app);
    std::string app_name() const;

    /// Send `{op:"scene", id, json: scene.to_json()}` and remember it for
    /// reconnects. Call again after host-side edits to refresh the editor.
    /// Returns false for a scene without an id.
    bool publish(const Scene& scene);

    /// Send `{op:"scene_remove", id}` and forget the scene; queued patches
    /// for it are discarded. Returns false if it was not published.
    bool unpublish(const std::string& scene_id);

    /// Apply queued patches to the mounted scenes they name (main thread),
    /// then re-publish every touched scene. Patches for scenes that are not
    /// mounted in `graph` are discarded. Returns how many patches applied.
    std::size_t poll(SceneGraph& graph);

    /// Single-scene variant: applies only the patches for `scene.id()`.
    /// Patches for other published scenes stay queued for their own poll().
    std::size_t poll(Scene& scene);

    /// Transport seam for tests or a host that owns its own socket: while a
    /// hook is installed, outgoing frames go to it instead of the WebSocket,
    /// and installing it counts as a connection opening (`hello` plus every
    /// published scene are sent right away). Pass nullptr to remove it. The
    /// hook runs under the link's lock and must not call back into the link.
    void set_transport(SendHook hook);

    /// Process one incoming frame as if it had arrived from the tool (the
    /// socket worker calls this). Only `{op:"patch", scene_id, actor_id,
    /// field, value}` is acted on; it is queued for poll(). Returns true when
    /// a patch was queued.
    bool receive(const std::string& frame);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::scene
