#include "log_bridge.h"

#include <memory>
#include <utility>

namespace ergo::studio::web {
namespace {

class LogFnSink final : public portus::LogSink {
public:
    explicit LogFnSink(net::LogFn log) : log_(std::move(log)) {}
    void write(const portus::LogRecord& rec) override {
        if (!log_) return;
        const char* level = rec.level >= portus::LogLevel::Error ? "error"
                          : rec.level >= portus::LogLevel::Warn  ? "warn"
                                                                 : "info";
        log_(level, portus::format_pretty(rec));
    }

private:
    net::LogFn log_;
};

} // namespace

portus::Logger studio_logger(net::LogFn log) {
    return portus::Logger("ergo-web", portus::LogLevel::Info, {std::make_shared<LogFnSink>(std::move(log))});
}

} // namespace ergo::studio::web
