#pragma once

#include <SFML/Network/UdpSocket.hpp>

namespace ltr::network {

// SFML 2.6 n'expose pas SO_BROADCAST directement. On sous-classe pour
// l'activer après le bind. Indispensable pour diffuser vers 255.255.255.255
// sur Windows (et recommandé ailleurs).
class BroadcastUdpSocket : public sf::UdpSocket {
public:
    bool enableBroadcast();
};

} // namespace ltr::network
