#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <portus/json.hpp>

struct evp_pkey_st;

namespace ergo::studio::registry {

using PublicKey = std::unique_ptr<evp_pkey_st, void (*)(evp_pkey_st*)>;

PublicKey verified_signer(const std::string& root, const std::string& signer,
                          std::time_t now, std::string& fingerprint);
bool verifies(evp_pkey_st* key, const portus::Json& document);

} // namespace ergo::studio::registry
