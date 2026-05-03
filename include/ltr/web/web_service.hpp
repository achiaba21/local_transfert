#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/web/http_server.hpp"
#include "ltr/web/web_session_store.hpp"
#include "ltr/web/download_ticket_store.hpp"
#include "ltr/web/sse_broadcaster.hpp"
#include "ltr/web/web_upload_announce.hpp"

namespace ltr::web {

// Façade publique de la couche web. Possède :
//  - HttpServer (+ routes)
//  - WebSessionStore (sessions actives + PIN)
//  - DownloadTicketStore (tickets host→browser)
//  - SseBroadcaster (1 canal par session)
//  - thread "keepalive" qui ré-émet PeerSeenEvent toutes les 2s
//    et émet PeerLostEvent pour les sessions expirées
//
// Toutes les communications vers l'UI passent par core::EventBus.
class WebService {
public:
    WebService(core::EventBus& bus,
               domain::Device self,
               std::filesystem::path downloadDir,
               int webAnnounceTimeoutSec = 300);
    ~WebService();

    WebService(const WebService&)            = delete;
    WebService& operator=(const WebService&) = delete;

    void start();
    void stop();

    // V1.6.4 — Sprint Sécurité : configure le dossier de stockage du
    // cert auto-signé. À appeler AVANT start() pour activer HTTPS.
    void setCertConfigDir(std::filesystem::path cfgDir) {
        certCfgDir_ = std::move(cfgDir);
    }

    // Envoi host → browser. Crée N tickets et émet un SSE 'files-offer'
    // vers la session destinataire. Ne bloque pas (les downloads sont tirés
    // par le navigateur).
    void pushFiles(const std::string& sessionToken,
                   const std::string& sessionId,
                   const std::vector<std::filesystem::path>& files);

    // Getters pour la SharePanel UI.
    std::string localUrl() const;
    std::string accessPin() const { return accessPin_; }
    std::uint16_t port() const { return port_.load(); }

    // V1.1.9-batch : timeout d'attente d'une décision announce.
    int announceTimeoutSec() const noexcept { return announceTimeoutSec_; }

    // V1.1.8-UX2 : annule un download web en cours. Positionne un flag
    // atomique partagé entre WebService et le provider cpp-httplib ; le
    // prochain chunk du stream return false → connexion coupée,
    // TransferFailedEvent{cancelled} émis par la route.
    // No-op si la session n'a pas de download actif.
    void cancelSession(const std::string& sessionId);

    // V1.1.8-UX2 : appelé par les routes download au démarrage d'un GET.
    // Retourne un shared_ptr<atomic<bool>> qui :
    //   - sera mis à true si `cancelSession(sid)` est appelé
    //   - doit être vérifié par le provider à chaque chunk
    // Plusieurs GET concurrents sur le même sid partagent le même flag.
    std::shared_ptr<std::atomic<bool>>
    acquireCancelFlag(const std::string& sessionId);

    // V1.2 — Sprint Web P2P : pousse l'évènement SSE `web-peers` vers
    // une session spécifique (la liste des AUTRES sessions actives, hors
    // celle pointée par `token`). Utilisé après auth/logout/eviction.
    void emitWebPeersTo(const std::string& token);

    // V1.2 — Sprint Web P2P : pousse l'évènement SSE `web-peers` vers
    // toutes les sessions actives. Chaque session reçoit la liste des
    // autres (jamais elle-même).
    void emitWebPeersToAll();

    // Exposés pour les routes (accès contrôlé depuis route_registrar).
    WebSessionStore&         sessions()    { return sessions_; }
    DownloadTicketStore&     tickets()     { return tickets_; }
    SseBroadcaster&          broadcaster() { return broadcaster_; }
    WebUploadAnnounceStore&  announces()   { return announces_; } // V1.1.2
    core::EventBus&       bus()         { return bus_; }
    HttpServer&           httpServer()  { return server_; }
    const domain::Device& self()  const { return self_; }
    const std::filesystem::path& downloadDir() const { return downloadDir_; }
    const std::string&    accessPinRef() const { return accessPin_; }

    // V1.6.4 — Sprint Sécurité : HTTPS coexistant + fingerprint.
    HttpServer*           httpsServer() { return httpsServer_.get(); }
    const std::string&    fingerprint() const { return fingerprint_; }
    std::uint16_t         portHttps() const { return portHttps_.load(); }


private:
    void keepaliveLoop();
    // V1.1.7 : travail lourd (zip dossier, émission SSE files-offer) exécuté
    // dans un thread détaché depuis pushFiles() pour ne pas freezer l'UI.
    void zipAndAnnounce(const std::string& sessionToken,
                        const std::string& sessionId,
                        const std::vector<std::filesystem::path>& files);
    static std::string make6DigitPin();

    core::EventBus&       bus_;
    domain::Device        self_;
    std::filesystem::path downloadDir_;

    HttpServer              server_;
    // V1.6.4 — Sprint Sécurité : HTTPS server optionnel sur 45457
    // (fallback 45458/45459 si occupé). Cert auto-signé persisté dans
    // cfgDir.
    std::unique_ptr<HttpServer> httpsServer_;
    std::string             fingerprint_;
    std::atomic<std::uint16_t> portHttps_{0};
    std::filesystem::path   certCfgDir_;
    WebSessionStore         sessions_;
    DownloadTicketStore     tickets_;
    SseBroadcaster          broadcaster_;
    WebUploadAnnounceStore  announces_;

    std::string           accessPin_;
    std::atomic<std::uint16_t> port_{0};
    std::atomic<bool>     running_{false};
    std::thread           keepaliveThread_;
    int                   announceTimeoutSec_{300};  // V1.1.9-batch

    // V1.1.8-UX2 : map partagée sid → flag de cancel. Ajoutée au 1er
    // GET sur un ticket, retirée quand le download se termine (normal ou
    // cancel).
    std::mutex cancelMu_;
    std::unordered_map<std::string,
        std::shared_ptr<std::atomic<bool>>> cancelFlags_;
};

} // namespace ltr::web
