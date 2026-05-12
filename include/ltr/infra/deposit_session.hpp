#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ltr::infra {

// Une session = 1 visite déposant qui aboutit (ou non) à un dépôt.
// POD sérialisable. Persistée jusqu'à finalize + ttl GC.
struct DepositSessionFile {
    std::string   name;          // nom logique (potentiellement sanitisé)
    std::uint64_t size{0};
    std::string   sha256;        // hex
    std::string   storedPath;    // chemin filesystem absolu
};

struct DepositSession {
    enum class Status : std::uint8_t {
        Open,
        Finalized,
        Cancelled,
        Failed,
    };

    std::string                     id;          // 16 hex
    std::string                     linkId;
    std::string                     depositorName;
    bool                            consentAccepted{false};
    std::int64_t                    startedAt{0};
    std::int64_t                    finishedAt{0};
    Status                          status{Status::Open};
    std::vector<DepositSessionFile> files;
    std::uint64_t                   totalBytes{0};
};

const char* depositSessionStatusToStr(DepositSession::Status status);
DepositSession::Status depositSessionStatusFromStr(const std::string& value);

// Génère un id session unique (16 hex). Utilisé par le service.
std::string makeDepositSessionId();

} // namespace ltr::infra
