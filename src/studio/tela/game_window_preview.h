#pragma once
#include <dwmapi.h>
#include <windows.h>

#include <chrono>
#include <string>

namespace ergo::studio::tela_shell {
// Display a selected profile's game window in this process's Studio window.
class GameWindowPreview {
public:
    explicit GameWindowPreview(HWND destination) : destination_(destination) {}
    ~GameWindowPreview();
    GameWindowPreview(const GameWindowPreview&) = delete;
    GameWindowPreview& operator=(const GameWindowPreview&) = delete;
    std::string update(const std::string& title, float dpi);

private:
    void release();
    HWND destination_, source_{};
    HTHUMBNAIL thumbnail_{};
    std::string title_, message_;
    RECT bounds_{};
    bool ended_ = false;
    std::chrono::steady_clock::time_point next_lookup_{};
};
} // namespace ergo::studio::tela_shell
