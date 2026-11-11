// Win32 clipboard implementation of IClipboard (Windows only).
#include "ergo/studio/platform/clipboard.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

namespace ergo::studio::platform {
namespace {

std::string narrow(const wchar_t* w, std::size_t len) {
    if (len == 0) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), s.data(), n, nullptr, nullptr);
    return s;
}

std::string last_error(const char* what) { return std::string(what) + " failed (error " + std::to_string(GetLastError()) + ")"; }

/// CloseClipboard() on scope exit.
struct OpenedClipboard {
    ~OpenedClipboard() { CloseClipboard(); }
};

class WinClipboard final : public IClipboard {
public:
    ClipboardText read_text() override {
        ClipboardText out;
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            out.error = "the clipboard holds no text";
            return out;
        }
        if (!OpenClipboard(nullptr)) {
            out.error = last_error("OpenClipboard");
            return out;
        }
        OpenedClipboard opened;
        const HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (!data) {
            out.error = last_error("GetClipboardData");
            return out;
        }
        const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (!text) {
            out.error = last_error("GlobalLock");
            return out;
        }
        // The block is NUL-terminated by contract, but never read past its size.
        const std::size_t capacity = GlobalSize(data) / sizeof(wchar_t);
        const std::size_t limit    = capacity < static_cast<std::size_t>(kMaxClipboardChars)
                                         ? capacity
                                         : static_cast<std::size_t>(kMaxClipboardChars);
        std::size_t len = 0;
        while (len < limit && text[len] != L'\0') ++len;
        out.text = narrow(text, len);
        GlobalUnlock(data);
        out.ok = true;
        return out;
    }
};

} // namespace

std::shared_ptr<IClipboard> make_system_clipboard() { return std::make_shared<WinClipboard>(); }

} // namespace ergo::studio::platform
