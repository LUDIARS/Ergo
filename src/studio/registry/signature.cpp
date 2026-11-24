#include "signature.h"

#include "canonical.h"
#include "ergo/studio/registry/trust.h"

#include <lorica/checked_size.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <portus/crypto.hpp>

namespace ergo::studio::registry {
namespace {

using Certificate = std::unique_ptr<X509, decltype(&X509_free)>;

Certificate read_certificate(const std::string& pem) {
    if (!lorica::contains_range(kCertificateLimit, 0, pem.size())) return {nullptr, X509_free};
    const std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    return {bio ? PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) : nullptr, X509_free};
}

} // namespace

PublicKey verified_signer(const std::string& root_pem, const std::string& signing_pem,
                          std::time_t now, std::string& fingerprint) {
    PublicKey failure(nullptr, EVP_PKEY_free);
    auto root = read_certificate(root_pem);
    auto signer = read_certificate(signing_pem);
    if (!root || !signer || X509_cmp(root.get(), signer.get()) == 0 || X509_check_ca(signer.get()) != 0
        || X509_check_ca(root.get()) <= 0) return failure;
    const PublicKey root_key(X509_get_pubkey(root.get()), EVP_PKEY_free);
    if (!root_key || EVP_PKEY_base_id(root_key.get()) != EVP_PKEY_ED25519) return failure;
    PublicKey key(X509_get_pubkey(signer.get()), EVP_PKEY_free);
    if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_ED25519) return failure;
    const auto usage = X509_get_key_usage(signer.get());
    if ((usage & KU_DIGITAL_SIGNATURE) == 0) return failure;

    const std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> store(X509_STORE_new(), X509_STORE_free);
    const std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)> chain(X509_STORE_CTX_new(), X509_STORE_CTX_free);
    if (!store || !chain || X509_STORE_add_cert(store.get(), root.get()) != 1
        || X509_STORE_CTX_init(chain.get(), store.get(), signer.get(), nullptr) != 1) return failure;
    X509_STORE_CTX_set_time(chain.get(), 0, now);
    if (X509_verify_cert(chain.get()) != 1) return failure;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned size = 0;
    if (X509_digest(signer.get(), EVP_sha256(), digest, &size) != 1 || size != 32) return failure;
    fingerprint = portus::hex(std::string_view(reinterpret_cast<const char*>(digest), size));
    return key;
}

bool verifies(EVP_PKEY* key, const portus::Json& document) {
    const auto& encoded = document.at("signature").as_string();
    std::string signature;
    if (encoded.size() != 88 || !portus::base64_decode(encoded, signature) || signature.size() != 64
        || portus::base64_encode(signature) != encoded) return false;
    const auto payload = signing_payload(document);
    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key) == 1
        && EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
                            reinterpret_cast<const unsigned char*>(payload.data()), payload.size()) == 1;
}

} // namespace ergo::studio::registry
