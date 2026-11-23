#pragma once
/// Browser pages of the Studio (WS /shell/ws): one tela::Runtime and one
/// tela::WebSession per page (SPEC-TL-WEB). The WebSocket threads only queue
/// what a page sent; the UI thread feeds it to the page's runtime, which runs
/// the declaration's callbacks there, and sends the frames WebSession made.
#include "ergo/studio/net/server.h"
#include "ergo/studio/tela/studio_declaration.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace ergo::studio::tela_shell {

class BrowserScreens {
public:
    /// At most `max_pages` pages at once; another one is closed (1013, try again later).
    explicit BrowserScreens(net::LogFn log = {}, std::size_t max_pages = 8);
    ~BrowserScreens();
    BrowserScreens(const BrowserScreens&)            = delete;
    BrowserScreens& operator=(const BrowserScreens&) = delete;

    /// Any thread: a page opened /shell/ws with that access.
    net::WsHandlers open(std::shared_ptr<net::IWsConnection> conn, ScreenAccess access);

    /// UI thread: adopt new pages and drop closed ones, give every page the
    /// declaration when it changed, run their input, send their frames.
    void pump(const StudioDeclaration& decl);

    /// Pages adopted by the last pump().
    std::size_t size() const { return pages_.size(); }

private:
    struct Page;
    net::LogFn                                  log_;
    std::size_t                                 max_pages_;
    std::mutex                                  mtx_;
    std::vector<std::shared_ptr<Page>>          opened_;   ///< guarded by mtx_
    std::size_t                                 live_ = 0; ///< guarded by mtx_ (opened, not yet closed)
    std::map<uint64_t, std::shared_ptr<Page>>   pages_;    ///< UI thread
};

} // namespace ergo::studio::tela_shell
