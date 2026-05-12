#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_session.hpp"

namespace ltr::infra {

struct DepositReceiptFile {
    std::string   name;
    std::uint64_t size{0};
    std::string   sha256;
};

// Reçu de dépôt remis au déposant + conservé côté host.
// Sérialisé en JSON signé HMAC-SHA-256.
struct DepositReceipt {
    std::string                     id;             // 16 hex, public
    std::string                     sessionId;
    std::string                     linkId;
    std::string                     linkLabel;
    std::string                     depositorName;
    bool                            consentAccepted{false};
    std::vector<DepositReceiptFile> files;
    std::uint64_t                   totalBytes{0};
    std::int64_t                    createdAt{0};
    std::string                     signature;      // hex HMAC-SHA-256
};

// Service responsable de la construction et de la vérification des reçus.
// Le secret HMAC doit être stable côté host (réutilise celui de
// WebSessionStore). Si le secret change, les reçus antérieurs ne sont
// plus vérifiables.
class DepositReceiptService {
public:
    explicit DepositReceiptService(std::string hmacSecret);

    // Construit un reçu signé à partir d'une session finalisée et de son lien.
    DepositReceipt build(const DepositSession& session,
                         const DepositLink& link,
                         std::int64_t nowEpochSec) const;

    // Vérifie qu'une signature correspond bien aux champs canoniques.
    bool verify(const DepositReceipt& receipt) const;

    // Sérialise en JSON pretty.
    std::string toJson(const DepositReceipt& receipt) const;

private:
    std::string canonicalPayload(const DepositReceipt& receipt) const;
    std::string hmacSha256Hex(const std::string& key,
                              const std::string& message) const;

    std::string hmacSecret_;
};

// Génère un id reçu (16 hex). Public pour les tests.
std::string makeDepositReceiptId();

} // namespace ltr::infra
