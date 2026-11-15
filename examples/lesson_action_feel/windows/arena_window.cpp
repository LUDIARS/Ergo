#include "arena_window.h"
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <utility>

namespace lesson_action_feel::graphical {
namespace { constexpr wchar_t window_class[] = L"ErgoLessonActionArena"; }
ArenaWindow::~ArenaWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
    if (registered_) UnregisterClassW(window_class, GetModuleHandleW(nullptr));
    if (graphics_token_) Gdiplus::GdiplusShutdown(graphics_token_);
}
bool ArenaWindow::open() {
    if (hwnd_ || graphics_token_) return false;
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&graphics_token_, &startup, nullptr) != Gdiplus::Ok) return false;
    WNDCLASSW cls{};
    cls.lpfnWndProc = dispatch;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    cls.lpszClassName = window_class;
    registered_ = RegisterClassW(&cls) != 0;
    if (!registered_ && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    hwnd_ = CreateWindowExW(0, window_class, L"Ergo / Action Lab", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr, nullptr, cls.hInstance, this);
    if (!hwnd_) return false;
    ShowWindow(hwnd_, SW_SHOW);
    return true;
}
bool ArenaWindow::pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) closed_ = true;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return !closed_;
}
bool ArenaWindow::pressed(unsigned key) {
    if (key >= pressed_.size()) return false;
    return std::exchange(pressed_[key], false);
}
bool ArenaWindow::held(unsigned key) const { return key < held_.size() && held_[key]; }
Vec2 ArenaWindow::aim(const GameView& view) const {
    if (view.fighters.empty() || !has_mouse_) return {};
    RECT rect{};
    GetClientRect(hwnd_, &rect);
    return (viewport(rect.right, rect.bottom, view).world(mouse_) - view.camera_offset - view.fighters.front().pos).normalized();
}
void ArenaWindow::show(GameView view, ArenaHud hud) {
    view_ = std::move(view);
    hud_ = std::move(hud);
    if (IsIconic(hwnd_)) return;
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}
LRESULT CALLBACK ArenaWindow::dispatch(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<ArenaWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<ArenaWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
    const LRESULT result = self->handle(message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->hwnd_ = nullptr;
    }
    return result;
}
LRESULT ArenaWindow::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE: closed_ = true; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_KILLFOCUS:
        active_ = false; held_.fill(false); pressed_.fill(false); return 0;
    case WM_SETFOCUS: active_ = true; return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize = {960, 640}; return 0;
    }
    case WM_KEYDOWN:
        if (wparam < held_.size()) {
            if (!held_[wparam]) pressed_[wparam] = true;
            held_[wparam] = true;
        }
        return 0;
    case WM_KEYUP:
        if (wparam < held_.size()) held_[wparam] = false;
        return 0;
    case WM_LBUTTONDOWN: pressed_[VK_LBUTTON] = true; return 0;
    case WM_MOUSEMOVE: mouse_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}; has_mouse_ = true; return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(hwnd_, &paint);
        RECT rect{};
        GetClientRect(hwnd_, &rect);
        paint_arena(dc, rect.right, rect.bottom, view_, hud_);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    default: return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
}
} // namespace lesson_action_feel::graphical
