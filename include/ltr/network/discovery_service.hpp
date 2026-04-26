#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <SFML/Network/IpAddress.hpp>

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"

namespace ltr::network {

class DiscoveryService {
public:
    DiscoveryService(core::EventBus& bus, domain::Device self);
    ~DiscoveryService();

    DiscoveryService(const DiscoveryService&)            = delete;
    DiscoveryService& operator=(const DiscoveryService&) = delete;

    void start();
    void stop();

    // Salve ponctuelle de SOLICIT broadcast (5 × 400ms) : permet de faire
    // apparaître immédiatement les pairs déjà allumés au lieu d'attendre
    // leur prochain HELLO périodique.
    void rescan();

    // SOLICIT unicast vers une IP précise : sert pour l'ajout manuel par
    // saisie utilisateur. Si le pair est joignable, il répondra par un HELLO
    // capté par listenLoop et ajouté à la liste des pairs connus.
    void probe(sf::IpAddress ip);

    // Indique si une salve de rescan est en cours (pour l'UI).
    bool isScanning() const noexcept { return scanBurstsRemaining_.load() > 0; }

    // Lookup thread-safe d'un pair par IP — utilisé pour le timeout de
    // probe() côté AppController.
    bool hasPeer(sf::IpAddress ip) const;

private:
    void beaconLoop();
    void listenLoop();
    void ttlLoop();

    // Message en attente d'émission par le beaconThread. Deux usages :
    //  - SOLICIT      : demande à un pair (ou tous en broadcast) de répondre
    //  - HelloUnicast : réponse à un SOLICIT reçu
    struct OutgoingBeacon {
        enum class Kind { Solicit, HelloUnicast };
        Kind kind{Kind::Solicit};
        sf::IpAddress target{sf::IpAddress::None};
        std::chrono::steady_clock::time_point sendAt{};
    };

    core::EventBus& bus_;
    domain::Device self_;

    std::atomic<bool> running_{false};
    std::thread beaconThread_;
    std::thread listenerThread_;
    std::thread ttlThread_;

    mutable std::mutex mu_;
    std::map<std::string, domain::Device> knownPeers_; // id -> device

    // File d'émission ponctuelle drainée par beaconLoop. Alimentée par
    // rescan(), probe(), et listenLoop() (réponses aux SOLICIT).
    std::mutex                    outMu_;
    std::deque<OutgoingBeacon>    outQueue_;
    std::atomic<int>              scanBurstsRemaining_{0};

    // IP locale mise en cache au démarrage du beaconThread pour être
    // insérée dans chaque HELLO (résout l'IP manquante sous Windows
    // multi-interfaces où `sender` vu par le pair peut valoir 0.0.0.0).
    std::string                   selfIpCached_;
};

} // namespace ltr::network
