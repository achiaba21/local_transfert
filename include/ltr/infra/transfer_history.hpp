#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ltr::infra {

// V1.6.5 — Sprint Stabilité (Wave 4 item K).
// Historique persistant des transferts effectués côté host.
//
// Stockage JSON : `cfgDir/transfer_history.json`
//   {
//     "transfers": [
//       {
//         "sessionId": "...",
//         "peerDeviceId": "...",
//         "peerName": "...",
//         "kind": "tcp-out"|"tcp-in"|"http-up"|"http-down",
//         "fileCount": <int>,
//         "totalBytes": <uint64>,
//         "status": "ok"|"failed"|"cancelled",
//         "startedAt": <epochSec>,
//         "finishedAt": <epochSec>,
//         "error": "..."  (optionnel)
//       },
//       ...
//     ]
//   }
//
// Cap MAX_ENTRIES = 1000, drop des plus anciennes en cas de dépassement.
// Auto-purge entries finished > 6 mois au load.
//
// NOTE : les transferts P2P (browser ↔ browser) NE SONT PAS loggés ici
// — le host ne voit que les signaux, pas les données. Seul le browser
// (transfer_registry.js localStorage) garde leur trace.
class TransferHistory {
public:
    enum class Kind  : std::uint8_t { TcpOut, TcpIn, HttpUp, HttpDown };
    enum class Status : std::uint8_t { Pending, Ok, Failed, Cancelled };

    struct Entry {
        std::string  sessionId;
        std::string  peerDeviceId;
        std::string  peerName;
        Kind         kind{Kind::TcpOut};
        int          fileCount{0};
        std::uint64_t totalBytes{0};
        Status       status{Status::Pending};
        std::int64_t startedAt{0};
        std::int64_t finishedAt{0};
        std::string  error;
    };

    explicit TransferHistory(std::filesystem::path path);

    void load();
    void save() const;

    // Insère une nouvelle entry (status Pending). Si sessionId existe
    // déjà, met à jour les métadonnées.
    void insert(const Entry& entry);

    // Met à jour l'entry pointée par sessionId :
    //   - patch progress / fileCount / totalBytes
    //   - patch status (Ok/Failed/Cancelled) + finishedAt + error
    // No-op si sessionId inconnu.
    void updateProgress(const std::string& sessionId,
                        std::uint64_t totalBytes);
    void markDone(const std::string& sessionId,
                  Status status,
                  std::uint64_t totalBytes,
                  const std::string& error = "");

    std::optional<Entry> get(const std::string& sessionId) const;

    // Snapshot trié par finishedAt (ou startedAt si pending) DESC.
    std::vector<Entry> snapshot() const;

    std::size_t size() const;

    // Helper conversion enum → string pour JSON / UI.
    static const char* kindToStr(Kind k);
    static Kind        kindFromStr(const std::string& s);
    static const char* statusToStr(Status s);
    static Status      statusFromStr(const std::string& s);

private:
    void saveLocked() const;
    void enforceCap();
    void purgeOlderThan(std::int64_t cutoffEpoch);

    std::filesystem::path path_;
    mutable std::mutex mu_;
    std::vector<Entry> entries_;  // append-only, oldest first
};

}  // namespace ltr::infra
