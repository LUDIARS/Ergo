#pragma once
#include "arena_renderer.h"
#include <array>

namespace lesson_action_feel::graphical {
class ArenaWindow {
public:
    ArenaWindow() = default;
    ~ArenaWindow();
    ArenaWindow(const ArenaWindow&) = delete;
    ArenaWindow& operator=(const ArenaWindow&) = delete;
    bool open();
    bool pump();
    bool pressed(unsigned key);
    bool held(unsigned key) const;
    bool active() const { return active_ && hwnd_ && !IsIconic(hwnd_); }
    Vec2 aim(const GameView& view) const;
    void show(GameView view, ArenaHud hud);
private:
    static LRESULT CALLBACK dispatch(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
    HWND hwnd_ = nullptr;
    ULONG_PTR graphics_token_ = 0;
    std::array<bool, 256> held_{};
    std::array<bool, 256> pressed_{};
    bool active_ = true, closed_ = false;
    bool registered_ = false;
    POINT mouse_{};
    bool has_mouse_ = false;
    GameView view_;
    ArenaHud hud_;
};
} // namespace lesson_action_feel::graphical
