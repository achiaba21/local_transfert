#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <SFML/Network/IpAddress.hpp>

namespace ltr::domain {

// Type de pair : application native (découverte LAN + protocole TCP LTR1)
// ou session navigateur authentifiée via l'interface web.
enum class PeerKind : std::uint8_t {
    Native = 0,
    Web    = 1,
};

struct Device {
    std::string id;          // UUID v4 (Native) ou sessionToken (Web)
    std::string name;        // "Mac de Serge" ou "iPhone (Safari)"
    std::string platform;    // "macOS", "Windows", "iOS", "Android", ...
    sf::IpAddress ip{sf::IpAddress::None};
    std::uint16_t tcpPort{0};
    std::chrono::steady_clock::time_point lastSeen{};

    // Classement natif/web. Default Native pour compat descendante.
    PeerKind kind{PeerKind::Native};

    // Renseigné uniquement si kind == Web. Token d'authentification
    // serveur pour router les pushs via WebService::pushFiles.
    std::string sessionToken;
};

} // namespace ltr::domain
