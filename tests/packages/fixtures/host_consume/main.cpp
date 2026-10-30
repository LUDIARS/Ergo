// Host program for tests/packages/test_host_consume.cmake. The host sets
// C++14; this compiles only because Ergo's targets carry their C++ standard
// (ergo_physics2d -> ergo_math: C++20) as a usage requirement.
#include "ergo/bind/bind.h"
#include "ergo/input/keyboard_device.h"
#include "ergo/log/log.h"
#include "ergo/physics2d/physics2d.h"

#include <cstdio>

int main() {
    // ergo_input no longer logs by itself: a host that wants key logs
    // subscribes to the observer.
    int key_events = 0;
    ergo::input::KeyboardDevice keyboard;
    keyboard.initialize();
    keyboard.setConnected(true);
    keyboard.observer().subscribe([&key_events](const ergo::input::InputEvent& ev) {
        ++key_events;
        ERGO_LOG_INFO("[host] key event: code=%u", static_cast<unsigned>(ev.code));
    });
    keyboard.injectKeyState(ergo::input::KeyCode::Space, true);
    keyboard.injectKeyState(ergo::input::KeyCode::Space, false);

    float speed = 1.5f;
    auto handle = ergo::bind::Engine::instance().bind("host.speed", &speed);

    using namespace ergo::physics2d;
    World world(ergo::math::Vec<2, float>{{0.0f, -10.0f}});
    BodyDef def;
    def.type = BodyType::Dynamic;
    def.position = {{0.0f, 10.0f}};
    BodyHandle body = world.create_body(def, make_circle_shape(0.5f));
    for (int i = 0; i < 60; ++i) world.step(1.0f / 60.0f);
    const float y = world.get_body(body)->position.data[1];

    const bool ok = key_events == 2 && handle != ergo::bind::INVALID_HANDLE && y < 10.0f;
    std::printf("host_app: key_events=%d y=%.3f -> %s\n", key_events, static_cast<double>(y),
                ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
