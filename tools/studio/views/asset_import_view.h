#pragma once

#include "api_view.h"

#include <chrono>

namespace ergo::studio::views {

class AssetImportView final : public ApiView {
public:
    using ApiView::ApiView;
    uint64_t view_revision() const override;

private:
    void on_first_show() override;
    void render(ui::ViewBuilder& b) override;
    void action(const std::string& path);
    std::string selected_;
    std::chrono::steady_clock::time_point refreshed_{};
};

} // namespace ergo::studio::views
