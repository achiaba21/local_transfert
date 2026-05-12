#pragma once

#include <cstdint>
#include <string>

namespace ltr::infra {

// Lien de dépôt persistant. POD sérialisable — pas de logique métier ici.
// Lifecycle géré par DepositLinkService.
struct DepositLink {
    std::string   id;                       // 12 hex, public côté host/audit
    std::string   token;                    // 43 chars b64url, EST le secret
    std::string   label;                    // libellé host (UTF-8)
    std::string   consentText;              // affiché au déposant (UTF-8)
    std::uint64_t maxBytesPerDeposit{0};    // 0 = sans limite par lien
    int           maxFilesPerDeposit{0};    // 0 = sans limite par lien
    std::int64_t  createdAt{0};             // epoch s UTC
    std::int64_t  expiresAt{0};             // 0 = sans expiration
    bool          revoked{false};
};

// Renvoie true si le lien est utilisable maintenant (ni expiré, ni révoqué).
bool depositLinkIsActive(const DepositLink& link, std::int64_t nowEpochSec);

// Identifiant court (8 premiers chars) pour logs et nommage filesystem.
std::string depositLinkShortId(const DepositLink& link);

// Sanitize un libellé pour usage filesystem (remplace les caractères
// dangereux par '_', trim, max length). Pas de dépendance externe.
std::string sanitizeForFilesystem(const std::string& input,
                                  std::size_t maxLen = 80);

} // namespace ltr::infra
