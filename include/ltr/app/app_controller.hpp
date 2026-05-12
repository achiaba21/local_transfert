#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ltr/app/app_state.hpp"
#include "ltr/core/event_bus.hpp"
#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/config.hpp"
#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_link_repository.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/infra/deposit_receipt.hpp"
#include "ltr/infra/deposit_session_repository.hpp"
#include "ltr/infra/deposit_session_service.hpp"
#include "ltr/infra/deposit_token_generator.hpp"
#include "ltr/infra/known_peers.hpp"
#include "ltr/infra/peers_history.hpp"      // V1.6.5 — Wave 4 item J
#include "ltr/infra/policy_enforcement.hpp" // Phase 3 — Contrôle IT
#include "ltr/infra/quota_service.hpp"
#include "ltr/infra/retention_service.hpp"  // Phase 3 — Contrôle IT
#include "ltr/infra/transfer_history.hpp"   // V1.6.5 — Wave 4 item K
#include "ltr/network/discovery_service.hpp"
#include "ltr/network/transfer_client.hpp"
#include "ltr/network/transfer_server.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::app {

class AppController {
public:
    AppController();
    ~AppController();

    AppController(const AppController&)            = delete;
    AppController& operator=(const AppController&) = delete;

    void start();
    void stop();

    // À appeler à chaque frame du thread UI : draine l'EventBus et met à jour
    // l'état applicatif.
    void tick();

    const AppState& state() const noexcept { return state_; }
    AppState& state() noexcept { return state_; }

    // Commandes UI.
    void toggleSelectPeer(const std::string& deviceId);
    void clearSelection();
    void addFiles(const std::vector<std::filesystem::path>& paths);
    void clearFiles();
    bool canSend() const;
    void requestSend();

    // V1.1
    void toggleFileCheck(const std::filesystem::path& absolutePath);
    void cancelPending(const std::string& sessionId);

    // V1.4 — Sprint Clipboard Paste : lit le presse-papier et ajoute
    // son contenu à la liste des fichiers à envoyer (texte/image/file).
    // Les fichiers temp (texte, image) sont écrits dans clipboardTempDir().
    void pasteFromClipboard();
    std::filesystem::path clipboardTempDir() const;

    void acceptIncoming();
    void rejectIncoming();
    void cancelSession(const std::string& sessionId);

    // V1.1.8-UX2 : ouvre le dossier de téléchargement dans Finder/Explorer.
    void openDownloadDir();

    // V1.1.8-UX4 : plier/déplier le SharePanel + persister dans la config.
    void toggleSharePanel();
    bool isSharePanelCollapsed() const noexcept {
        return state_.sharePanelCollapsed;
    }

    // V1.1.9 — Sprint Transfer Resume.
    // Relance un transfert échoué (status Failed && resumable). Le client
    // réutilise le même sessionId — le serveur écrase son sidecar et
    // redémarre depuis 0. Wave 3 : négociation skipBytes vraie.
    void resumeTransfer(const std::string& sessionId);
    // Relance séquentiellement tous les transferts resumable (1 par 1).
    void resumeAllTransfers();
    // Ignorer une card Failed : la retire de la liste (passive — le
    // sidecar receiver expire naturellement à 24 h).
    void ignoreTransfer(const std::string& sessionId);

    // V1.1.2 : offres web (announce). Accept ouvre un dialog natif
    // "choisir dossier", puis résout via WebService. Refuse envoie le refus.
    //
    // Le dialog natif (tinyfiledialogs) n'est pas dans ltr_core pour garder
    // cette couche indépendante de la couche UI. On injecte donc le handler
    // via setFolderPicker() — MainScreen/UIApp l'initialise au démarrage.
    // Le handler reçoit le dossier par défaut et retourne le chemin choisi
    // (vide si annulation).
    using FolderPicker = std::function<std::string(const std::string&)>;
    void setFolderPicker(FolderPicker fn) { folderPicker_ = std::move(fn); }
    void acceptWebUpload();   // legacy, plus appelé V1.1.9
    void refuseWebUpload();   // legacy

    // V1.1.9-batch : inbox web (accumule les announces au lieu de modale
    // bloquante).
    void acceptWebUpload(const std::string& uploadId);
    void refuseWebUpload(const std::string& uploadId);
    void acceptAllWebUploads();
    void refuseAllWebUploads();
    void toggleWebInboxModal();

    // Découverte manuelle.
    void rescan();
    void probePeer(const std::string& ipv4);
    bool isScanning() const noexcept;

    const infra::Config& config() const noexcept { return cfg_; }

    // Infos à afficher dans la SharePanel desktop (QR code + URL + PIN).
    struct WebShareInfo {
        std::string url;   // "http://192.168.1.42:45456" ou vide si down
        std::string pin;   // "472931"
        std::uint16_t port{0};
        // V1.6.4 — Sprint Sécurité : empreinte SHA-256 du cert HTTPS,
        // format "AB:CD:EF:..." (32 octets). Vide si HTTPS désactivé.
        std::string fingerprint;
    };
    WebShareInfo webShareInfo() const;

private:
    void onEvent(const core::Event& ev);
    std::string makePinCode() const;

    core::EventBus bus_;
    infra::Config  cfg_;
    AppState       state_;

    std::unique_ptr<network::DiscoveryService> discovery_;
    std::unique_ptr<network::TransferServer>   server_;
    std::unique_ptr<network::TransferClient>   client_;
    std::unique_ptr<web::WebService>           web_;

    // V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP).
    std::string                                selfFingerprint_;
    std::unique_ptr<infra::KnownPeers>         knownPeers_;

    // V1.6.5 — Sprint Stabilité (Wave 4 items J + K).
    std::unique_ptr<infra::PeersHistory>       peersHistory_;
    std::unique_ptr<infra::TransferHistory>    transferHistory_;
    std::unique_ptr<infra::JsonPolicyRepository> policyRepository_;
    std::unique_ptr<infra::PolicyService>        policyService_;
    std::unique_ptr<infra::JsonQuotaRepository>  quotaRepository_;
    std::unique_ptr<infra::QuotaService>         quotaService_;
    // Phase 2 — Portail Client Externe.
    std::unique_ptr<infra::SecureRandomTokenGenerator>   depositTokenGen_;
    std::unique_ptr<infra::JsonDepositLinkRepository>    depositLinkRepo_;
    std::unique_ptr<infra::DepositLinkService>           depositLinkService_;
    std::unique_ptr<infra::JsonDepositSessionRepository> depositSessionRepo_;
    std::unique_ptr<infra::JsonDepositHistoryRepository> depositHistoryRepo_;
    std::unique_ptr<infra::DepositHistoryStore>          depositHistory_;
    std::unique_ptr<infra::DepositReceiptService>        depositReceipts_;
    std::unique_ptr<infra::DepositSessionService>        depositSessionService_;
    // Phase 3 — Contrôle IT.
    std::unique_ptr<infra::PolicyEnforcementService>     policyEnforcement_;
    std::unique_ptr<infra::RetentionService>             retentionService_;
    std::thread                                          retentionThread_;
    // Cache totalBytes par sessionId pour calculer le delta sur Done/Failed.
    std::unordered_map<std::string, std::uint64_t> sessionBytes_;

public:
    // V1.6.5 — accès lecture seule pour HistoryScreen + sidebar.
    infra::PeersHistory*    peersHistory()    { return peersHistory_.get(); }
    infra::TransferHistory* transferHistory() { return transferHistory_.get(); }

    // Phase 2 — API publique pour l'écran DepositLinksScreen.
    infra::DepositResult<infra::DepositLink>
    createDepositLink(const infra::DepositLinkSpec& spec);
    bool revokeDepositLink(const std::string& id);
    std::vector<infra::DepositLink> listDepositLinks();
    std::vector<infra::DepositHistory::Entry> depositHistorySnapshot();
private:

    std::string currentPinCode_; // PIN actif pour l'offre sortante

    // V1.1 : mémorise les fichiers envoyés par session pour auto-clean.
    // sessionId → liste des paths envoyés avec cette session.
    std::unordered_map<std::string, std::vector<std::filesystem::path>>
        sessionPaths_;

    // V1.1.2 : handler injecté par l'UI pour le dialog "choisir dossier".
    FolderPicker folderPicker_;

    // Threads de timeout pour les probes manuels — signalent "introuvable"
    // après 2s si aucun HELLO n'est revenu de l'IP ciblée.
    std::mutex              probeMu_;
    std::vector<std::thread> probeThreads_;
    std::atomic<bool>       shuttingDown_{false};
};

} // namespace ltr::app
