#include "game_window_preview.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ergo::studio::tela_shell {
namespace {
struct Match {
    HWND studio, result{};
    const std::wstring& title;
    bool ambiguous{};
};
BOOL CALLBACK match_window(HWND window, LPARAM param) {
    auto& match = *reinterpret_cast<Match*>(param);
    if (window == match.studio || !IsWindowVisible(window) ||
        GetWindowTextLengthW(window) != static_cast<int>(match.title.size()))
        return TRUE;
    std::wstring text(match.title.size() + 1, L'\0');
    int length = GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    text.resize(length);
    if (text != match.title) return TRUE;
    if (match.result) {
        match.ambiguous = true;
        return FALSE;
    }
    match.result = window;
    return TRUE;
}
} // namespace
void GameWindowPreview::release() {
    if (thumbnail_) DwmUnregisterThumbnail(thumbnail_);
    thumbnail_ = nullptr;
    source_ = nullptr;
    bounds_ = {};
}
GameWindowPreview::~GameWindowPreview() { release(); }
std::string GameWindowPreview::update(const std::string& title, float dpi) {
    if (title != title_) {
        release();
        title_ = title;
        ended_ = false;
        next_lookup_ = {};
        message_.clear();
    }
    if (title.empty() || ended_) return message_;
    if (source_ && !IsWindow(source_)) {
        release();
        ended_ = true;
        return message_ = "ゲームのウィンドウが閉じました";
    }
    if (!thumbnail_) {
        const auto now = std::chrono::steady_clock::now();
        if (now < next_lookup_) return message_;
        next_lookup_ = now + std::chrono::milliseconds(500);
        if (title.size() > 32767 || title.find('\0') != std::string::npos)
            return message_ = "ゲームのウィンドウ名が不正です";
        const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(),
                                              int(title.size()), nullptr, 0);
        if (chars <= 0) return message_ = "ゲームのウィンドウ名が不正です";
        std::wstring wide(chars, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), int(title.size()),
                            wide.data(), chars);
        Match match{destination_, nullptr, wide};
        EnumWindows(match_window, reinterpret_cast<LPARAM>(&match));
        if (match.ambiguous) return message_ = "同名のゲーム窓が複数あります";
        if (!match.result) return message_ = "ゲームのウィンドウを待っています";
        if (FAILED(DwmRegisterThumbnail(destination_, match.result, &thumbnail_)))
            return message_ = "ゲーム窓の映像を取得できません";
        source_ = match.result;
    }
    RECT client{}, source{};
    if (!GetClientRect(destination_, &client) || !GetClientRect(source_, &source) ||
        source.right <= 0 || source.bottom <= 0 || IsIconic(source_)) {
        DWM_THUMBNAIL_PROPERTIES props{};
        props.dwFlags = DWM_TNP_VISIBLE;
        props.fVisible = FALSE;
        DwmUpdateThumbnailProperties(thumbnail_, &props);
        bounds_ = {};
        return message_ = "表示中のゲーム窓を待っています";
    }
    const long top =
        std::min(client.bottom, std::lround((client.right / dpi < 540 ? 275.f : 132.f) * dpi));
    const double scale =
        std::min(double(client.right) / source.right, double(client.bottom - top) / source.bottom);
    if (scale <= 0) {
        release();
        return message_ = "ゲーム映像の表示領域がありません";
    }
    const long w = std::lround(source.right * scale), h = std::lround(source.bottom * scale);
    RECT rect{(client.right - w) / 2, top + (client.bottom - top - h) / 2, 0, 0};
    rect.right = rect.left + w;
    rect.bottom = rect.top + h;
    if (!EqualRect(&rect, &bounds_)) {
        DWM_THUMBNAIL_PROPERTIES props{};
        props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_SOURCECLIENTAREAONLY;
        props.rcDestination = rect;
        props.fVisible = TRUE;
        props.fSourceClientAreaOnly = TRUE;
        if (FAILED(DwmUpdateThumbnailProperties(thumbnail_, &props))) {
            release();
            return message_ = "ゲーム映像を表示できません";
        }
        bounds_ = rect;
    }
    return message_ = "実ゲームの映像";
}
} // namespace ergo::studio::tela_shell
