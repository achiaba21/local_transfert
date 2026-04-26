#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

#include "ltr/domain/device.hpp"
#include "ltr/domain/transfer_status.hpp"

namespace ltr::domain {

// Thread-safe, car un worker réseau met à jour `bytesTransferred` pendant
// que le thread UI lit la progression.
class TransferSession {
public:
    enum class Direction { Outgoing, Incoming };

    TransferSession(std::string sessionId,
                    Direction direction,
                    Device peer,
                    std::uint64_t totalBytes);

    const std::string& sessionId() const noexcept { return sessionId_; }
    Direction direction() const noexcept { return direction_; }
    const Device& peer() const noexcept { return peer_; }
    std::uint64_t totalBytes() const noexcept { return totalBytes_; }

    TransferStatus status() const;
    void setStatus(TransferStatus s);

    std::uint64_t bytesTransferred() const;
    double speedBytesPerSec() const;
    std::chrono::seconds eta() const;

    // Appelé par le thread worker après l'envoi/réception d'un chunk.
    void addBytes(std::uint64_t n);

    // Progression en [0, 1]. Retourne 1.0 si totalBytes == 0.
    double progress() const;

private:
    std::string sessionId_;
    Direction direction_;
    Device peer_;
    std::uint64_t totalBytes_;

    mutable std::mutex mu_;
    TransferStatus status_{TransferStatus::Pending};
    std::uint64_t bytesTransferred_{0};

    // Fenêtre glissante pour le calcul du débit instantané.
    struct Sample {
        std::chrono::steady_clock::time_point t;
        std::uint64_t totalSoFar;
    };
    std::deque<Sample> window_;
};

} // namespace ltr::domain
