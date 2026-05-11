#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ltr::infra {

// V1.6.5 — Sprint Stabilité (Wave 4 item J).
// Historique persistant des pairs vus par le host. Permet d'afficher en
// sidebar (grisé) les pairs hors-ligne récents, et de calculer des
// totaux cumulés transferts/bytes par pair.
//
// Stockage JSON : `cfgDir/peers_history.json`
//   {
//     "peers": {
//       "<deviceId>": {
//         "name": "Mac de Serge",
//         "platform": "macOS",
//         "kind": "native"|"web",
//         "fingerprint": "AB:CD:..." | "",
//         "firstSeen": <epochSec>,
//         "lastSeen":  <epochSec>,
//         "totalTransfers": <int>,
//         "totalBytes":     <uint64>
//       }, ...
//     }
//   }
//
// Rétention : pairs avec lastSeen > 30 jours sont auto-purgés au load
// (décision BA Q5).
class PeersHistory {
public:
    struct Entry {
        std::string deviceId;
        std::string name;
        std::string platform;
        std::string kind;          // "native" ou "web"
        std::string fingerprint;
        std::int64_t firstSeen{0};      // epoch seconds
        std::int64_t lastSeen{0};
        std::int64_t totalTransfers{0};
        std::uint64_t totalBytes{0};
    };

    explicit PeersHistory(std::filesystem::path path);

    void load();    // charge + purge auto les entrées > 30j
    void save() const;

    // Met à jour lastSeen et name/platform pour le pair vu maintenant.
    // Crée l'entry si nouvelle.
    void touch(const std::string& deviceId,
               const std::string& name,
               const std::string& platform,
               const std::string& kind,
               const std::string& fingerprint);

    // Incrémente totalTransfers + totalBytes sur un pair existant.
    // No-op si deviceId inconnu.
    void recordTransfer(const std::string& deviceId,
                        std::uint64_t bytes);

    // Lookup direct.
    std::optional<Entry> get(const std::string& deviceId) const;

    // Snapshot complet (offline + tous). Utilisé pour l'UI.
    std::vector<Entry> snapshot() const;

    // Pour sidebar : pairs avec lastSeen > X secondes (= offline depuis
    // X sec) ET pas dans `excludeIds` (pairs actuellement online).
    // Triés par lastSeen DESC (plus récent en premier).
    std::vector<Entry> snapshotOffline(
        std::int64_t minOfflineSec,
        const std::vector<std::string>& excludeIds) const;

    // Retire un pair de l'historique (clic-droit "Oublier ce pair").
    void forget(const std::string& deviceId);

    std::size_t size() const;

private:
    void saveLocked() const;
    void purgeOlderThan(std::int64_t cutoffEpoch);  // appelé au load

    std::filesystem::path path_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, Entry> entries_;  // deviceId → Entry
};

}  // namespace ltr::infra
