#include "canonical.h"
#include "ergo/studio/ingress/remote_scope.h"
#include "ergo/studio/registry/fetch.h"
#include "plugins/studio/plugin_registry.h"

#include <gtest/gtest.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <portus/crypto.hpp>

#include <memory>
#include <stdexcept>

namespace {

namespace reg = ergo::studio::registry;
namespace studio = ergo::studio::plugins::studio;
using portus::Json;

using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
const std::time_t kNow = std::time(nullptr);
const std::string kRev(40, 'a');

void require(bool value) {
    if (!value) throw std::runtime_error("OpenSSL test fixture failed");
}

Key key() {
    Key result(EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), EVP_PKEY_free);
    require(!!result);
    return result;
}

Cert certificate(EVP_PKEY* public_key, const char* name, X509* root, EVP_PKEY* issuer, bool expired = false) {
    Cert cert(X509_new(), X509_free);
    require(cert && X509_set_version(cert.get(), 2) == 1);
    require(ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), root ? 2 : 1) == 1);
    const auto* cn = reinterpret_cast<const unsigned char*>(name);
    auto* subject = X509_get_subject_name(cert.get());
    require(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, cn, -1, -1, 0) == 1
        && X509_set_issuer_name(cert.get(), root ? X509_get_subject_name(root) : subject) == 1
        && X509_set_pubkey(cert.get(), public_key) == 1
        && ASN1_TIME_set(X509_getm_notBefore(cert.get()), kNow - 3600)
        && ASN1_TIME_set(X509_getm_notAfter(cert.get()), kNow + (expired ? -1 : 3600)));
    const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> extension(
        X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, root ? "critical,CA:FALSE" : "critical,CA:TRUE"),
        X509_EXTENSION_free);
    require(extension && X509_add_ext(cert.get(), extension.get(), -1) == 1
        && X509_sign(cert.get(), issuer, nullptr) > 0);
    return cert;
}

std::string pem(X509* cert) {
    Bio bio(BIO_new(BIO_s_mem()), BIO_free);
    require(bio && PEM_write_bio_X509(bio.get(), cert) == 1);
    const char* data = nullptr;
    const auto size = BIO_get_mem_data(bio.get(), &data);
    require(size > 0);
    return std::string(data, static_cast<std::size_t>(size));
}

std::string signed_document(Json document, EVP_PKEY* signer) {
    const auto payload = reg::signing_payload(document);
    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    unsigned char signature[64];
    std::size_t size = sizeof(signature);
    require(ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, signer) == 1
        && EVP_DigestSign(ctx.get(), signature, &size, reinterpret_cast<const unsigned char*>(payload.data()), payload.size()) == 1
        && size == sizeof(signature));
    document.set("signature", portus::base64_encode(
        std::string_view(reinterpret_cast<const char*>(signature), size)));
    return document.dump();
}

struct Fixture {
    Key root_key = key();
    Key signer_key = key();
    Cert root = certificate(root_key.get(), "test root", nullptr, root_key.get());
    Cert signer = certificate(signer_key.get(), "test signer", root.get(), root_key.get());
    Json index = Json::object({{"version", 1}, {"serial", 8}, {"signed_at", "2027-01-15T08:00:00Z"},
        {"plugins", Json::array({Json::object({{"id", "test/pack"}, {"title", "Example"}, {"publisher", "test"},
            {"abi", 1}, {"kind", "pack"}, {"source", "Test/Repo:pack@" + kRev},
            {"domains", Json::array({"game"})}, {"styles", Json::array({"cpp"})}})})}});
    Json revoked = Json::object({{"version", 1}, {"serial", 8}, {"signed_at", "2027-01-15T08:00:00Z"},
        {"certificates", Json::array()}, {"plugins", Json::array()}});

    reg::Release release() const {
        return {signed_document(index, signer_key.get()), signed_document(revoked, signer_key.get()), pem(signer.get())};
    }
    reg::Trust verify(std::uint64_t minimum = 0) const {
        return reg::verify_release(release(), pem(root.get()), minimum, kNow);
    }
};

class FakeTransport final : public reg::IAssetTransport {
public:
    reg::AssetResponse get(const std::string& url, int, std::size_t) override {
        calls.push_back(url);
        if (url.find("https://github.com/") == 0) {
            return {302, first.empty() ? "https://release-assets.githubusercontent.com/files/" +
                url.substr(url.rfind('/') + 1) : first, {}};
        }
        return {200, {}, "{}"};
    }
    std::string first;
    std::vector<std::string> calls;
};

class FixtureTransport final : public reg::IAssetTransport {
public:
    explicit FixtureTransport(reg::Release release) : release_(std::move(release)) {}

    reg::AssetResponse get(const std::string& url, int, std::size_t) override {
        const auto name = url.substr(url.rfind('/') + 1);
        if (name == "index.signed.json") return {200, {}, release_.index};
        if (name == "revoked.signed.json") return {200, {}, release_.revoked};
        if (name == "signing.pem") return {200, {}, release_.signing_pem};
        return {};
    }

private:
    reg::Release release_;
};

struct TemporaryHome {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("ergo-studio-registry-" + portus::hex(portus::random_bytes(12)));
    ~TemporaryHome() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST(PluginRegistry, SignatureAndCanonicalJson) {
    // Expected UTF-8 bytes from ErgoPlugin canonicalJson: UTF-16 key sort, short escapes,
    // safe integer formatting, nested signature retained, and unpaired surrogate escaped.
    const auto input = reg::parse_document(R"({"\ue000":1,"\ud83d\ude00":2,"z":{"signature":"keep"},"control":"\b\f\u007f\ud800","serial":9007199254740991,"signature":"drop","negative_zero":-0})");
    const std::string expected = "{\"control\":\"\\b\\f\x7f\\ud800\",\"negative_zero\":0,\"serial\":9007199254740991,"
        "\"z\":{\"signature\":\"keep\"},\"\xf0\x9f\x98\x80\":2,\"\xee\x80\x80\":1}";
    EXPECT_EQ(reg::signing_payload(input), expected);
    Fixture fixture;
    const auto trust = fixture.verify();
    ASSERT_TRUE(trust.verified());
    EXPECT_EQ(trust.serial, 8u);
    EXPECT_EQ(trust.entries.at(0).rev, kRev);
}

TEST(PluginRegistry, RejectsWrongRootAndExpiredSigner) {
    Fixture fixture;
    const auto another = key();
    const auto other_root = certificate(another.get(), "another root", nullptr, another.get());
    EXPECT_FALSE(reg::verify_release(fixture.release(), pem(other_root.get()), 0, kNow).verified());
    fixture.signer = certificate(fixture.signer_key.get(), "test signer", fixture.root.get(), fixture.root_key.get(), true);
    EXPECT_FALSE(fixture.verify().verified());
}

TEST(PluginRegistry, RejectsTamperingAndRollback) {
    Fixture fixture;
    auto release = fixture.release();
    release.index.replace(release.index.find("Example"), 1, "e");
    EXPECT_FALSE(reg::verify_release(release, pem(fixture.root.get()), 0, kNow).verified());
    EXPECT_FALSE(fixture.verify(9).verified());
    EXPECT_TRUE(fixture.verify(8).verified());
    fixture.revoked.set("serial", 9);
    EXPECT_FALSE(fixture.verify().verified());
}

TEST(PluginRegistry, RevocationFailsClosed) {
    Fixture fixture;
    const auto trust = fixture.verify();
    ASSERT_TRUE(trust.verified());
    auto changed = fixture.release();
    changed.revoked.replace(changed.revoked.find("\"version\":1"), 11, "\"version\":2");
    EXPECT_FALSE(reg::verify_release(changed, pem(fixture.root.get()), 0, kNow).verified());
    fixture.revoked.set("plugins", Json::array({Json::object({{"id", "test/pack"}, {"rev", kRev}})}));
    const auto version_revoked = fixture.verify();
    ASSERT_TRUE(version_revoked.verified());
    EXPECT_FALSE(version_revoked.entries.at(0).reason.empty());
    const reg::Provenance installed{"test/pack", "test", trust.fingerprint, "test", 8};
    EXPECT_TRUE(trust.permits(installed, kRev));
    EXPECT_FALSE(version_revoked.permits(installed, kRev));
    fixture.revoked.set("certificates", Json::array({Json::object({{"fingerprint", trust.fingerprint}})}));
    EXPECT_FALSE(fixture.verify().verified());
}

TEST(PluginRegistry, MissingRootKeepsUntrustedEntries) {
    const Fixture fixture;
    const auto trust = reg::verify_release(fixture.release(), "", 0, kNow);
    EXPECT_FALSE(trust.verified());
    EXPECT_EQ(trust.entries.size(), 1u);
    EXPECT_FALSE(trust.reason.empty());
}

TEST(PluginRegistry, PersistsSerialAndFiltersInstalledPacks) {
    Fixture fixture;
    TemporaryHome home;
    const auto root = pem(fixture.root.get());
    studio::PluginRegistry first(home.path, root, std::make_unique<FixtureTransport>(fixture.release()), 1000);
    const auto trust = first.refresh();
    ASSERT_TRUE(trust.verified());

    studio::Ledger ledger;
    studio::LedgerPack signed_pack;
    signed_pack.id = "Test/Repo:pack";
    signed_pack.rev = kRev;
    signed_pack.enabled = true;
    signed_pack.trust = reg::Provenance{"test/pack", "test", trust.fingerprint, "test", trust.serial};
    ledger.packs.push_back(signed_pack);
    auto manual = signed_pack;
    manual.id = "Test/Repo:manual";
    manual.trust.reset();
    ledger.packs.push_back(manual);
    std::string error;
    ASSERT_TRUE(studio::write_ledger(home.path, ledger, error));
    ledger = studio::read_ledger(home.path);
    ASSERT_EQ(ledger.packs.size(), 2u);
    ASSERT_TRUE(ledger.packs[0].trust.has_value());
    EXPECT_EQ(ledger.packs[0].trust->fingerprint, trust.fingerprint);
    EXPECT_FALSE(ledger.packs[1].trust.has_value());

    fixture.revoked.set("plugins", Json::array({Json::object({{"id", "test/pack"}, {"rev", kRev}})}));
    studio::PluginRegistry revoked(home.path, root, std::make_unique<FixtureTransport>(fixture.release()), 1000);
    revoked.filter_startup(ledger);
    EXPECT_FALSE(ledger.packs[0].enabled);
    EXPECT_TRUE(ledger.packs[1].enabled);

    fixture.index.set("serial", 7);
    fixture.revoked.set("serial", 7);
    studio::PluginRegistry restarted(home.path, root, std::make_unique<FixtureTransport>(fixture.release()), 1000);
    EXPECT_FALSE(restarted.refresh().verified());
}

TEST(PluginRegistry, FetchesOnlyAllowedReleaseAssets) {
    FakeTransport fake;
    EXPECT_TRUE(reg::fetch_release(fake, 500).has_value());
    ASSERT_EQ(fake.calls.size(), 6u);
    const std::string base = "https://github.com/LUDIARS/ErgoPlugin/releases/latest/download/";
    EXPECT_EQ(fake.calls[0], base + "index.signed.json");
    EXPECT_EQ(fake.calls[2], base + "revoked.signed.json");
    EXPECT_EQ(fake.calls[4], base + "signing.pem");
    for (const auto* url : {"https://example.test/", "http://github.com/", "https://github.com:444/",
                            "https://github.com/example", "https://github.com@example.test/",
                            "//release-assets.githubusercontent.com/"}) {
        fake.calls.clear();
        fake.first = url;
        EXPECT_FALSE(reg::fetch_release(fake, 500).has_value());
        EXPECT_EQ(fake.calls.size(), 1u);
    }
}

TEST(PluginRegistry, RemoteRegistryMutationsAreRefused) {
    using ergo::studio::ingress::remote_refusal;
    EXPECT_FALSE(remote_refusal("GET", "/studio/api/plugins/registry").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/studio/api/plugins/registry/preview").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/studio/api/plugins/install").has_value());
    EXPECT_TRUE(remote_refusal("DELETE", "/studio/api/plugins").has_value());
}
