#include "ltr/infra/crypto_identity.hpp"

#include <picosha2.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <random>
#include <vector>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

constexpr std::size_t kNonceSize = 32;

std::array<std::uint8_t, kNonceSize> loadOrGenerateNonce(
        const std::filesystem::path& cfgDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(cfgDir, ec);

    const auto path = cfgDir / "identity.bin";
    std::array<std::uint8_t, kNonceSize> nonce{};

    if (fs::exists(path, ec)) {
        std::ifstream ifs(path, std::ios::binary);
        if (ifs) {
            ifs.read(reinterpret_cast<char*>(nonce.data()), kNonceSize);
            if (ifs.gcount() == static_cast<std::streamsize>(kNonceSize)) {
                return nonce;
            }
        }
        core::log_warn("[identity] identity.bin invalide, régénération");
    }

    // Génération d'un nonce 32 octets cryptographiquement random.
    std::random_device rd;
    std::mt19937_64 rng(rd() ^ static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    for (std::size_t i = 0; i < kNonceSize; ++i) {
        nonce[i] = static_cast<std::uint8_t>(rng() & 0xFF);
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (ofs) {
        ofs.write(reinterpret_cast<const char*>(nonce.data()), kNonceSize);
        core::log_info("[identity] nonce généré et persisté → "
                       + path.string());
    } else {
        core::log_error("[identity] impossible d'écrire " + path.string());
    }
    return nonce;
}

std::string formatFingerprint(const std::vector<std::uint8_t>& hash) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(hash.size() * 3);
    for (std::size_t i = 0; i < hash.size(); ++i) {
        if (i > 0) out.push_back(':');
        out.push_back(kHex[(hash[i] >> 4) & 0xF]);
        out.push_back(kHex[hash[i] & 0xF]);
    }
    return out;
}

} // namespace

std::string loadOrGenerateSelfFingerprint(
        const std::filesystem::path& cfgDir,
        const std::string& selfId) {
    const auto nonce = loadOrGenerateNonce(cfgDir);

    // input = nonce (32 octets) || selfId (UTF-8 bytes)
    std::vector<std::uint8_t> input;
    input.reserve(kNonceSize + selfId.size());
    input.insert(input.end(), nonce.begin(), nonce.end());
    for (char c : selfId) input.push_back(static_cast<std::uint8_t>(c));

    std::vector<std::uint8_t> hash(picosha2::k_digest_size);
    picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());

    auto fp = formatFingerprint(hash);
    core::log_info("[identity] self fingerprint = " + fp.substr(0, 23) + "...");
    return fp;
}

} // namespace ltr::infra
