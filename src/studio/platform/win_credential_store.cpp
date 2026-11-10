// Windows Credential Manager implementation of ICredentialStore.
#include "ergo/studio/platform/credential_store.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>

#include <vector>

namespace ergo::studio::platform {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const wchar_t* w, std::size_t len) {
    if (len == 0) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), s.data(), n, nullptr, nullptr);
    return s;
}

/// keyring-rs (windows-native) target name: "<user>.<service>".
std::wstring target(const std::string& service, const std::string& account) { return widen(account + "." + service); }

class WinCredentialStore final : public ICredentialStore {
public:
    std::optional<std::string> get(const std::string& service, const std::string& account, std::string& err) override {
        PCREDENTIALW cred = nullptr;
        if (!CredReadW(target(service, account).c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
            const DWORD e = GetLastError();
            if (e != ERROR_NOT_FOUND) err = "CredRead failed (error " + std::to_string(e) + ")";
            return std::nullopt;
        }
        // The secret is UTF-16LE without a terminator (keyring-rs layout).
        std::string secret = narrow(reinterpret_cast<const wchar_t*>(cred->CredentialBlob), cred->CredentialBlobSize / 2);
        CredFree(cred);
        return secret;
    }

    bool set(const std::string& service, const std::string& account, const std::string& secret, std::string& err) override {
        std::wstring name = target(service, account);
        std::wstring user = widen(account);
        std::wstring blob = widen(secret);
        CREDENTIALW c{};
        c.Type               = CRED_TYPE_GENERIC;
        c.TargetName         = name.data();
        c.UserName           = user.data();
        c.CredentialBlobSize = static_cast<DWORD>(blob.size() * sizeof(wchar_t));
        c.CredentialBlob     = reinterpret_cast<LPBYTE>(blob.data());
        c.Persist            = CRED_PERSIST_ENTERPRISE;
        if (!CredWriteW(&c, 0)) {
            err = "CredWrite failed (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        return true;
    }

    bool remove(const std::string& service, const std::string& account, std::string& err) override {
        if (!CredDeleteW(target(service, account).c_str(), CRED_TYPE_GENERIC, 0)) {
            const DWORD e = GetLastError();
            if (e == ERROR_NOT_FOUND) return true;
            err = "CredDelete failed (error " + std::to_string(e) + ")";
            return false;
        }
        return true;
    }
};

} // namespace

std::shared_ptr<ICredentialStore> make_system_credential_store() { return std::make_shared<WinCredentialStore>(); }

} // namespace ergo::studio::platform
