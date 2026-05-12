#pragma once

#include <string>

namespace ltr::infra {

// Génère un token cryptographiquement imprévisible utilisé comme secret
// d'un lien de dépôt. Interface — implémentations injectées par DI.
class DepositTokenGenerator {
public:
    virtual ~DepositTokenGenerator() = default;
    // Retourne 32 octets encodés en base64url (43 chars, sans padding).
    virtual std::string generate() = 0;
};

// Implémentation par défaut : std::random_device pour le seed, puis
// std::mt19937_64 pour produire 32 octets aléatoires. Suffit pour
// rendre un brute-force LAN infaisable (256 bits d'entropie).
class SecureRandomTokenGenerator final : public DepositTokenGenerator {
public:
    SecureRandomTokenGenerator();
    std::string generate() override;
};

// Helper exposé pour les tests : encode base64url sans padding.
std::string base64UrlEncode(const unsigned char* data, std::size_t len);

} // namespace ltr::infra
