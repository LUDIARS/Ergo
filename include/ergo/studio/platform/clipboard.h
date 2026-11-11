#pragma once
/// Reading text from the OS clipboard (the Studio UI has no text field, so a
/// string the user copied elsewhere comes in through here). Windows: the
/// Win32 clipboard. Platforms without an implementation return an explicit
/// "unsupported" error.
#include <memory>
#include <string>

namespace ergo::studio::platform {

struct ClipboardText {
    bool        ok = false;   ///< false: nothing was read (see error)
    std::string text;         ///< UTF-8, at most kMaxClipboardChars characters
    std::string error;
};

/// Longer clipboard text is cut here: callers read short strings (a URL), not documents.
constexpr int kMaxClipboardChars = 2048;

class IClipboard {
public:
    virtual ~IClipboard() = default;
    /// Call on the UI thread.
    virtual ClipboardText read_text() = 0;
};

std::shared_ptr<IClipboard> make_system_clipboard();

} // namespace ergo::studio::platform
