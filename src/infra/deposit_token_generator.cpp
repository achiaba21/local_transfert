#include "ltr/infra/deposit_token_generator.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <random>

namespace ltr::infra {

namespace {

constexpr char kB64UrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

} // namespace

std::string base64UrlEncode(const unsigned char* data, std::size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len) {
        const std::uint32_t v = (std::uint32_t(data[i]) << 16) |
                                (std::uint32_t(data[i + 1]) << 8) |
                                (std::uint32_t(data[i + 2]));
        out.push_back(kB64UrlAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kB64UrlAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kB64UrlAlphabet[(v >> 6)  & 0x3F]);
        out.push_back(kB64UrlAlphabet[v         & 0x3F]);
        i += 3;
    }
    if (i < len) {
        const std::size_t rem = len - i;
        std::uint32_t v = std::uint32_t(data[i]) << 16;
        if (rem == 2) v |= std::uint32_t(data[i + 1]) << 8;
        out.push_back(kB64UrlAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kB64UrlAlphabet[(v >> 12) & 0x3F]);
        if (rem == 2) {
            out.push_back(kB64UrlAlphabet[(v >> 6) & 0x3F]);
        }
    }
    return out;
}

SecureRandomTokenGenerator::SecureRandomTokenGenerator() = default;

std::string SecureRandomTokenGenerator::generate() {
    // 32 octets d'entropie — combinés std::random_device + clock fallback
    // pour environnements où random_device pourrait être déterministe
    // (rare, mais on couvre le cas).
    std::random_device rd;
    std::array<std::uint64_t, 4> seedWords{};
    for (auto& w : seedWords) {
        w = (std::uint64_t(rd()) << 32) | std::uint64_t(rd());
    }
    seedWords[0] ^= static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());

    std::seed_seq seq(seedWords.begin(), seedWords.end());
    std::mt19937_64 rng(seq);

    unsigned char bytes[32];
    for (int i = 0; i < 4; ++i) {
        const std::uint64_t v = rng();
        bytes[i * 8 + 0] = static_cast<unsigned char>(v >> 56);
        bytes[i * 8 + 1] = static_cast<unsigned char>(v >> 48);
        bytes[i * 8 + 2] = static_cast<unsigned char>(v >> 40);
        bytes[i * 8 + 3] = static_cast<unsigned char>(v >> 32);
        bytes[i * 8 + 4] = static_cast<unsigned char>(v >> 24);
        bytes[i * 8 + 5] = static_cast<unsigned char>(v >> 16);
        bytes[i * 8 + 6] = static_cast<unsigned char>(v >> 8);
        bytes[i * 8 + 7] = static_cast<unsigned char>(v);
    }
    return base64UrlEncode(bytes, sizeof(bytes));
}

} // namespace ltr::infra
