// Non-Windows builds: the HTTP client, the credential store, the process
// runner and the clipboard are not implemented yet. They fail loudly instead
// of switching to another path.
#include "ergo/studio/platform/clipboard.h"
#include "ergo/studio/platform/credential_store.h"
#include "ergo/studio/platform/http_client.h"
#include "ergo/studio/platform/process_runner.h"

namespace ergo::studio::platform {
namespace {

const char* const kUnsupported = "not supported on this platform (ergo_studio_platform has a Windows backend only)";

class UnsupportedHttpClient final : public IHttpClient {
public:
    HttpClientResponse send(const HttpClientRequest&) override {
        HttpClientResponse r;
        r.error = kUnsupported;
        return r;
    }
};

class UnsupportedCredentialStore final : public ICredentialStore {
public:
    std::optional<std::string> get(const std::string&, const std::string&, std::string& err) override {
        err = kUnsupported;
        return std::nullopt;
    }
    bool set(const std::string&, const std::string&, const std::string&, std::string& err) override {
        err = kUnsupported;
        return false;
    }
    bool remove(const std::string&, const std::string&, std::string& err) override {
        err = kUnsupported;
        return false;
    }
};

class UnsupportedProcessRunner final : public IProcessRunner {
public:
    ProcessResult run(const ProcessSpec&) override {
        ProcessResult r;
        r.error = kUnsupported;
        return r;
    }
};

class UnsupportedClipboard final : public IClipboard {
public:
    ClipboardText read_text() override {
        ClipboardText r;
        r.error = kUnsupported;
        return r;
    }
};

} // namespace

std::shared_ptr<IClipboard>       make_system_clipboard() { return std::make_shared<UnsupportedClipboard>(); }
std::shared_ptr<IProcessRunner>   make_system_process_runner() { return std::make_shared<UnsupportedProcessRunner>(); }
std::optional<std::filesystem::path> find_on_path(const std::string&) { return std::nullopt; }
std::shared_ptr<IHttpClient>      make_system_http_client() { return std::make_shared<UnsupportedHttpClient>(); }
std::shared_ptr<ICredentialStore> make_system_credential_store() { return std::make_shared<UnsupportedCredentialStore>(); }

} // namespace ergo::studio::platform
