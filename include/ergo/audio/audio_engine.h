#pragma once

/// ergo::audio — backend-agnostic game audio engine.
///
/// Default backend: **FMOD Core** (auto-detected at CMake configure time
/// via `FMOD_SDK_DIR`). When FMOD is unavailable the build falls back
/// transparently to a **Dummy** backend that just logs what would have
/// played — hosts don't need any #ifdefs.
///
/// Only one-shot playback is exposed right now. Streams / 3D / DSP
/// chains / FMOD Studio events are intentionally left for later
/// iterations — this header stays small so we can commit the wiring
/// without the rest of the game depending on an evolving API.

#include <cstdint>
#include <string>

namespace ergo::audio {

using SoundHandle = uint32_t;
constexpr SoundHandle INVALID_SOUND = 0;

/// Which backend the linked library actually uses at runtime. Resolved
/// at CMake configure time.
enum class Backend : uint8_t {
    Dummy = 0,   ///< No real playback, logs events. Always available.
    FMOD  = 1,   ///< FMOD Core 2.x (requires FMOD_SDK_DIR on the build).
};

class Engine {
public:
    static Engine& instance();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    /// Bring up the audio system. Returns false on init failure; the
    /// engine is then left in an uninitialized state and subsequent
    /// calls are safe no-ops. Idempotent — second call returns true
    /// without re-init.
    bool initialize();

    /// Tear down. Idempotent.
    void shutdown();

    /// Call once per frame from the main thread. Pumps FMOD's internal
    /// update loop; Dummy backend does nothing.
    void update();

    /// Load an on-disk WAV / OGG (plus anything else FMOD decodes). The
    /// returned handle is opaque and stable until `unload_sound` or
    /// `shutdown`. Returns 0 on failure.
    SoundHandle load_sound(const std::string& path);

    /// Release the underlying resource. Safe on INVALID_SOUND.
    void unload_sound(SoundHandle handle);

    /// Fire-and-forget one-shot playback.
    ///   `volume` : 0.0 .. 1.0 (linear gain)
    ///   `pitch`  : frequency ratio. 1.0 = unchanged, 2.0 = 1 octave up.
    ///              Values <= 0 are clamped to 0.01.
    void play(SoundHandle handle, float volume = 1.0f, float pitch = 1.0f);

    /// Which backend is compiled in.
    Backend     backend()      const;
    const char* backend_name() const;

    bool is_initialized() const;

private:
    Engine();
    ~Engine();
    struct Impl;
    Impl* impl_ = nullptr;
};

// Convenience free-function shortcuts.
inline bool         initialize()                                        { return Engine::instance().initialize(); }
inline void         shutdown()                                          { Engine::instance().shutdown(); }
inline void         update()                                            { Engine::instance().update(); }
inline SoundHandle  load_sound(const std::string& path)                 { return Engine::instance().load_sound(path); }
inline void         unload_sound(SoundHandle h)                         { Engine::instance().unload_sound(h); }
inline void         play(SoundHandle h, float vol = 1.0f, float pitch = 1.0f) {
    Engine::instance().play(h, vol, pitch);
}

} // namespace ergo::audio
