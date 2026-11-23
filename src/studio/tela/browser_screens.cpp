#include "ergo/studio/tela/browser_screens.h"

#include <tela/runtime.hpp>
#include <tela/web_session.hpp>

#include <atomic>
#include <deque>
#include <string>
#include <utility>

namespace ergo::studio::tela_shell {

struct BrowserScreens::Page {
    Page(std::shared_ptr<net::IWsConnection> c, ScreenAccess a) : conn(std::move(c)), access(a) {}

    std::shared_ptr<net::IWsConnection> conn;
    ScreenAccess                        access;

    // Shared with the connection thread.
    std::mutex              mtx;
    std::deque<std::string> inbox;
    std::atomic<bool>       closed{false};

    // UI thread only.
    ::tela::Runtime                     runtime;
    std::unique_ptr<::tela::WebSession> session;
    uint64_t                            declared = 0;   ///< declaration revision the runtime holds (0 = none)
    bool                                warned   = false;
};

BrowserScreens::BrowserScreens(net::LogFn log, std::size_t max_pages) : log_(std::move(log)), max_pages_(max_pages) {}

BrowserScreens::~BrowserScreens() {
    for (auto& [id, page] : pages_) {
        if (page->session) page->session->disconnect();
    }
}

net::WsHandlers BrowserScreens::open(std::shared_ptr<net::IWsConnection> conn, ScreenAccess access) {
    auto page = std::make_shared<Page>(conn, access);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (live_ >= max_pages_) {
            if (log_) log_("warn", "[shell] too many browser pages; refused one");
            conn->close(1013);
            return {};
        }
        ++live_;
        opened_.push_back(page);
    }
    std::weak_ptr<Page> weak = page;
    net::WsHandlers h;
    h.on_text = [weak](const std::string& text) {
        if (auto p = weak.lock()) {
            std::lock_guard<std::mutex> lk(p->mtx);
            p->inbox.push_back(text);
        }
    };
    h.on_close = [this, weak] {
        if (auto p = weak.lock()) {
            if (!p->closed.exchange(true)) {
                std::lock_guard<std::mutex> lk(mtx_);
                --live_;
            }
        }
    };
    return h;
}

void BrowserScreens::pump(const StudioDeclaration& decl) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& p : opened_) {
            p->runtime.theme(decl.theme());
            p->session = std::make_unique<::tela::WebSession>(p->runtime);
            pages_.emplace(p->conn->id(), std::move(p));
        }
        opened_.clear();
    }
    for (auto it = pages_.begin(); it != pages_.end();) {
        Page& p = *it->second;
        if (p.closed.load() || !p.conn->is_open()) {
            p.session->disconnect();
            if (!p.closed.exchange(true)) {
                std::lock_guard<std::mutex> lk(mtx_);
                --live_;
            }
            it = pages_.erase(it);
            continue;
        }
        if (p.declared != decl.revision()) {
            p.runtime.document(decl.document(p.access));
            p.declared = decl.revision();
        }
        std::deque<std::string> inbox;
        {
            std::lock_guard<std::mutex> lk(p.mtx);
            inbox.swap(p.inbox);
        }
        for (const auto& m : inbox) {
            if (!p.session->receive(m) && !p.warned && log_) {
                p.warned = true;   // once per page: a broken page must not flood the log
                log_("warn", "[shell] page " + std::to_string(it->first) + ": " + p.session->last_error());
            }
        }
        for (const auto& out : p.session->poll()) p.conn->send_text(out);
        ++it;
    }
}

} // namespace ergo::studio::tela_shell
