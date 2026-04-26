#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ltr/web/download_ticket.hpp"

namespace ltr::web {

// Registre thread-safe des tickets de download. Émis lors d'un
// WebService::pushFiles(), consultés aux requêtes GET /api/download/:id.
// Les tickets expirent après kDownloadTicketTtl (15 min).
//
// V1.1.8 : tickets rejouables. `get()` ne supprime plus le ticket après
// lecture — la seule voie de sortie est `evictExpired()`. Utile si le
// visiteur annule puis reclique, ou si la connexion coupe en plein
// download.
class DownloadTicketStore {
public:
    DownloadTicketStore() = default;

    DownloadTicketStore(const DownloadTicketStore&)            = delete;
    DownloadTicketStore& operator=(const DownloadTicketStore&) = delete;

    // Crée un ticket kind=File pointant un fichier sur disque.
    std::string issue(const std::string& sessionToken,
                      const std::string& sessionId,
                      const std::filesystem::path& path,
                      const std::string& displayName,
                      std::uint64_t size);

    // V1.1.8 : crée un ticket kind=StreamingZip.
    // `entries` est un snapshot (copie) de la liste des fichiers à
    // inclure ; `zipSize` est la taille précalculée du zip STORE.
    std::string issueStreamingZip(const std::string& sessionToken,
                                  const std::string& sessionId,
                                  std::vector<ZipEntry> entries,
                                  const std::string& displayName,
                                  std::uint64_t zipSize);

    // Lookup non-destructif (copie). Retourne std::nullopt si inconnu
    // ou expiré.
    std::optional<DownloadTicket> get(const std::string& ticketId);

    // Purge les tickets expirés. Retourne la liste des tickets supprimés
    // pour que le caller puisse émettre TransferFailedEvent{expired}.
    std::vector<DownloadTicket> evictExpired();

    // Alias historique (même sémantique que get, conservé pour la lisibilité
    // au niveau route — on double-check que le sessionToken match avant de
    // vraiment servir).
    std::optional<DownloadTicket> peek(const std::string& ticketId) const;

    // V1.1.9-batch : tous les tickets actifs (non expirés) d'une session.
    // Utilisé par GET /api/download/bundle pour streamer un ZIP unique.
    std::vector<DownloadTicket> listBySession(
        const std::string& sessionToken) const;

private:
    mutable std::mutex mu_;
    std::map<std::string, DownloadTicket> tickets_;
};

} // namespace ltr::web
