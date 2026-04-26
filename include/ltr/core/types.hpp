#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>

namespace ltr::core {

// Ports réseau par défaut.
constexpr std::uint16_t kDiscoveryPort = 45454;
constexpr std::uint16_t kTransferPort  = 45455;
constexpr std::uint16_t kWebPort       = 45456;

// Plage de fallback si kWebPort est occupé : on essaie successivement
// kWebPort, kWebPort+1, ..., kWebPort+kWebPortFallbackRange-1.
constexpr std::uint16_t kWebPortFallbackRange = 10;

// Durée d'inactivité avant expiration d'une session web.
constexpr auto kWebSessionTtl = std::chrono::seconds(30);

// Intervalle de ré-émission du PeerSeenEvent pour les sessions web
// (keepalive pour que la session reste dans la liste desktop).
constexpr auto kWebKeepaliveInterval = std::chrono::milliseconds(2000);

// Durée de vie d'un ticket de download (host → browser).
constexpr auto kDownloadTicketTtl = std::chrono::minutes(5);

// Taille d'un chunk de fichier (TCP FILE_CHUNK).
constexpr std::size_t kChunkSize = 256 * 1024; // 256 KB

// V1.1.3 : chunk HTTP côté web (download host → browser). Plus grand que
// kChunkSize car le contexte est différent :
//  - moins de syscalls write() → throughput +10-15 % sur LAN
//  - moins d'events TransferProgressEvent dispersés → UI plus fluide
//  - cohérent avec la taille des buffers TCP modernes (~1 Mo)
constexpr std::size_t kHttpChunkSize = 1024 * 1024; // 1 MB

// Rythme d'émission du beacon de découverte.
constexpr auto kBeaconInterval = std::chrono::milliseconds(2000);

// TTL pour considérer un pair comme déconnecté.
constexpr auto kPeerTtl = std::chrono::seconds(6);

// Timeout d'acceptation côté destinataire.
constexpr auto kOfferTimeout = std::chrono::seconds(30);

} // namespace ltr::core
