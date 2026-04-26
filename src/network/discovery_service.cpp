#include "ltr/network/discovery_service.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/network/broadcast_socket.hpp"

#include <nlohmann/json.hpp>

#include <vector>

namespace ltr::network {

using namespace std::chrono;

DiscoveryService::DiscoveryService(core::EventBus& bus, domain::Device self)
    : bus_(bus), self_(std::move(self)) {}

DiscoveryService::~DiscoveryService() {
    stop();
}

namespace {

template <typename Fn>
void runThreadSafely(const char* name, Fn&& fn) {
    try { fn(); }
    catch (const std::exception& e) {
        core::log_error(std::string("[thread ") + name + "] std::exception: " + e.what());
    }
    catch (...) {
        core::log_error(std::string("[thread ") + name + "] unknown exception");
    }
}

// Cadence interne du beaconLoop : on quitte le sleep long d'origine pour
// pouvoir drainer la file d'émission ponctuelle avec une latence faible.
constexpr auto kBeaconTick = milliseconds(100);

} // namespace

void DiscoveryService::start() {
    if (running_.exchange(true)) return;
    beaconThread_   = std::thread([this]{ runThreadSafely("beacon",   [this]{ beaconLoop(); });   });
    listenerThread_ = std::thread([this]{ runThreadSafely("listener", [this]{ listenLoop(); });   });
    ttlThread_      = std::thread([this]{ runThreadSafely("ttl",      [this]{ ttlLoop(); });      });
}

void DiscoveryService::stop() {
    if (!running_.exchange(false)) return;
    if (beaconThread_.joinable())   beaconThread_.join();
    if (listenerThread_.joinable()) listenerThread_.join();
    if (ttlThread_.joinable())      ttlThread_.join();
}

void DiscoveryService::rescan() {
    const auto now = steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(outMu_);
        for (int i = 0; i < 5; ++i) {
            outQueue_.push_back(OutgoingBeacon{
                OutgoingBeacon::Kind::Solicit,
                sf::IpAddress::Broadcast,
                now + milliseconds(i * 400)
            });
        }
    }
    scanBurstsRemaining_.store(5);
}

void DiscoveryService::probe(sf::IpAddress ip) {
    std::lock_guard<std::mutex> lock(outMu_);
    outQueue_.push_back(OutgoingBeacon{
        OutgoingBeacon::Kind::Solicit,
        ip,
        steady_clock::now()
    });
}

bool DiscoveryService::hasPeer(sf::IpAddress ip) const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& [_, d] : knownPeers_) {
        if (d.ip == ip) return true;
    }
    return false;
}

void DiscoveryService::beaconLoop() {
    BroadcastUdpSocket sock;
    sock.setBlocking(true);
    if (sock.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        core::log_error("Discovery: bind beacon failed");
        return;
    }
    if (!sock.enableBroadcast()) {
        core::log_warn("Discovery: SO_BROADCAST non activable (diffusion limitée)");
    }

    // IP locale mise en cache une seule fois au démarrage du thread.
    const sf::IpAddress localIp = sf::IpAddress::getLocalAddress();
    selfIpCached_ = (localIp != sf::IpAddress::None) ? localIp.toString() : std::string{};
    if (selfIpCached_.empty()) {
        core::log_warn("Discovery: IP locale inconnue — le pair utilisera sender");
    }

    auto buildHelloPayload = [&]() {
        nlohmann::json j = {
            {"proto",    "LTR1"},
            {"kind",     "HELLO"},
            {"id",       self_.id},
            {"name",     self_.name},
            {"platform", self_.platform},
            {"tcpPort",  self_.tcpPort},
        };
        if (!selfIpCached_.empty()) j["ip"] = selfIpCached_;
        return j.dump();
    };

    auto buildSolicitPayload = [&]() {
        const nlohmann::json j = {
            {"proto", "LTR1"},
            {"kind",  "SOLICIT"},
            {"id",    self_.id},
        };
        return j.dump();
    };

    auto nextHello = steady_clock::now(); // envoyer un HELLO dès le premier tick

    while (running_.load()) {
        const auto now = steady_clock::now();

        // 1) HELLO périodique broadcast
        if (now >= nextHello) {
            const std::string payload = buildHelloPayload();
            sock.send(payload.data(), payload.size(),
                      sf::IpAddress::Broadcast, core::kDiscoveryPort);
            nextHello = now + core::kBeaconInterval;
        }

        // 2) Drainer la file d'émission ponctuelle (SOLICIT + HELLO unicast).
        std::vector<OutgoingBeacon> toSend;
        {
            std::lock_guard<std::mutex> lock(outMu_);
            for (auto it = outQueue_.begin(); it != outQueue_.end(); ) {
                if (it->sendAt <= now) {
                    toSend.push_back(std::move(*it));
                    it = outQueue_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto& item : toSend) {
            const std::string payload = (item.kind == OutgoingBeacon::Kind::Solicit)
                ? buildSolicitPayload()
                : buildHelloPayload();
            sock.send(payload.data(), payload.size(),
                      item.target, core::kDiscoveryPort);

            // Un SOLICIT broadcast issu d'un burst de rescan consomme un crédit.
            if (item.kind == OutgoingBeacon::Kind::Solicit &&
                item.target == sf::IpAddress::Broadcast &&
                scanBurstsRemaining_.load() > 0) {
                scanBurstsRemaining_.fetch_sub(1);
            }
        }

        std::this_thread::sleep_for(kBeaconTick);
    }
}

void DiscoveryService::listenLoop() {
    BroadcastUdpSocket sock;
    sock.setBlocking(false);
    if (sock.bind(core::kDiscoveryPort) != sf::Socket::Done) {
        core::log_error("Discovery: bind listener failed (port occupé ?)");
        return;
    }
    sock.enableBroadcast();

    constexpr std::size_t kBufSize = 2048;
    char buf[kBufSize];

    while (running_.load()) {
        std::size_t received = 0;
        sf::IpAddress sender;
        std::uint16_t port = 0;
        const auto st = sock.receive(buf, kBufSize, received, sender, port);

        if (st == sf::Socket::NotReady) {
            std::this_thread::sleep_for(milliseconds(100));
            continue;
        }
        if (st != sf::Socket::Done || received == 0) continue;

        try {
            auto j = nlohmann::json::parse(std::string(buf, received));
            if (j.value("proto", "") != "LTR1") continue;

            const std::string kind     = j.value("kind", "");
            const std::string senderId = j.value("id", "");
            if (senderId.empty() || senderId == self_.id) continue;

            if (kind == "SOLICIT") {
                // Répondre HELLO unicast au sender via la file d'émission.
                std::lock_guard<std::mutex> lock(outMu_);
                outQueue_.push_back(OutgoingBeacon{
                    OutgoingBeacon::Kind::HelloUnicast,
                    sender,
                    steady_clock::now()
                });
                continue;
            }

            if (kind != "HELLO") continue;

            domain::Device d;
            d.id       = senderId;
            d.name     = j.value("name", "");
            d.platform = j.value("platform", "");
            d.tcpPort  = j.value("tcpPort", static_cast<std::uint16_t>(0));

            // IP prioritaire depuis le payload (résout le cas Windows
            // multi-interfaces où sender peut valoir 0.0.0.0). Fallback sender.
            const auto ipStr = j.value("ip", std::string{});
            if (!ipStr.empty()) {
                sf::IpAddress parsed(ipStr);
                d.ip = (parsed != sf::IpAddress::None) ? parsed : sender;
            } else {
                d.ip = sender;
            }
            d.lastSeen = steady_clock::now();

            bool isNew = false;
            {
                std::lock_guard<std::mutex> lock(mu_);
                auto it = knownPeers_.find(d.id);
                if (it == knownPeers_.end()) {
                    knownPeers_.emplace(d.id, d);
                    isNew = true;
                } else {
                    it->second.lastSeen = d.lastSeen;
                    it->second.ip       = d.ip;
                    it->second.name     = d.name;
                    it->second.platform = d.platform;
                    it->second.tcpPort  = d.tcpPort;
                }
            }

            if (isNew) bus_.post(core::PeerSeenEvent{d});
        } catch (const std::exception&) {
            // packet malformé : ignorer
        }
    }
}

void DiscoveryService::ttlLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(milliseconds(500));
        const auto now = steady_clock::now();

        std::vector<std::string> lost;
        {
            std::lock_guard<std::mutex> lock(mu_);
            for (auto it = knownPeers_.begin(); it != knownPeers_.end(); ) {
                if (now - it->second.lastSeen > core::kPeerTtl) {
                    lost.push_back(it->first);
                    it = knownPeers_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& id : lost) {
            bus_.post(core::PeerLostEvent{std::move(id)});
        }
    }
}

} // namespace ltr::network
