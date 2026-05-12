#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/domain/transfer_request.hpp"
#include "ltr/domain/transfer_session.hpp"

namespace ltr::infra { class TransferQuota; }

namespace ltr::network {

// Serveur TCP : accepte les connexions entrantes et gère une session par
// connexion dans un thread worker dédié.
class TransferServer {
public:
    TransferServer(core::EventBus& bus,
                   domain::Device self,
                   std::filesystem::path downloadDir,
                   std::uint16_t port,
                   int resumeSidecarTtlHours = 24,
                   std::string selfFingerprint = "");
    ~TransferServer();

    TransferServer(const TransferServer&)            = delete;
    TransferServer& operator=(const TransferServer&) = delete;

    void start();
    void stop();

    // Réponses du destinataire sur une offre reçue.
    void acceptOffer(const std::string& sessionId);
    void rejectOffer(const std::string& sessionId, const std::string& reason);
    void cancelSession(const std::string& sessionId);
    void setQuota(infra::TransferQuota* quota) noexcept { quota_ = quota; }

private:
    struct PendingSession {
        std::unique_ptr<sf::TcpSocket> socket;
        domain::TransferRequest request;
        std::mutex m;
        std::condition_variable cv;
        enum class Decision { None, Accept, Reject, Cancel } decision{Decision::None};
        std::string reason;
    };

    void startAcceptThread();
    void acceptLoop();
    void sessionWorker(std::unique_ptr<sf::TcpSocket> sock);

    core::EventBus& bus_;
    domain::Device self_;
    std::filesystem::path downloadDir_;
    std::uint16_t port_;
    int resumeSidecarTtlHours_{24};  // V1.1.9
    std::string selfFingerprint_;    // V1.6.4 — empreinte stable inclue dans Accept
    infra::TransferQuota* quota_{nullptr};

    std::atomic<bool> running_{false};
    sf::TcpListener listener_;
    std::thread acceptThread_;

    std::mutex sessionsMu_;
    std::unordered_map<std::string, std::shared_ptr<PendingSession>> pending_;
    std::unordered_map<std::string, std::atomic<bool>*> cancels_;

    std::vector<std::thread> workers_;
};

} // namespace ltr::network
