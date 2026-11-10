#pragma once
/// OS credential storage for secrets plugins must not write to disk in plain
/// text (Melpomene's GitHub token). Windows: Credential Manager, generic
/// credentials named "<account>.<service>" holding the secret as UTF-16LE —
/// the layout of the Rust keyring crate (v3, windows-native), so a token
/// saved by the Ars-Musa Tauri app is found unchanged.
#include <memory>
#include <optional>
#include <string>

namespace ergo::studio::platform {

class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;
    /// nullopt when there is no such credential. `err` set on a real failure.
    virtual std::optional<std::string> get(const std::string& service, const std::string& account, std::string& err) = 0;
    virtual bool set(const std::string& service, const std::string& account, const std::string& secret, std::string& err) = 0;
    /// True also when there was nothing to remove.
    virtual bool remove(const std::string& service, const std::string& account, std::string& err) = 0;
};

std::shared_ptr<ICredentialStore> make_system_credential_store();

} // namespace ergo::studio::platform
