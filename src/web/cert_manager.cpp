#include "ltr/web/cert_manager.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include "ltr/core/logger.hpp"

namespace ltr::web {

namespace {

// 5 ans en secondes (V1.6.4 décision BA #1).
constexpr long kValiditySec = 5L * 365L * 24L * 3600L;

// Encode un BIO en string.
std::string bioToString(BIO* bio) {
    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);
    if (!mem || !mem->data) return {};
    return std::string(mem->data, mem->length);
}

// Convertit un X509 → PEM string.
std::string certToPem(X509* cert) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return {};
    PEM_write_bio_X509(bio, cert);
    auto pem = bioToString(bio);
    BIO_free(bio);
    return pem;
}

// Convertit un EVP_PKEY → PEM string.
std::string keyToPem(EVP_PKEY* key) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return {};
    PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
    auto pem = bioToString(bio);
    BIO_free(bio);
    return pem;
}

// Charge un X509 depuis un PEM string.
X509* certFromPem(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) return nullptr;
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return cert;
}

// Calcule SHA-256(DER cert) et retourne la version hex format
// "AB:CD:EF:..." en majuscules.
std::string fingerprintFromCert(X509* cert) {
    if (!cert) return {};
    unsigned char* der = nullptr;
    int derLen = i2d_X509(cert, &der);
    if (derLen <= 0) return {};
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(der, derLen, hash.data());
    OPENSSL_free(der);

    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(SHA256_DIGEST_LENGTH * 3);
    for (size_t i = 0; i < hash.size(); ++i) {
        if (i > 0) out.push_back(':');
        out.push_back(kHex[(hash[i] >> 4) & 0xF]);
        out.push_back(kHex[hash[i] & 0xF]);
    }
    return out;
}

// Lit un fichier texte complet.
std::string readFile(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Génère un nouveau cert RSA 2048 self-signed (CN=LocalTransfer, 5 ans).
CertPair generateNew() {
    CertPair out;

    // 1. Génère la clé RSA 2048.
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) return out;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) { EVP_PKEY_free(pkey); return out; }
    if (EVP_PKEY_keygen_init(ctx) <= 0
        || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0
        || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return out;
    }
    EVP_PKEY_CTX_free(ctx);

    // 2. Génère le cert X509 self-signed.
    X509* cert = X509_new();
    if (!cert) { EVP_PKEY_free(pkey); return out; }
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), kValiditySec);
    X509_set_pubkey(cert, pkey);

    // CN=LocalTransfer, O=LocalTransfer, C=FR
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("FR"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("LocalTransfer"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("LocalTransfer"), -1, -1, 0);
    X509_set_issuer_name(cert, name);  // self-signed

    // Sign avec SHA-256.
    if (X509_sign(cert, pkey, EVP_sha256()) == 0) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return out;
    }

    out.certPem = certToPem(cert);
    out.keyPem = keyToPem(pkey);
    out.fingerprintSha256 = fingerprintFromCert(cert);

    X509_free(cert);
    EVP_PKEY_free(pkey);
    return out;
}

} // namespace

CertPair loadOrGenerate(const std::filesystem::path& cfgDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(cfgDir, ec);

    const auto certPath = cfgDir / "cert.pem";
    const auto keyPath  = cfgDir / "key.pem";

    if (fs::exists(certPath, ec) && fs::exists(keyPath, ec)) {
        CertPair out;
        out.certPem = readFile(certPath);
        out.keyPem  = readFile(keyPath);
        if (!out.certPem.empty() && !out.keyPem.empty()) {
            X509* cert = certFromPem(out.certPem);
            if (cert) {
                out.fingerprintSha256 = fingerprintFromCert(cert);
                X509_free(cert);
                core::log_info("[cert] loaded existing cert, fp="
                               + out.fingerprintSha256.substr(0, 23) + "...");
                return out;
            }
        }
        core::log_warn("[cert] existing cert invalid, regenerating");
    }

    core::log_info("[cert] generating new self-signed RSA 2048 cert (5y)");
    auto pair = generateNew();
    if (pair.certPem.empty()) {
        core::log_error("[cert] generation FAILED");
        return pair;
    }

    std::ofstream(certPath) << pair.certPem;
    std::ofstream(keyPath)  << pair.keyPem;
    core::log_info("[cert] saved to " + cfgDir.string()
                   + " fp=" + pair.fingerprintSha256.substr(0, 23) + "...");
    return pair;
}

} // namespace ltr::web
