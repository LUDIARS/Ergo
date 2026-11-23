#include "api_view.h"

namespace ergo::studio::views {

ApiView::ApiView(host::IPlugin& plugin) : plugin_(plugin), api_(std::make_unique<PluginApi>(plugin)) {}

ApiView::~ApiView() {
    alive_->store(false);
    api_.reset();   // joins the worker before the slots go away
}

void ApiView::build(ui::ViewBuilder& b) {
    std::lock_guard<std::recursive_mutex> lk(mtx_);
    if (!shown_) {
        shown_ = true;
        on_first_show();
    }
    render(b);
}

void ApiView::fetch(const std::string& name, const std::string& method, const std::string& sub_with_query,
                    const std::string& body, std::function<void(const ApiResult&)> then) {
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        slots_[name].loading = true;
    }
    ++revision_;
    auto alive = alive_;
    api_->call(method, sub_with_query, body, [this, alive, name, then = std::move(then)](ApiResult r) {
        if (!alive->load()) return;
        {
            std::lock_guard<std::recursive_mutex> lk(mtx_);
            Slot& s   = slots_[name];
            s.loading = false;
            s.loaded  = true;
            s.result  = std::move(r);
            if (then) then(s.result);
        }
        ++revision_;
    });
}

const Slot& ApiView::slot(const std::string& name) const {
    static const Slot empty;
    const auto it = slots_.find(name);
    return it == slots_.end() ? empty : it->second;
}

bool ApiView::slot_state(ui::ViewBuilder& b, const std::string& name) const {
    const Slot& s = slot(name);
    if (!s.loaded) {
        b.muted(s.loading ? "\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF\xE4\xB8\xAD\xE2\x80\xA6" : "\xE2\x80\x94");   // 読み込み中… / —
        return false;
    }
    if (!s.result.ok()) {
        b.label(s.result.error_text(), &b.theme().error);
        return false;
    }
    return true;
}

void ApiView::set_notice(std::string text, bool ok) {
    std::lock_guard<std::recursive_mutex> lk(mtx_);
    notice_    = std::move(text);
    notice_ok_ = ok;
    ++revision_;
}

void ApiView::render_notice(ui::ViewBuilder& b) const {
    if (notice_.empty()) return;
    b.label(notice_, notice_ok_ ? &b.theme().ok : &b.theme().error);
}

} // namespace ergo::studio::views
