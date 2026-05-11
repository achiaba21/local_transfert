#include "ltr/network/transfer_client.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/domain/transfer_session.hpp"
#include "ltr/infra/filesystem_service.hpp"
#include "ltr/infra/known_peers.hpp"   // V1.6.4 — TOFU TCP
#include "ltr/network/protocol.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

#include <SFML/Network/TcpSocket.hpp>

namespace ltr::network {

using namespace std::chrono;

namespace {

std::string generateSessionId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    const auto a = dist(rng);
    const auto b = dist(rng);
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                  static_cast<unsigned long long>(a),
                  static_cast<unsigned long long>(b));
    return std::string(buf);
}

} // namespace

TransferClient::TransferClient(core::EventBus& bus, domain::Device self,
                               infra::KnownPeers* knownPeers)
    : bus_(bus), self_(std::move(self)), knownPeers_(knownPeers) {}

TransferClient::~TransferClient() {
    // Signale l'annulation à tous les workers, puis les joint.
    std::vector<std::shared_ptr<WorkerCtx>> all;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [_, ctx] : workers_) {
            ctx->cancel.store(true);
            all.push_back(ctx);
        }
    }
    for (auto& ctx : all) {
        if (ctx->thread.joinable()) ctx->thread.join();
    }
}

std::string TransferClient::resumeSession(
    const std::string& sessionId,
    const domain::Device& peer,
    const std::vector<std::filesystem::path>& files,
    const std::string& pinCode) {
    // V1.1.9 MVP : redémarre avec le MÊME sessionId. Le serveur écrasera
    // son sidecar précédent et repart à zéro côté disque. Wave 3 :
    // négociation ResumeOffer/ResumeResponse + skipBytes pour continuer
    // aux offsets partials existants.
    auto ctx = std::make_shared<WorkerCtx>();
    // Crash V1.4.4 : si workers_[sessionId] était déjà occupé par le
    // worker précédent (cas resume après un envoi échoué), écraser
    // le shared_ptr déclenchait le dtor du WorkerCtx → dtor du
    // std::thread encore joinable → std::terminate() → SIGABRT.
    // On extrait l'ancien sous lock, puis on join HORS du lock.
    std::shared_ptr<WorkerCtx> old;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = workers_.find(sessionId);
        if (it != workers_.end()) old = it->second;
        workers_[sessionId] = ctx;
    }
    if (old && old->thread.joinable()) {
        old->thread.join();   // immédiat si runSender a déjà return
    }
    ctx->thread = std::thread(
        [this, sessionId, peer, files, pinCode, ctx]() {
            try {
                runSender(sessionId, peer, files, pinCode, ctx);
            } catch (const std::exception& e) {
                core::log_error(std::string("resumeSession: ") + e.what());
                bus_.post(core::TransferFailedEvent{sessionId,
                    std::string("exception: ") + e.what(),
                    core::ErrorCategory::Unknown});
            } catch (...) {
                core::log_error("resumeSession: exception inconnue");
                bus_.post(core::TransferFailedEvent{sessionId,
                    "exception inconnue",
                    core::ErrorCategory::Unknown});
            }
        });
    return sessionId;
}

std::string TransferClient::sendFiles(
    const domain::Device& peer,
    const std::vector<std::filesystem::path>& files,
    const std::string& pinCode) {

    auto ctx = std::make_shared<WorkerCtx>();
    const auto sessionId = generateSessionId();

    {
        std::lock_guard<std::mutex> lock(mu_);
        workers_[sessionId] = ctx;
    }

    ctx->thread = std::thread(
        [this, sessionId, peer, files, pinCode, ctx]() {
            try {
                runSender(sessionId, peer, files, pinCode, ctx);
            } catch (const std::exception& e) {
                core::log_error(std::string("runSender std::exception: ") + e.what());
                bus_.post(core::TransferFailedEvent{sessionId, std::string("exception: ") + e.what()});
            } catch (...) {
                core::log_error("runSender unknown exception");
                bus_.post(core::TransferFailedEvent{sessionId, "exception inconnue"});
            }
        });

    return sessionId;
}

void TransferClient::cancel(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mu_);
    if (auto it = workers_.find(sessionId); it != workers_.end()) {
        it->second->cancel.store(true);
    }
}

void TransferClient::runSender(
    std::string sessionId,
    domain::Device peer,
    std::vector<std::filesystem::path> inputs,
    std::string pinCode,
    std::shared_ptr<WorkerCtx> ctx) {
    using nlohmann::json;

    // PIÈGE C++ : un thread ne peut pas effacer sa propre entrée dans
    // `workers_` ; quand le shared_ptr<WorkerCtx> tombe à 0, le destructeur
    // de std::thread est appelé sur un thread joinable (ni joint, ni
    // détaché) → std::terminate(). Le nettoyage est donc différé à
    // ~TransferClient qui joint tous les workers d'un coup.
    auto cleanup = []{};

    core::log_info("[sender " + sessionId.substr(0, 8) + "] start → " +
                   peer.name + " @ " + peer.ip.toString() + ":" +
                   std::to_string(peer.tcpPort));

    // Garde-fous : une IP invalide ou un port nul font planter SFML.
    if (peer.ip == sf::IpAddress::None || peer.ip == sf::IpAddress::Any) {
        core::log_error("[sender] IP pair invalide");
        bus_.post(core::TransferFailedEvent{sessionId, "IP pair invalide"});
        cleanup();
        return;
    }
    if (peer.tcpPort == 0) {
        core::log_error("[sender] port pair inconnu (0)");
        bus_.post(core::TransferFailedEvent{sessionId, "port pair inconnu"});
        cleanup();
        return;
    }

    // Construire la liste complète des fichiers à envoyer.
    auto entries = infra::FilesystemService::enumerate(inputs);
    core::log_info("[sender] " + std::to_string(entries.size()) + " fichier(s)");
    if (entries.empty()) {
        bus_.post(core::TransferFailedEvent{sessionId, "aucun fichier à envoyer"});
        cleanup();
        return;
    }

    std::uint64_t total = 0;
    for (const auto& e : entries) total += e.meta.size;

    // Ouvrir la connexion TCP.
    core::log_info("[sender] connect TCP...");
    sf::TcpSocket sock;
    sock.setBlocking(true);
    const auto connSt = sock.connect(peer.ip, peer.tcpPort, sf::seconds(5));
    if (connSt != sf::Socket::Done) {
        core::log_error("[sender] connect a échoué (status=" +
                        std::to_string(static_cast<int>(connSt)) + ")");
        bus_.post(core::TransferFailedEvent{sessionId, "connexion refusée",
            core::ErrorCategory::Network});
        cleanup();
        return;
    }
    core::log_info("[sender] connect OK");

    // OFFER
    json offer;
    offer["sessionId"]  = sessionId;
    offer["senderId"]   = self_.id;
    offer["senderName"] = self_.name;
    offer["pinCode"]    = pinCode;
    offer["files"]      = json::array();
    for (const auto& e : entries) {
        offer["files"].push_back({
            {"relativePath", e.meta.relativePath},
            {"size",         e.meta.size},
        });
    }
    const auto offerJson = offer.dump();
    core::log_info("[sender] OFFER envoyé (" + std::to_string(offerJson.size()) + "B)");
    if (writeJsonFrame(sock, MessageType::Offer, offerJson)
        != sf::Socket::Done) {
        core::log_error("[sender] writeJsonFrame OFFER a échoué");
        bus_.post(core::TransferFailedEvent{sessionId, "envoi OFFER échoué"});
        cleanup();
        return;
    }

    bus_.post(core::TransferStartedEvent{sessionId});
    core::log_info("[sender] OFFER envoyé, attente ACCEPT/REJECT...");

    // Attendre ACCEPT/REJECT (potentiellement long : ne pas timeout court).
    Frame resp;
    const auto readSt = readFrame(sock, resp);
    core::log_info("[sender] réponse reçue : status=" +
                   std::to_string(static_cast<int>(readSt)) +
                   " type=" + std::to_string(static_cast<int>(resp.type)));
    if (readSt != sf::Socket::Done) {
        bus_.post(core::TransferFailedEvent{sessionId, "pas de réponse du pair",
            core::ErrorCategory::Network});
        cleanup();
        return;
    }
    if (resp.type == MessageType::Reject) {
        std::string reason = "refusé";
        try {
            auto j = json::parse(resp.payload.begin(), resp.payload.end());
            reason = j.value("reason", reason);
        } catch (const std::exception& e) {
            core::log_warn(std::string("REJECT parse error: ") + e.what());
        }
        bus_.post(core::OfferAnsweredEvent{sessionId, false, reason});
        bus_.post(core::TransferFailedEvent{sessionId, reason});
        cleanup();
        return;
    }
    if (resp.type != MessageType::Accept) {
        bus_.post(core::TransferFailedEvent{sessionId, "réponse inattendue"});
        cleanup();
        return;
    }

    // V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP).
    // L'Accept du receveur peut inclure son empreinte stable. On la
    // vérifie contre known_peers.json :
    //   - inconnue → set TOFU (silencieux)
    //   - identique → OK
    //   - différente → poste FingerprintChangedEvent (non-bloquant)
    if (knownPeers_ && !peer.id.empty()) {
        try {
            const auto j = json::parse(resp.payload);
            const auto fp = j.value("fingerprint", std::string{});
            if (!fp.empty()) {
                const auto previous = knownPeers_->get(peer.id);
                const auto result = knownPeers_->set(peer.id, fp);
                using SR = infra::KnownPeers::SetResult;
                if (result == SR::New) {
                    core::log_info("[tofu] nouveau pair " + peer.id.substr(0, 8)
                                   + " fp=" + fp.substr(0, 23) + "...");
                } else if (result == SR::Changed) {
                    core::log_warn("[tofu] empreinte CHANGÉE pour pair "
                                   + peer.id.substr(0, 8)
                                   + " ancienne=" + (previous ? previous->substr(0, 23) : "?")
                                   + "... nouvelle=" + fp.substr(0, 23) + "...");
                    bus_.post(core::FingerprintChangedEvent{
                        peer.id,
                        previous.value_or(""),
                        fp});
                }
            }
        } catch (const std::exception& e) {
            core::log_warn(std::string("[tofu] Accept parse error: ") + e.what());
        }
    }

    bus_.post(core::OfferAnsweredEvent{sessionId, true, ""});

    // Envoi des fichiers.
    domain::TransferSession session(sessionId,
                                    domain::TransferSession::Direction::Outgoing,
                                    peer,
                                    total);
    auto lastReport = steady_clock::now();
    std::vector<char> buf(core::kChunkSize);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (ctx->cancel.load()) {
            json c = {{"reason", "cancelled"}};
            writeJsonFrame(sock, MessageType::Cancel, c.dump());
            bus_.post(core::TransferFailedEvent{sessionId, "annulé",
                core::ErrorCategory::Cancelled});
            cleanup();
            return;
        }

        const auto& e = entries[i];
        json h = {{"index", i},
                  {"relativePath", e.meta.relativePath},
                  {"size", e.meta.size}};
        if (writeJsonFrame(sock, MessageType::FileHeader, h.dump())
            != sf::Socket::Done) {
            bus_.post(core::TransferFailedEvent{sessionId, "envoi FILE_HEADER échoué"});
            cleanup();
            return;
        }

        std::ifstream ifs(e.absolutePath, std::ios::binary);
        if (!ifs) {
            bus_.post(core::TransferFailedEvent{sessionId,
                "fichier illisible: " + e.meta.relativePath});
            cleanup();
            return;
        }

        std::uint64_t remaining = e.meta.size;
        while (remaining > 0) {
            if (ctx->cancel.load()) {
                json c = {{"reason", "cancelled"}};
                writeJsonFrame(sock, MessageType::Cancel, c.dump());
                bus_.post(core::TransferFailedEvent{sessionId, "annulé",
                core::ErrorCategory::Cancelled});
                cleanup();
                return;
            }
            const std::size_t toRead = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, core::kChunkSize));
            ifs.read(buf.data(), static_cast<std::streamsize>(toRead));
            const auto got = ifs.gcount();
            if (got <= 0) {
                bus_.post(core::TransferFailedEvent{sessionId, "lecture échouée"});
                cleanup();
                return;
            }
            if (writeFrame(sock, MessageType::FileChunk,
                           buf.data(), static_cast<std::size_t>(got))
                != sf::Socket::Done) {
                bus_.post(core::TransferFailedEvent{sessionId, "envoi chunk échoué"});
                cleanup();
                return;
            }
            remaining -= static_cast<std::uint64_t>(got);
            session.addBytes(static_cast<std::uint64_t>(got));

            const auto now = steady_clock::now();
            if (now - lastReport > milliseconds(200)) {
                bus_.post(core::TransferProgressEvent{
                    sessionId,
                    session.bytesTransferred(),
                    session.speedBytesPerSec(),
                    session.eta()});
                lastReport = now;
            }
        }

        // FILE_END (sans hash pour performance — vérif optionnelle en V2).
        json fe = {{"index", i}, {"sha256", ""}};
        if (writeJsonFrame(sock, MessageType::FileEnd, fe.dump())
            != sf::Socket::Done) {
            bus_.post(core::TransferFailedEvent{sessionId, "envoi FILE_END échoué"});
            cleanup();
            return;
        }
    }

    // DONE.
    if (writeFrame(sock, MessageType::Done, nullptr, 0) != sf::Socket::Done) {
        bus_.post(core::TransferFailedEvent{sessionId, "envoi DONE échoué"});
        cleanup();
        return;
    }

    bus_.post(core::TransferProgressEvent{
        sessionId, session.bytesTransferred(),
        session.speedBytesPerSec(), std::chrono::seconds(0)});
    bus_.post(core::TransferDoneEvent{sessionId});
    cleanup();
}

} // namespace ltr::network
