#pragma once
/// Adapts one ergo_studio_plugin_v1 (C ABI) to the C++ plugin contract.
/// Internal to ergo_studio_host.
#include "dynamic_library.h"

#include "ergo/studio/host/plugin.h"
#include "ergo/studio/plugin_abi.h"

#include <map>
#include <memory>
#include <mutex>

namespace ergo::studio::host {

class AbiPlugin final : public IPlugin, public IPluginHttp, public IPluginSocket, public IPluginHealth {
    struct Key {   ///< only create() can make one, so only create() constructs
        explicit Key() = default;
    };

public:
    /// Calls the pack's create function. Throws std::runtime_error when it
    /// returns nothing, a different ABI version, or an incomplete table.
    static std::unique_ptr<IPlugin> create(std::shared_ptr<DynamicLibrary> lib, ergo_studio_plugin_v1_create_fn fn,
                                           LogFn log);
    AbiPlugin(Key, std::shared_ptr<DynamicLibrary> lib, LogFn log);
    ~AbiPlugin() override;

    const PluginInfo& info() const override { return info_; }
    IPluginHttp*      http() override { return p_->http ? this : nullptr; }
    IPluginSocket*    socket() override { return p_->ws_open ? this : nullptr; }
    IPluginHealth*    health() override { return p_->health ? this : nullptr; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

private:
    std::shared_ptr<net::IWsConnection> find_conn(uint64_t id);
    void                                forget_conn(uint64_t id);

    static void host_log(void* ctx, const char* level, ergo_studio_str_v1 msg);
    static void host_ws_send(void* ctx, uint64_t conn, ergo_studio_str_v1 text);
    static void host_ws_close(void* ctx, uint64_t conn, uint16_t code);

    // Declared first: the library is unloaded only after everything else.
    std::shared_ptr<DynamicLibrary>                          lib_;
    LogFn                                                    log_;
    ergo_studio_host_v1                                      host_{};
    const ergo_studio_plugin_v1*                             p_ = nullptr;
    PluginInfo                                               info_;
    std::mutex                                               conns_mtx_;
    std::map<uint64_t, std::weak_ptr<net::IWsConnection>>    conns_;
};

} // namespace ergo::studio::host
