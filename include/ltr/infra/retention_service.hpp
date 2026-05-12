#pragma once

#include <cstdint>
#include <filesystem>

#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/peers_history.hpp"
#include "ltr/infra/policy_enforcement.hpp"
#include "ltr/infra/transfer_history.hpp"

namespace ltr::infra {

// Phase 3 — purge périodique de l'historique et des fichiers reçus
// selon `retention.historyDays` / `retention.receivedFilesDays`.
//
// Stateless : utilise les services existants (PeersHistory propose
// déjà forget(), on snapshot puis on appelle forget pour chaque
// entrée trop ancienne).
//
// Lifecycle : 1 appel au boot puis 1 appel toutes les 24 h depuis
// un thread de l'AppController.
class RetentionService {
public:
    RetentionService(PolicyEnforcementService& policy,
                     std::filesystem::path downloadDir);

    // Purge les 3 historiques persistants si historyDays > 0.
    // Retourne le nombre total d'entrées supprimées.
    int purgeHistories(PeersHistory& peers,
                       std::int64_t nowEpochSec);

    // Purge les fichiers du dossier de téléchargement si
    // receivedFilesDays > 0. Skip 'Deposits/.receipts/' (reçus
    // signés à conserver pour audit).
    // Retourne le nombre de fichiers supprimés.
    int purgeReceivedFiles(std::int64_t nowEpochSec);

private:
    PolicyEnforcementService& policy_;
    std::filesystem::path     downloadDir_;
};

} // namespace ltr::infra
