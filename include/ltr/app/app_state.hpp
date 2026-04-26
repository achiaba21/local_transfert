#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "ltr/domain/device.hpp"
#include "ltr/domain/transfer_request.hpp"
#include "ltr/domain/transfer_status.hpp"

namespace ltr::app {

// V1.1.8-UX1 : direction en enum (au lieu de string "→"/"←") pour permettre
// un rendu par icône côté UI et éviter tout string-matching fragile.
enum class TransferDirection { Outgoing, Incoming };

struct UiTransfer {
    std::string sessionId;
    std::string peerName;
    TransferDirection direction{TransferDirection::Outgoing};
    std::uint64_t totalBytes{0};
    std::uint64_t bytesTransferred{0};
    double speedBps{0.0};
    std::chrono::seconds eta{0};
    domain::TransferStatus status{domain::TransferStatus::Pending};
    std::string error;

    // V1.1.8-UX2 : posé par AppController dès que le status bascule sur un
    // état terminal (Done/Failed/Cancelled). Consommé par `tick()` pour
    // auto-nettoyer la card après un TTL (10 s Done, 30 s Failed/Cancelled).
    // Expired n'est PAS auto-cleané (l'utilisateur doit voir qu'un visiteur
    // web n'a rien téléchargé).
    std::chrono::steady_clock::time_point terminalAt{};

    // V1.1.9 — Sprint Transfer Resume : métadonnées nécessaires pour
    // reprendre un transfert échoué.
    std::vector<std::filesystem::path> sourcePaths;
    std::string peerId;
    std::string pinCode;
    bool        resumable{false};      // vrai si status Failed ET category resumable
    std::string lastErrorCategory;     // "network" / "protocol" / "permanent" / "cancelled"
};

// V1.1 : chaque fichier à envoyer a un état "checked" indépendant. L'utilisateur
// peut décocher certains fichiers avant de cliquer ENVOYER — seuls les cochés
// partent. Après envoi réussi, les fichiers envoyés sont retirés de la liste.
//
// V1.1.5 : ajout notion Kind. Un item représente SOIT un fichier, SOIT un
// dossier entier (agrégé, pas développé en lignes individuelles). Au moment
// de l'envoi, FilesystemService::enumerate() expand chaque dossier.
struct SelectedFile {
    enum class Kind { File, Folder };

    Kind                  kind{Kind::File};
    std::filesystem::path absolutePath;  // fichier OU racine dossier
    std::string           displayName;   // "fichier.txt" ou "monDossier/"
    std::uint64_t         size{0};       // taille fichier ou total agrégé
    int                   fileCount{1};  // 1 pour File, N pour Folder
    bool                  checked{true};
};

struct AppState {
    domain::Device self;

    std::vector<domain::Device> peers;
    std::unordered_set<std::string> selectedPeerIds;

    // Chemins tels que choisis par l'utilisateur (fichiers OU dossiers).
    // Sert de source de vérité pour l'envoi — la structure des dossiers est
    // ainsi préservée.
    std::vector<std::filesystem::path> inputPaths;

    // V1.1 : liste des fichiers avec case à cocher.
    std::vector<SelectedFile> selectedFiles;
    std::uint64_t selectedFilesCheckedTotal{0};
    std::uint64_t selectedFilesCheckedCount{0};

    std::optional<domain::TransferRequest> incomingOffer;
    std::string incomingPinDisplay; // "4 2 7 9"

    // V1.1.2 (déprécié V1.1.9-batch) : pendingWebOffer = 1 seule offre
    // web à la fois → modale bloquante. Remplacé par `webInbox` ci-dessous
    // qui accumule N announces et expose une modale liste à la demande.
    struct PendingWebOffer {
        std::string   uploadId;
        std::string   senderName;
        std::uint64_t totalBytes{0};
        int           filesCount{0};
        std::string   firstFileName;
    };
    std::optional<PendingWebOffer> pendingWebOffer; // legacy, ignoré

    // V1.1.9-batch : file d'attente des demandes d'upload web.
    // Affichée via un badge compact dans le header + modale liste.
    struct WebInboxEntry {
        std::string   uploadId;
        std::string   senderName;
        std::uint64_t totalBytes{0};
        int           filesCount{0};
        std::string   firstFileName;
    };
    std::vector<WebInboxEntry> webInbox;
    // Secondes écoulées depuis le passage à 0 demande. Pour fade du badge.
    float webInboxFadeSec{0.f};
    // Modale inbox ouverte ? Ouvre/ferme via clic badge ou bouton fermer.
    bool webInboxModalOpen{false};

    std::deque<UiTransfer> transfers;

    // V1.1.8-UX4 : état plié du SharePanel (droite). Synchronisé avec
    // `infra::Config::sharePanelCollapsed` au démarrage et au toggle.
    bool sharePanelCollapsed{false};

    // Journal d'événements pour debug (dernières lignes).
    std::deque<std::string> logTail;
};

} // namespace ltr::app
