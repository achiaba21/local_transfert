#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ltr::infra {

// Sprint Transfer Resume : I/O helpers pour les sidecars de resume.
//
// Côté RECEIVER : un sidecar JSON par session globale (pas par fichier)
// dans downloadDir/ltr-resume-<sessionId>.json, qui contient l'état de
// TOUS les fichiers de la session (done + partial + not_started).
//
// Côté SENDER : un unique pending-sessions.json dans le config dir qui
// liste les sessions Failed avec assez d'info pour les rejouer.

// === Sidecar receiver ===

enum class FileResumeStatus { NotStarted, Partial, Done };

struct SidecarFileState {
    std::string         relativePath;
    FileResumeStatus    status{FileResumeStatus::NotStarted};
    std::uint64_t       expectedSize{0};
    std::uint64_t       bytesReceived{0};
    std::string         sha256Prefix;    // 4 Ko début de source
    std::string         partialPath;     // relatif à downloadDir, si Partial
};

struct Sidecar {
    std::string sessionId;
    std::string senderDeviceId;
    std::string senderName;
    std::chrono::system_clock::time_point createdAt{};
    std::chrono::system_clock::time_point lastUpdateAt{};
    std::vector<SidecarFileState> files;
};

// Chemin standard du sidecar pour une session donnée.
std::filesystem::path sidecarPath(
    const std::filesystem::path& downloadDir,
    const std::string& sessionId);

// Écrit le sidecar atomiquement (.tmp + rename) — concurrent-safe côté
// lecteurs.
bool writeSidecar(const std::filesystem::path& downloadDir,
                  const Sidecar& s);

// Charge un sidecar. Return std::nullopt si absent, JSON corrompu, ou
// sessionId inconsistant. En cas de corruption, le fichier est supprimé.
std::optional<Sidecar> readSidecar(
    const std::filesystem::path& downloadDir,
    const std::string& sessionId);

// Supprime le sidecar (succès transfert, cancel explicite, ou corruption).
void deleteSidecar(const std::filesystem::path& downloadDir,
                   const std::string& sessionId);

// Purge tous les sidecars ltr-resume-*.json plus vieux que `ttlHours`
// dans `downloadDir`. Retourne le nombre de fichiers supprimés.
int purgeOldSidecars(const std::filesystem::path& downloadDir,
                     int ttlHours);

// === Pending sessions côté sender ===

struct PendingSession {
    std::string   sessionId;
    std::string   peerId;
    std::string   peerName;
    std::string   peerIp;         // "192.168.1.3"
    std::uint16_t peerTcpPort{0};
    std::string   pinCode;
    std::vector<std::filesystem::path> sourcePaths;
    std::uint64_t totalBytes{0};
    std::uint64_t bytesTransferred{0};
    int           retryAttempts{0};
    std::string   lastErrorCategory;  // "network"/"protocol"/"permanent"
    std::chrono::system_clock::time_point createdAt{};
};

// Charge la liste depuis <configDir>/pending-sessions.json. Retourne
// liste vide si absent ou corrompu (avec delete du corrompu).
std::vector<PendingSession> loadPendingSessions(
    const std::filesystem::path& configDir);

// Écrit la liste complète (atomique .tmp + rename).
bool savePendingSessions(const std::filesystem::path& configDir,
                         const std::vector<PendingSession>& sessions);

} // namespace ltr::infra
