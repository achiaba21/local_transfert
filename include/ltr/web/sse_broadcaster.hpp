#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "ltr/web/sse_channel.hpp"

namespace ltr::web {

// Multiplexeur de canaux SSE : 1 canal par session authentifiée.
// Utilisation :
//   - le handler GET /api/events appelle attach() à la connexion,
//     puis consomme via SseChannel::waitAndPop dans sa boucle long-live,
//     et appelle detach() à la déconnexion.
//   - WebService::pushFiles() appelle send(sessionToken, message) pour
//     pousser un événement vers le navigateur concerné.
class SseBroadcaster {
public:
    SseBroadcaster() = default;

    SseBroadcaster(const SseBroadcaster&)            = delete;
    SseBroadcaster& operator=(const SseBroadcaster&) = delete;

    // Crée (ou récupère) le canal associé à cette session.
    std::shared_ptr<SseChannel> attach(const std::string& sessionToken);

    // Détache et ferme proprement le canal.
    void detach(const std::string& sessionToken);

    // Envoie un message SSE à une session. No-op si session inconnue ou
    // canal fermé.
    void send(const std::string& sessionToken, const std::string& message);

    // Ferme tous les canaux (utilisé au shutdown).
    void closeAll();

private:
    mutable std::mutex mu_;
    std::map<std::string, std::shared_ptr<SseChannel>> channels_;
};

} // namespace ltr::web
