#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ltr/core/event_bus.hpp"
#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/infra/deposit_receipt.hpp"
#include "ltr/infra/deposit_result.hpp"
#include "ltr/infra/deposit_session.hpp"
#include "ltr/infra/deposit_session_repository.hpp"
#include "ltr/infra/quota_service.hpp"

namespace ltr::infra {

// Service principal du portail de dépôt. Orchestrateur SRP :
// - vérifie le lien actif,
// - applique consent + nom,
// - applique limites du lien + quota Business global,
// - écrit les fichiers dans un sous-dossier dédié au dépôt,
// - finalise + génère le reçu signé,
// - poste DepositReceivedEvent.
//
// Toutes les dépendances sont des interfaces ou des services injectés.
// AUCUNE dépendance http/json directe.
class DepositSessionService {
public:
    using Clock = std::function<std::int64_t()>;

    DepositSessionService(DepositSessionRepository& sessionRepo,
                          DepositLinkService& linkService,
                          TransferQuota& quota,
                          DepositReceiptService& receipts,
                          DepositHistoryStore& history,
                          core::EventBus& bus,
                          std::filesystem::path depositsRoot,
                          Clock clock = {});

    // Ouvre une session pour le déposant identifié par (linkToken, name).
    // Refuse en amont si conditions non remplies.
    DepositResult<DepositSession>
    begin(const std::string& linkToken,
          const std::string& depositorName,
          bool consentAccepted);

    // Ajoute un fichier. Le stream est consommé jusqu'à EOF.
    // Réservation + commit quota gérés ici. Le sha256 est calculé en
    // streaming pour rester O(1) en mémoire.
    DepositResult<DepositSessionFile>
    addFile(const std::string& sessionId,
            const std::string& fileName,
            std::uint64_t expectedSize,
            std::istream& stream);

    // Finalise la session, persiste l'historique, génère le reçu signé,
    // poste DepositReceivedEvent.
    DepositResult<DepositReceipt>
    finalize(const std::string& sessionId);

    // Annule la session : release quota, retire les fichiers déjà écrits,
    // marque status=Cancelled.
    void cancel(const std::string& sessionId);

    // Récupère une session par id (lecture seule).
    std::optional<DepositSession> get(const std::string& sessionId);

    // Garbage-collect des sessions Open dont startedAt < (now - ttl).
    void gcAbandoned(std::int64_t ttlSeconds);

private:
    std::filesystem::path makeDepositDir(const DepositLink& link,
                                         const DepositSession& session) const;
    static std::string isoDate(std::int64_t epochSec);

    DepositSessionRepository& sessionRepo_;
    DepositLinkService&       linkService_;
    TransferQuota&            quota_;
    DepositReceiptService&    receipts_;
    DepositHistoryStore&      history_;
    core::EventBus&           bus_;
    std::filesystem::path     depositsRoot_;
    Clock                     clock_;

    std::mutex mu_;
    // sessionId → reservation IDs en cours (file granularity).
    std::unordered_map<std::string, std::vector<std::string>>
        activeReservations_;
};

} // namespace ltr::infra
