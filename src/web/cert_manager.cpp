#include "ltr/web/cert_manager.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  pragma comment(lib, "iphlpapi.lib")
#else
#  include <arpa/inet.h>
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#endif

#include "ltr/core/logger.hpp"

namespace ltr::web {

namespace {

// V1.6.4 fix : 397 jours, plafond accepté par Chrome/Safari/iOS depuis 2020.
// Les browsers refusent purement les certs > 825 j et la limite a été
// abaissée à 397 j en sept. 2020.
constexpr long kValiditySec = 397L * 24L * 3600L;

// Enumère les IPv4 non-loopback "up" (Wi-Fi + Ethernet). Sert à peupler
// l'extension SAN du cert pour que le navigateur d'un device LAN
// l'accepte (Chrome/Safari refusent net les certs sans SAN matchant
// l'IP d'accès).
std::vector<std::string> enumerateLanIPv4() {
    std::vector<std::string> ips;
#ifdef _WIN32
    ULONG bufLen = 16 * 1024;
    std::vector<unsigned char> buf(bufLen);
    auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                              | GAA_FLAG_SKIP_DNS_SERVER, nullptr, addrs, &bufLen)
        != NO_ERROR) {
        return ips;
    }
    for (auto* ad = addrs; ad; ad = ad->Next) {
        if (ad->OperStatus != IfOperStatusUp) continue;
        if (ad->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* ua = ad->FirstUnicastAddress; ua; ua = ua->Next) {
            if (!ua->Address.lpSockaddr) continue;
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char str[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str))) {
                ips.emplace_back(str);
            }
        }
    }
#else
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return ips;
    for (auto* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if ((ifa->ifa_flags & IFF_UP) == 0) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        char str[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str))) {
            ips.emplace_back(str);
        }
    }
    freeifaddrs(ifap);
#endif
    return ips;
}

// Ajoute une extension X509v3 par NID via la string OpenSSL ("IP:1.2.3.4,
// DNS:foo"). Retourne true si OK.
bool addExt(X509* cert, int nid, const std::string& value) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
    // OpenSSL 1.1 attend char*, OpenSSL 3.x attend const char*. const_cast
    // est sûr car les API ne modifient pas la string.
    char* mutableValue = const_cast<char*>(value.c_str());
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, mutableValue);
    if (!ext) return false;
    const bool ok = X509_add_ext(cert, ext, -1) != 0;
    X509_EXTENSION_free(ext);
    return ok;
}

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

    // 2. Génère le cert X509v3 self-signed.
    X509* cert = X509_new();
    if (!cert) { EVP_PKEY_free(pkey); return out; }
    // Version 2 = X509v3 (0-indexé). REQUIS pour autoriser les extensions
    // SAN/KU/EKU. Sans ça les browsers refusent en bloc.
    X509_set_version(cert, 2);
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

    // Extensions X509v3 — SAN obligatoire pour iOS Safari / Chrome moderne
    // qui rejettent net les certs sans SAN matchant l'hôte d'accès.
    std::string san = "DNS:localhost,IP:127.0.0.1,IP:0:0:0:0:0:0:0:1";
    const auto lanIps = enumerateLanIPv4();
    for (const auto& ip : lanIps) {
        san += ",IP:" + ip;
    }
    addExt(cert, NID_subject_alt_name, san);
    addExt(cert, NID_basic_constraints, "critical,CA:FALSE");
    addExt(cert, NID_key_usage,
           "critical,digitalSignature,keyEncipherment");
    addExt(cert, NID_ext_key_usage, "serverAuth");
    addExt(cert, NID_subject_key_identifier, "hash");

    core::log_info("[cert] SAN = " + san);

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
                // V1.6.4 fix : refuse les vieux certs v1 ou sans SAN
                // (générés par la 1re version de cert_manager). Les
                // browsers les rejettent net depuis un device LAN.
                const long ver = X509_get_version(cert);
                const bool hasSan =
                    X509_get_ext_by_NID(cert, NID_subject_alt_name, -1) >= 0;
                if (ver < 2 || !hasSan) {
                    X509_free(cert);
                    core::log_warn("[cert] vieux cert détecté (v="
                                   + std::to_string(ver + 1)
                                   + " san=" + (hasSan ? "1" : "0")
                                   + ") — régénération");
                } else {
                    out.fingerprintSha256 = fingerprintFromCert(cert);
                    X509_free(cert);
                    core::log_info("[cert] loaded existing cert, fp="
                                   + out.fingerprintSha256.substr(0, 23)
                                   + "...");
                    return out;
                }
            }
        } else {
            core::log_warn("[cert] existing cert files unreadable, regenerating");
        }
    }

    core::log_info("[cert] generating new self-signed X509v3 RSA 2048 cert");
    auto pair = generateNew();
    if (pair.certPem.empty()) {
        core::log_error("[cert] generation FAILED");
        return pair;
    }

    std::error_code rmEc;
    fs::remove(certPath, rmEc);
    fs::remove(keyPath, rmEc);
    std::ofstream(certPath) << pair.certPem;
    std::ofstream(keyPath)  << pair.keyPem;
    core::log_info("[cert] saved to " + cfgDir.string()
                   + " fp=" + pair.fingerprintSha256.substr(0, 23) + "...");
    return pair;
}

} // namespace ltr::web
