#include "ltr/network/protocol.hpp"

#include <array>
#include <cstring>

namespace ltr::network {

namespace {

constexpr std::array<std::uint8_t, 4> kMagic = {'L', 'T', 'R', '1'};

void writeBE32(std::uint8_t* dst, std::uint32_t v) {
    dst[0] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    dst[1] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    dst[2] = static_cast<std::uint8_t>((v >>  8) & 0xFF);
    dst[3] = static_cast<std::uint8_t>( v        & 0xFF);
}

std::uint32_t readBE32(const std::uint8_t* src) {
    return (static_cast<std::uint32_t>(src[0]) << 24) |
           (static_cast<std::uint32_t>(src[1]) << 16) |
           (static_cast<std::uint32_t>(src[2]) <<  8) |
            static_cast<std::uint32_t>(src[3]);
}

} // namespace

std::vector<std::uint8_t> encodeFrame(MessageType type,
                                      const void* payload,
                                      std::size_t size) {
    std::vector<std::uint8_t> out;
    out.resize(4 + 1 + 4 + size);
    std::memcpy(out.data(), kMagic.data(), 4);
    out[4] = static_cast<std::uint8_t>(type);
    writeBE32(out.data() + 5, static_cast<std::uint32_t>(size));
    if (size > 0 && payload != nullptr) {
        std::memcpy(out.data() + 9, payload, size);
    }
    return out;
}

sf::Socket::Status sendAll(sf::TcpSocket& sock,
                           const void* data,
                           std::size_t size) {
    const auto* p = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < size) {
        std::size_t justSent = 0;
        const auto st = sock.send(p + sent, size - sent, justSent);
        if (st == sf::Socket::Partial) {
            sent += justSent;
            continue;
        }
        if (st != sf::Socket::Done) return st;
        sent += justSent;
    }
    return sf::Socket::Done;
}

sf::Socket::Status receiveAll(sf::TcpSocket& sock,
                              void* data,
                              std::size_t size) {
    auto* p = static_cast<char*>(data);
    std::size_t got = 0;
    while (got < size) {
        std::size_t received = 0;
        const auto st = sock.receive(p + got, size - got, received);
        if (st == sf::Socket::Partial) {
            got += received;
            continue;
        }
        if (st != sf::Socket::Done) return st;
        got += received;
    }
    return sf::Socket::Done;
}

sf::Socket::Status readFrame(sf::TcpSocket& sock, Frame& out) {
    std::uint8_t header[9];
    auto st = receiveAll(sock, header, sizeof(header));
    if (st != sf::Socket::Done) return st;

    if (std::memcmp(header, kMagic.data(), 4) != 0) {
        return sf::Socket::Error;
    }
    out.type = static_cast<MessageType>(header[4]);
    const std::uint32_t len = readBE32(header + 5);

    out.payload.resize(len);
    if (len > 0) {
        st = receiveAll(sock, out.payload.data(), len);
        if (st != sf::Socket::Done) return st;
    }
    return sf::Socket::Done;
}

sf::Socket::Status writeFrame(sf::TcpSocket& sock,
                              MessageType type,
                              const void* payload,
                              std::size_t size) {
    std::uint8_t header[9];
    std::memcpy(header, kMagic.data(), 4);
    header[4] = static_cast<std::uint8_t>(type);
    writeBE32(header + 5, static_cast<std::uint32_t>(size));

    auto st = sendAll(sock, header, sizeof(header));
    if (st != sf::Socket::Done) return st;
    if (size == 0) return sf::Socket::Done;
    return sendAll(sock, payload, size);
}

sf::Socket::Status writeJsonFrame(sf::TcpSocket& sock,
                                  MessageType type,
                                  const std::string& json) {
    return writeFrame(sock, type, json.data(), json.size());
}

} // namespace ltr::network
