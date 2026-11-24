#include "ws_session.h"

#include <exception>
#include <string_view>
#include <utility>

namespace ergo::studio::web {

void PortusWsConnection::send_text(const std::string& payload) {
    if (!open_.load()) return;
    if (auto ws = ws_.lock()) ws->send(payload);
}

void PortusWsConnection::close(uint16_t code) {
    if (!open_.exchange(false)) return;
    if (auto ws = ws_.lock()) ws->close(code);
}

bool PortusWsConnection::is_open() const {
    if (!open_.load()) return false;
    const auto ws = ws_.lock();
    return ws && ws->ready_state() == portus::WebSocket::OPEN;
}

WsSession::WsSession() : thread_([this] { run(); }) {}

WsSession::~WsSession() {
    end();
    if (thread_.joinable()) thread_.join();
}

void WsSession::post(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (ending_) return;
        queue_.push_back(std::move(task));
    }
    wake_.notify_one();
}

void WsSession::end() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        ending_ = true;
    }
    wake_.notify_one();
}

void WsSession::run() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mutex_);
            wake_.wait(lk, [this] { return ending_ || !queue_.empty(); });
            if (queue_.empty()) break;   // ending and drained
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();
    }
    finished_ = true;
}

void WsSession::bind(const std::shared_ptr<portus::WebSocket>& ws, std::shared_ptr<PortusWsConnection> conn,
                     net::IWsAcceptor& acceptor, const std::string& plugin_id, const net::HttpRequest& req,
                     net::LogFn log) {
    // The callbacks hold the session through a raw pointer: the web server owns
    // every session until its thread has finished (after the close below).
    WsSession* self = this;
    post([self, conn, &acceptor, plugin_id, req, log] {
        try {
            self->handlers_ = acceptor.accept(plugin_id, req, conn);
        } catch (const std::exception& e) {
            if (log) log("error", "[ergo] plugin '" + plugin_id + "' WS accept threw: " + e.what());
            conn->close(1011);
        }
    });
    ws->on_message = [self, plugin_id, log](std::string_view data, bool is_binary) {
        if (is_binary) return;   // binary frames are ignored, as before
        std::string text(data);
        self->post([self, plugin_id, log, text = std::move(text)] {
            if (!self->handlers_.on_text) return;
            try {
                self->handlers_.on_text(text);
            } catch (const std::exception& e) {
                if (log) log("error", "[ergo] plugin '" + plugin_id + "' WS handler threw: " + e.what());
            }
        });
    };
    ws->on_close = [self, conn](int, std::string_view) {
        conn->mark_closed();
        self->post([self] {
            if (!self->handlers_.on_close) return;
            try {
                self->handlers_.on_close();
            } catch (...) {
            }
        });
        self->end();
    };
}

} // namespace ergo::studio::web
