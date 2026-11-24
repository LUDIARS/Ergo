#include "preview_window.h"

#include "preview_gpu.h"
#include "preview_surface.h"
#include "ergo/studio/platform/sha256.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;
namespace {

struct Window {
    PreviewSurface surface;
    pictor::VulkanContext vk;

    Window() {
        pictor::VulkanContextConfig config;
        config.app_name = "Ergo Studio Visus";
        config.enable_swapchain_transfer_src = true;
        config.create_default_render_pass = false;
        config.frames_in_flight = 1;
        if (!vk.initialize(&surface, config)) throw std::runtime_error(vk.init_result().detail);
    }
};

} // namespace

struct PreviewWindow::State {
    State(fs::path shader_dir, host::LogFn logger) : shaders(std::move(shader_dir)), log(std::move(logger)) {}
    fs::path shaders;
    host::LogFn log;
    std::mutex mutex;
    std::deque<CaptureRequest> pending;
    std::unique_ptr<Window> window;
    fs::path project;
};

PreviewWindow::PreviewWindow(fs::path shaders, host::LogFn log)
    : state_(std::make_unique<State>(std::move(shaders), std::move(log))) {}

PreviewWindow::~PreviewWindow() = default;

bool PreviewWindow::request(CaptureRequest request) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto duplicate = std::find_if(state_->pending.begin(), state_->pending.end(), [&](const auto& queued) {
        return queued.definition == request.definition && queued.revision == request.revision &&
               queued.capture == request.capture && queued.explicit_request == request.explicit_request;
    });
    if (duplicate != state_->pending.end()) return true;
    if (state_->pending.size() >= 32) return false;
    if (request.explicit_request) state_->pending.push_front(std::move(request));
    else state_->pending.push_back(std::move(request));
    return true;
}

void PreviewWindow::pump(const fs::path& project_root) {
    if (state_->project != project_root || (state_->window && state_->window->surface.should_close())) close();
    state_->project = project_root;
    if (project_root.empty()) return;
    auto catalog_root = project_root / kCatalogDirectory;
    std::optional<CaptureRequest> request;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->pending.empty()) return;
        request = std::move(state_->pending.front());
        state_->pending.pop_front();
    }
    if (request->definition.parent_path().parent_path() != catalog_root) return;
    try {
        const auto definition = read_bounded(request->definition, kMaxVisusBytes);
        const auto model = read_bounded(request->source, kMaxModelBytes);
        if (!definition || !model) return;
        if (platform::sha256_hex(platform::sha256_hex(*definition) + platform::sha256_hex(*model)) != request->revision)
            return;
        FolderAssetCatalog catalog(catalog_root);
        if (request->capture && !request->explicit_request &&
            has_capture(catalog.find(request->name), request->revision)) return;
        const auto mesh = preview_mesh(request->source, *model);
        if (!state_->window) state_->window = std::make_unique<Window>();
        const auto png = draw_preview(state_->window->vk, state_->shaders, mesh, request->capture);
        if (request->capture) store_capture(catalog, *request, png);
    } catch (const std::exception& error) {
        close();
        if (state_->log) state_->log("warn", "[asset_import] Visus preview: " + std::string(error.what()));
    }
}

void PreviewWindow::close() { state_->window.reset(); }

} // namespace ergo::studio::plugins::asset_import
