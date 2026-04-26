#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/domain/file_meta.hpp"

namespace ltr::network {

class TransferClient {
public:
    explicit TransferClient(core::EventBus& bus, domain::Device self);
    ~TransferClient();

    TransferClient(const TransferClient&)            = delete;
    TransferClient& operator=(const TransferClient&) = delete;

    // Lance un transfert vers un pair. Retourne l'identifiant de session.
    std::string sendFiles(const domain::Device& peer,
                          const std::vector<std::filesystem::path>& files,
                          const std::string& pinCode);

    // V1.1.9 — Sprint Transfer Resume : relance un transfert échoué avec
    // un sessionId existant. Pour le MVP Wave 2, redémarre from scratch
    // (même sid réutilisé → le serveur détecte et traite comme nouvel envoi,
    // son sidecar précédent étant remplacé). Wave 3 : négociation skipBytes
    // vraie via ResumeOffer/ResumeResponse.
    std::string resumeSession(const std::string& sessionId,
                              const domain::Device& peer,
                              const std::vector<std::filesystem::path>& files,
                              const std::string& pinCode);

    void cancel(const std::string& sessionId);

private:
    struct WorkerCtx {
        std::thread thread;
        std::atomic<bool> cancel{false};
    };

    void runSender(std::string sessionId,
                   domain::Device peer,
                   std::vector<std::filesystem::path> inputs,
                   std::string pinCode,
                   std::shared_ptr<WorkerCtx> ctx);

    core::EventBus& bus_;
    domain::Device self_;

    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<WorkerCtx>> workers_;
};

} // namespace ltr::network
