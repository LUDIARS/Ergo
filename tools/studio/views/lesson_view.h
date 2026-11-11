#pragma once
/// The 授業 tab: connect / disconnect, link status, and what the lesson
/// plugin is doing (assignment steps for a student, demo features for the
/// presenter). Reads LessonPlugin::shown() only; never the token or the code.
///
/// The Studio UI has no text field, so the connection string comes from the
/// clipboard: the user copies it on the lesson page and presses the button.
#include "plugins/lesson/lesson_plugin.h"

#include "ergo/studio/platform/clipboard.h"
#include "ergo/studio/ui/plugin_view.h"

namespace ergo::studio::views {

class LessonView final : public host::IPluginView {
public:
    /// `plugin` owns this view (LessonPlugin::attach_view) and outlives it.
    LessonView(plugins::lesson::LessonPlugin& plugin, std::shared_ptr<platform::IClipboard> clipboard);

    uint64_t view_revision() const override { return plugin_.revision() + local_revision_; }
    void     build(ui::ViewBuilder& b) override;

private:
    void paste_and_connect();

    plugins::lesson::LessonPlugin&        plugin_;
    std::shared_ptr<platform::IClipboard> clipboard_;
    std::string                           clipboard_error_;   ///< UI thread only
    uint64_t                              local_revision_ = 0;
};

} // namespace ergo::studio::views
