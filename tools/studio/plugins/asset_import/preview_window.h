#pragma once

#include "capture_request.h"
#include "ergo/studio/host/plugin.h"

#include <memory>

namespace ergo::studio::plugins::asset_import {

class PreviewWindow {
public:
    PreviewWindow(std::filesystem::path shaders, host::LogFn log);
    ~PreviewWindow();
    PreviewWindow(const PreviewWindow&) = delete;
    PreviewWindow& operator=(const PreviewWindow&) = delete;

    /// Request can be called on the plug-in worker or an HTTP connection thread.
    bool request(CaptureRequest request);
    /// Pump and close are called by the GLFW owner thread.
    void pump(const std::filesystem::path& project_root);
    void close();

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace ergo::studio::plugins::asset_import
