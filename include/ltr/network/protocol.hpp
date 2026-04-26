#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SFML/Network/TcpSocket.hpp>

namespace ltr::network {

// Types de message. Les codes sont figés — ne jamais les réordonner.
enum class MessageType : std::uint8_t {
    Offer       = 0x01,
    Accept      = 0x02,
    Reject      = 0x03,
    FileHeader  = 0x04,
    FileChunk   = 0x05,
    FileEnd     = 0x06,
    Done        = 0x07,
    Cancel      = 0x08,
    Error       = 0x09,
    // V1.1.9 — Sprint Transfer Resume (protocole LTR1.1, backward-compat).
    // Un pair legacy LTR1 répond Reject face à ces types → fallback auto
    // vers Offer classique côté client.
    ResumeOffer    = 0x0A,
    ResumeResponse = 0x0B,
    Ping           = 0x0C,
    Pong           = 0x0D,
};

struct Frame {
    MessageType type{};
    std::vector<std::uint8_t> payload;
};

// Sérialise un message : [magic(4)="LTR1"][type(1)][len(4 BE)][payload].
std::vector<std::uint8_t> encodeFrame(MessageType type,
                                      const void* payload,
                                      std::size_t size);

// Envoi robuste sur un socket TCP (boucle jusqu'à tout envoyer).
sf::Socket::Status sendAll(sf::TcpSocket& sock,
                           const void* data,
                           std::size_t size);

// Réception exacte de `size` octets.
sf::Socket::Status receiveAll(sf::TcpSocket& sock,
                              void* data,
                              std::size_t size);

// Lit une frame complète depuis un socket TCP bloquant.
sf::Socket::Status readFrame(sf::TcpSocket& sock, Frame& out);

// Écrit une frame complète sur un socket TCP.
sf::Socket::Status writeFrame(sf::TcpSocket& sock,
                              MessageType type,
                              const void* payload,
                              std::size_t size);

// Helpers JSON pour payload.
sf::Socket::Status writeJsonFrame(sf::TcpSocket& sock,
                                  MessageType type,
                                  const std::string& json);

} // namespace ltr::network
