#include "ltr/network/transfer_server.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/infra/filesystem_service.hpp"
#include "ltr/infra/hash_service.hpp"
#include "ltr/infra/quota_service.hpp"
#include "ltr/infra/resume_sidecar.hpp"
#include "ltr/network/protocol.hpp"

#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

namespace ltr::network {

using namespace std::chrono;

namespace {

class QuotaReservationGuard {
public:
    QuotaReservationGuard(infra::TransferQuota* quota,
                          std::string id,
                          infra::TransferFlow flow,
                          std::uint64_t expectedBytes)
        : quota_(quota), id_(std::move(id)) {
        if (!quota_) return;
        decision_ = quota_->tryReserve(id_, flow, expectedBytes);
        active_ = decision_.allowed;
    }

    ~QuotaReservationGuard() {
        if (quota_ && active_) quota_->release(id_);
    }

    bool allowed() const noexcept { return decision_.allowed; }
    const infra::QuotaDecision& decision() const noexcept { return decision_; }

    void commit(std::uint64_t actualBytes) {
        if (!quota_ || !active_) return;
        quota_->commit(id_, actualBytes);
        active_ = false;
    }

private:
    infra::TransferQuota* quota_{nullptr};
    std::string id_;
    bool active_{false};
    infra::QuotaDecision decision_;
};

} // namespace

TransferServer::TransferServer(core::EventBus& bus,
                               domain::Device self,
                               std::filesystem::path downloadDir,
                               std::uint16_t port,
                               int resumeSidecarTtlHours,
                               std::string selfFingerprint)
    : bus_(bus),
      self_(std::move(self)),
      downloadDir_(std::move(downloadDir)),
      port_(port),
      resumeSidecarTtlHours_(resumeSidecarTtlHours),
      selfFingerprint_(std::move(selfFingerprint)) {}

TransferServer::~TransferServer() {
    stop();
}

void TransferServer::start() {
    if (running_.exchange(true)) return;
    if (listener_.listen(port_) != sf::Socket::Done) {
        core::log_error("TransferServer: listen(" + std::to_string(port_) +
                        ") failed");
        running_.store(false);
        return;
    }
    listener_.setBlocking(false);

    // V1.1.9 — Sprint Transfer Resume : purge des sidecars expirés
    // avant d'accepter de nouvelles sessions.
    const int removed = infra::purgeOldSidecars(downloadDir_,
                                                  resumeSidecarTtlHours_);
    if (removed > 0) {
        core::log_info("[resume] " + std::to_string(removed)
                       + " sidecar(s) purgé(s)");
    }

    startAcceptThread();
}

void TransferServer::stop() {
    if (!running_.exchange(false)) return;
    listener_.close();

    // Annuler toutes les sessions en attente d'acceptation.
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        for (auto& [id, ps] : pending_) {
            std::lock_guard<std::mutex> pl(ps->m);
            ps->decision = PendingSession::Decision::Cancel;
            ps->reason   = "shutdown";
            ps->cv.notify_all();
        }
    }

    if (acceptThread_.joinable()) acceptThread_.join();

    // Après le join de acceptThread_, plus personne n'écrit dans workers_.
    std::vector<std::thread> toJoin;
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        toJoin = std::move(workers_);
    }
    for (auto& t : toJoin) {
        if (t.joinable()) t.join();
    }
}

void TransferServer::startAcceptThread() {
    acceptThread_ = std::thread([this]{
        try { acceptLoop(); }
        catch (const std::exception& e) {
            core::log_error(std::string("acceptLoop std::exception: ") + e.what());
        }
        catch (...) {
            core::log_error("acceptLoop unknown exception");
        }
    });
}

void TransferServer::acceptOffer(const std::string& sessionId) {
    std::shared_ptr<PendingSession> ps;
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        auto it = pending_.find(sessionId);
        if (it == pending_.end()) return;
        ps = it->second;
    }
    std::lock_guard<std::mutex> pl(ps->m);
    ps->decision = PendingSession::Decision::Accept;
    ps->cv.notify_all();
}

void TransferServer::rejectOffer(const std::string& sessionId,
                                 const std::string& reason) {
    std::shared_ptr<PendingSession> ps;
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        auto it = pending_.find(sessionId);
        if (it == pending_.end()) return;
        ps = it->second;
    }
    std::lock_guard<std::mutex> pl(ps->m);
    ps->decision = PendingSession::Decision::Reject;
    ps->reason   = reason;
    ps->cv.notify_all();
}

void TransferServer::cancelSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMu_);
    if (auto it = cancels_.find(sessionId); it != cancels_.end()) {
        it->second->store(true);
    }
    if (auto it = pending_.find(sessionId); it != pending_.end()) {
        std::lock_guard<std::mutex> pl(it->second->m);
        it->second->decision = PendingSession::Decision::Cancel;
        it->second->cv.notify_all();
    }
}

void TransferServer::acceptLoop() {
    while (running_.load()) {
        auto sock = std::make_unique<sf::TcpSocket>();
        const auto st = listener_.accept(*sock);
        if (st == sf::Socket::NotReady) {
            std::this_thread::sleep_for(milliseconds(50));
            continue;
        }
        if (st != sf::Socket::Done) continue;

        sock->setBlocking(true);
        std::lock_guard<std::mutex> lock(sessionsMu_);
        workers_.emplace_back(
            [this, s = std::move(sock)]() mutable {
                try { sessionWorker(std::move(s)); }
                catch (const std::exception& e) {
                    core::log_error(std::string("sessionWorker std::exception: ") + e.what());
                }
                catch (...) {
                    core::log_error("sessionWorker unknown exception");
                }
            });
    }
}

void TransferServer::sessionWorker(std::unique_ptr<sf::TcpSocket> sock) {
    using nlohmann::json;

    Frame frame;
    auto st = readFrame(*sock, frame);
    if (st != sf::Socket::Done || frame.type != MessageType::Offer) {
        core::log_warn("TransferServer: premier message invalide");
        return;
    }

    domain::TransferRequest req;
    try {
        auto j = json::parse(frame.payload.begin(), frame.payload.end());
        req.sessionId  = j.value("sessionId",  "");
        req.senderId   = j.value("senderId",   "");
        req.senderName = j.value("senderName", "");
        req.pinCode    = j.value("pinCode",    "");
        for (const auto& f : j.value("files", json::array())) {
            domain::FileMeta meta;
            meta.relativePath = f.value("relativePath", "");
            meta.size         = f.value("size", std::uint64_t{0});
            req.files.push_back(std::move(meta));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("OFFER parse error: ") + e.what());
        return;
    }

    if (req.sessionId.empty() || req.files.empty()) {
        core::log_warn("TransferServer: OFFER incomplet");
        return;
    }

    QuotaReservationGuard quotaGuard(
        quota_, req.sessionId, infra::TransferFlow::TcpIn, req.totalSize());
    if (!quotaGuard.allowed()) {
        json r = {{"sessionId", req.sessionId},
                  {"reason", quotaGuard.decision().reason.empty()
                       ? "quota_exceeded" : quotaGuard.decision().reason}};
        writeJsonFrame(*sock, MessageType::Reject, r.dump());
        bus_.post(core::TransferFailedEvent{
            req.sessionId, r["reason"].get<std::string>(),
            core::ErrorCategory::Permanent});
        return;
    }

    // Vérifier l'espace disque disponible.
    if (!infra::FilesystemService::hasSpace(downloadDir_, req.totalSize())) {
        json r = {{"sessionId", req.sessionId},
                  {"reason",    "no_disk_space"}};
        writeJsonFrame(*sock, MessageType::Reject, r.dump());
        bus_.post(core::TransferFailedEvent{
            req.sessionId, "Espace disque insuffisant (côté récepteur)"});
        return;
    }

    // Publier l'offre pour que l'UI la présente à l'utilisateur.
    auto ps = std::make_shared<PendingSession>();
    ps->socket  = std::move(sock);
    ps->request = req;
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        pending_[req.sessionId] = ps;
    }
    bus_.post(core::IncomingOfferEvent{req});

    // Attendre la décision (avec timeout).
    PendingSession::Decision decision;
    {
        std::unique_lock<std::mutex> lk(ps->m);
        ps->cv.wait_for(lk, core::kOfferTimeout, [&]{
            return ps->decision != PendingSession::Decision::None;
        });
        decision = ps->decision;
    }

    // Nettoyage du pending map — on garde le socket dans `ps`.
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        pending_.erase(req.sessionId);
    }

    if (decision != PendingSession::Decision::Accept) {
        const auto reason =
            (decision == PendingSession::Decision::Reject) ? "rejected" :
            (decision == PendingSession::Decision::Cancel) ? "cancelled" :
            "timeout";
        json r = {{"sessionId", req.sessionId}, {"reason", reason}};
        writeJsonFrame(*ps->socket, MessageType::Reject, r.dump());
        bus_.post(core::OfferAnsweredEvent{req.sessionId, false, reason});
        return;
    }

    // Accepter. V1.6.4 — Wave 2 TOFU : on inclut l'empreinte stable du
    // host (peerId du receveur B) pour que l'émetteur A puisse vérifier
    // qu'il s'agit bien du même host qu'à la dernière session.
    json okPayload = {{"sessionId", req.sessionId}};
    if (!selfFingerprint_.empty()) {
        okPayload["fingerprint"] = selfFingerprint_;
    }
    if (writeJsonFrame(*ps->socket, MessageType::Accept, okPayload.dump())
        != sf::Socket::Done) return;

    bus_.post(core::OfferAnsweredEvent{req.sessionId, true, ""});
    bus_.post(core::TransferStartedEvent{req.sessionId});

    // V1.1.9 — Sprint Transfer Resume : sidecar initial (tous NotStarted).
    // Mis à jour à chaque FileEnd. Supprimé à la complétion (DONE) ou à un
    // cancel explicite. Conservé sur erreur réseau pour permettre resume.
    infra::Sidecar sidecar;
    sidecar.sessionId      = req.sessionId;
    sidecar.senderDeviceId = req.senderId;
    sidecar.senderName     = req.senderName;
    sidecar.createdAt      = std::chrono::system_clock::now();
    sidecar.lastUpdateAt   = sidecar.createdAt;
    for (const auto& f : req.files) {
        infra::SidecarFileState sf;
        sf.relativePath = f.relativePath;
        sf.expectedSize = f.size;
        sf.status       = infra::FileResumeStatus::NotStarted;
        sidecar.files.push_back(std::move(sf));
    }
    infra::writeSidecar(downloadDir_, sidecar);

    // Flag d'annulation pour cette session.
    std::atomic<bool> cancelFlag{false};
    {
        std::lock_guard<std::mutex> lock(sessionsMu_);
        cancels_[req.sessionId] = &cancelFlag;
    }

    auto cancelGuard = [&]{
        std::lock_guard<std::mutex> lock(sessionsMu_);
        cancels_.erase(req.sessionId);
    };

    // Recevoir les fichiers.
    domain::TransferSession session(req.sessionId,
                                    domain::TransferSession::Direction::Incoming,
                                    domain::Device{/*sender minimal*/},
                                    req.totalSize());
    auto lastReport = steady_clock::now();

    for (std::size_t i = 0; i < req.files.size(); ++i) {
        if (cancelFlag.load()) break;

        Frame hdr;
        if (readFrame(*ps->socket, hdr) != sf::Socket::Done) {
            bus_.post(core::TransferFailedEvent{req.sessionId, "connexion perdue"});
            cancelGuard();
            return;
        }
        if (hdr.type != MessageType::FileHeader) {
            bus_.post(core::TransferFailedEvent{req.sessionId, "protocole: attendu FILE_HEADER"});
            cancelGuard();
            return;
        }

        std::string relPath;
        std::uint64_t fileSize = 0;
        try {
            auto j = json::parse(hdr.payload.begin(), hdr.payload.end());
            relPath  = j.value("relativePath", "");
            fileSize = j.value("size", std::uint64_t{0});
        } catch (...) {
            bus_.post(core::TransferFailedEvent{req.sessionId, "FILE_HEADER invalide"});
            cancelGuard();
            return;
        }

        const auto target = infra::FilesystemService::uniqueTargetPath(
            downloadDir_, relPath);
        const auto tmp = target.string() + ".part";

        std::ofstream ofs(tmp, std::ios::binary);
        if (!ofs) {
            bus_.post(core::TransferFailedEvent{req.sessionId, "impossible d'écrire"});
            cancelGuard();
            return;
        }

        std::uint64_t remaining = fileSize;
        while (remaining > 0) {
            if (cancelFlag.load()) {
                ofs.close();
                // V1.1.9 : cancel user → supprimer .part + sidecar.
                std::error_code ec;
                std::filesystem::remove(tmp, ec);
                infra::deleteSidecar(downloadDir_, req.sessionId);
                bus_.post(core::TransferFailedEvent{req.sessionId, "annulé",
                    core::ErrorCategory::Cancelled});
                cancelGuard();
                return;
            }

            Frame chunk;
            if (readFrame(*ps->socket, chunk) != sf::Socket::Done) {
                // V1.1.9 : erreur réseau → CONSERVER .part et sidecar pour
                // resume. Update sidecar avec bytesReceived courant.
                ofs.flush();
                ofs.close();
                if (i < sidecar.files.size()) {
                    auto& sf = sidecar.files[i];
                    sf.status        = infra::FileResumeStatus::Partial;
                    sf.bytesReceived = fileSize - remaining;
                    sf.partialPath   = std::filesystem::path(tmp)
                                         .filename().string();
                }
                sidecar.lastUpdateAt = std::chrono::system_clock::now();
                infra::writeSidecar(downloadDir_, sidecar);
                bus_.post(core::TransferFailedEvent{req.sessionId,
                    "connexion perdue", core::ErrorCategory::Network});
                cancelGuard();
                return;
            }
            if (chunk.type != MessageType::FileChunk) {
                bus_.post(core::TransferFailedEvent{req.sessionId, "protocole: attendu FILE_CHUNK"});
                cancelGuard();
                return;
            }
            if (chunk.payload.size() > remaining) {
                bus_.post(core::TransferFailedEvent{req.sessionId, "chunk trop long"});
                cancelGuard();
                return;
            }
            ofs.write(reinterpret_cast<const char*>(chunk.payload.data()),
                      static_cast<std::streamsize>(chunk.payload.size()));
            remaining -= chunk.payload.size();
            session.addBytes(chunk.payload.size());

            const auto now = steady_clock::now();
            if (now - lastReport > milliseconds(200)) {
                bus_.post(core::TransferProgressEvent{
                    req.sessionId,
                    session.bytesTransferred(),
                    session.speedBytesPerSec(),
                    session.eta()});
                lastReport = now;
            }
        }
        ofs.close();

        // FILE_END optionnel : vérifier hash si fourni.
        Frame end;
        if (readFrame(*ps->socket, end) != sf::Socket::Done ||
            end.type != MessageType::FileEnd) {
            bus_.post(core::TransferFailedEvent{req.sessionId, "FILE_END manquant"});
            cancelGuard();
            return;
        }

        // Renommer .part → final
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) {
            core::log_warn("rename .part échec : " + ec.message());
        }

        // V1.1.9 : marquer le fichier Done dans le sidecar.
        if (i < sidecar.files.size()) {
            sidecar.files[i].status        = infra::FileResumeStatus::Done;
            sidecar.files[i].bytesReceived = fileSize;
        }
        sidecar.lastUpdateAt = std::chrono::system_clock::now();
        infra::writeSidecar(downloadDir_, sidecar);
    }

    // Attendre DONE
    Frame done;
    if (readFrame(*ps->socket, done) == sf::Socket::Done &&
        done.type == MessageType::Done) {
        // V1.1.9 : session complète → supprimer le sidecar.
        infra::deleteSidecar(downloadDir_, req.sessionId);
        quotaGuard.commit(session.bytesTransferred());
        bus_.post(core::TransferDoneEvent{req.sessionId});
    } else {
        bus_.post(core::TransferFailedEvent{req.sessionId, "DONE non reçu",
            core::ErrorCategory::Network});
    }
    cancelGuard();
}

} // namespace ltr::network
