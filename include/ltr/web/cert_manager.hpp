#pragma once

#include <filesystem>
#include <string>

namespace ltr::web {

// V1.6.4 — Sprint Sécurité
// Gère le certificat auto-signé pour HTTPS LAN.
// Persisté dans cfgDir/cert.pem + cfgDir/key.pem (RSA 2048, 5 ans).
struct CertPair {
    std::string certPem;            // PEM-encoded X509 cert
    std::string keyPem;              // PEM-encoded RSA private key
    std::string fingerprintSha256;  // hex "AB:CD:EF:..." majuscules
};

// Charge le cert/key existant ou en génère un nouveau (RSA 2048,
// CN=LocalTransfer, validity 5 ans). Persiste dans cfgDir.
// Retourne CertPair vide en cas d'échec catastrophique (openssl).
CertPair loadOrGenerate(const std::filesystem::path& cfgDir);

} // namespace ltr::web
