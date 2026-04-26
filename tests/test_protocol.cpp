// Test unitaire minimaliste sur l'encodage de frames du protocole.
// Vérifie l'entête magic / type / taille sans ouvrir de socket.

#include "ltr/network/protocol.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

int main() {
    using namespace ltr::network;

    const std::string payload = R"({"sessionId":"abc","pinCode":"1234"})";
    auto bytes = encodeFrame(MessageType::Offer,
                             payload.data(), payload.size());

    assert(bytes.size() == 4 + 1 + 4 + payload.size());
    assert(bytes[0] == 'L' && bytes[1] == 'T' &&
           bytes[2] == 'R' && bytes[3] == '1');
    assert(bytes[4] == static_cast<std::uint8_t>(MessageType::Offer));

    const std::uint32_t len =
        (std::uint32_t(bytes[5]) << 24) |
        (std::uint32_t(bytes[6]) << 16) |
        (std::uint32_t(bytes[7]) <<  8) |
         std::uint32_t(bytes[8]);
    assert(len == payload.size());

    assert(std::memcmp(bytes.data() + 9,
                       payload.data(), payload.size()) == 0);

    // Frame vide.
    auto empty = encodeFrame(MessageType::Done, nullptr, 0);
    assert(empty.size() == 9);
    assert(empty[4] == static_cast<std::uint8_t>(MessageType::Done));

    std::cout << "test_protocol OK\n";
    return 0;
}
