#pragma once

#include <filesystem>
#include <string>

namespace ltr::infra {

struct Config {
    std::string deviceId;        // UUID v4, généré au premier lancement
    std::string deviceName;      // "Mac de Serge"
    std::string platform;        // "macOS" / "Windows" / ...
    std::filesystem::path downloadDir;

    // V1.1.8-UX4 : état du SharePanel à la dernière fermeture.
    // Champ JSON optionnel — default false (déplié) si absent.
    bool sharePanelCollapsed{false};

    // V1.1.9 — Sprint Transfer Resume : nombre de tentatives
    // silencieuses sur erreur ErrorCategory::Network avant de remonter
    // Failed à l'UI (backoff 1s, 4s, 16s, cap à 5 max). Default 2.
    int autoRetryCount{2};

    // V1.1.9 : TTL des sidecars .ltr-resume-<sid>.json côté receveur.
    // Purge au démarrage de TransferServer::start.
    int resumeSidecarTtlHours{24};

    // V1.1.9-batch : durée d'attente d'une décision d'announce web.
    // Le visiteur voit "timeout" si l'host ne traite pas dans ce délai.
    // Default 5 min — relevé depuis 120 s pour laisser le temps à
    // l'utilisateur d'utiliser l'inbox sans pression.
    int webAnnounceTimeoutSec{300};

    static Config loadOrCreate();
    void save() const;

    // Emplacement du fichier de configuration.
    static std::filesystem::path configPath();
};

// Helpers.
std::string generateUuidV4();
std::string detectPlatform();
std::string defaultDeviceName();

} // namespace ltr::infra
