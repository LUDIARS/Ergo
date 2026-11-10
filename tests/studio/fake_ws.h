#pragma once
/// Test doubles for plugin tests: an in-memory WS connection and helpers.
#include "plugins/common/json_util.h"

#include "ergo/studio/host/plugin.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace studio_test {

using ergo::studio::plugins::JsonValue;

class FakeConn final : public ergo::studio::net::IWsConnection {
public:
    explicit FakeConn(uint64_t id) : id_(id) {}
    uint64_t id() const override { return id_; }
    void     send_text(const std::string& p) override {
        std::lock_guard<std::mutex> lk(m_);
        sent_.push_back(p);
    }
    void close(uint16_t code) override {
        open_       = false;
        close_code_ = code;
    }
    bool     is_open() const override { return open_; }
    uint16_t close_code() const { return close_code_; }

    std::vector<JsonValue> frames() {
        std::lock_guard<std::mutex> lk(m_);
        std::vector<JsonValue> out;
        for (const auto& s : sent_) {
            JsonValue v;
            if (ergo::common::jsonm::parse(s, v)) out.push_back(v);
        }
        return out;
    }
    /// Frames whose op is `op`, oldest first.
    std::vector<JsonValue> all(const std::string& op) {
        std::vector<JsonValue> out;
        for (const auto& f : frames()) {
            if (ergo::studio::plugins::op_of(f) == op) out.push_back(f);
        }
        return out;
    }
    /// Last frame whose op is `op`, or a null value.
    JsonValue last(const std::string& op) {
        const auto v = all(op);
        return v.empty() ? JsonValue() : v.back();
    }

private:
    uint64_t                 id_;
    bool                     open_       = true;
    uint16_t                 close_code_ = 0;
    std::mutex               m_;
    std::vector<std::string> sent_;
};

struct Client {
    std::shared_ptr<FakeConn>        conn;
    ergo::studio::net::WsHandlers    h;
};

inline Client open(ergo::studio::host::IPluginSocket& s, uint64_t id) {
    Client c;
    c.conn = std::make_shared<FakeConn>(id);
    const std::shared_ptr<ergo::studio::net::IWsConnection> as_connection = c.conn;
    c.h = s.on_open(ergo::studio::net::HttpRequest{}, as_connection);
    return c;
}

inline ergo::studio::net::HttpRequest req(const std::string& method, const std::string& body = {}) {
    ergo::studio::net::HttpRequest r;
    r.method = method;
    r.body   = body;
    return r;
}

inline const ergo::studio::host::LogFn& quiet() {
    static const ergo::studio::host::LogFn fn = [](const std::string&, const std::string&) {};
    return fn;
}

} // namespace studio_test
