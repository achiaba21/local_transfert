#include "ltr/infra/deposit_receipt.hpp"

#include <array>
#include <cstdio>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>
#include <picosha2.h>

namespace ltr::infra {

namespace {

nlohmann::json receiptFileToJson(const DepositReceiptFile& f) {
    return {
        {"name",   f.name},
        {"size",   f.size},
        {"sha256", f.sha256},
    };
}

// Sérialisation canonique : clés triées + dump sans indentation, pour que
// build()/verify() produisent le même bytes-for-bytes.
nlohmann::json receiptToCanonicalJson(const DepositReceipt& r) {
    nlohmann::json files = nlohmann::json::array();
    for (const auto& f : r.files) files.push_back(receiptFileToJson(f));

    nlohmann::ordered_json j;
    j["id"]              = r.id;
    j["sessionId"]       = r.sessionId;
    j["linkId"]          = r.linkId;
    j["linkLabel"]       = r.linkLabel;
    j["depositorName"]   = r.depositorName;
    j["consentAccepted"] = r.consentAccepted;
    j["files"]           = files;
    j["totalBytes"]      = r.totalBytes;
    j["createdAt"]       = r.createdAt;
    return j;
}

} // namespace

std::string makeDepositReceiptId() {
    std::random_device rd;
    std::array<std::uint32_t, 4> seedWords{rd(), rd(), rd(), rd()};
    std::seed_seq seq(seedWords.begin(), seedWords.end());
    std::mt19937 rng(seq);
    char buf[17] = {0};
    std::snprintf(buf, sizeof(buf), "%08x%08x",
                  static_cast<unsigned>(rng()),
                  static_cast<unsigned>(rng()));
    return std::string(buf);
}

DepositReceiptService::DepositReceiptService(std::string hmacSecret)
    : hmacSecret_(std::move(hmacSecret)) {}

std::string
DepositReceiptService::canonicalPayload(const DepositReceipt& r) const {
    return receiptToCanonicalJson(r).dump();
}

// HMAC-SHA-256 via picosha2 (RFC 2104). Identique à WebSessionStore.
std::string
DepositReceiptService::hmacSha256Hex(const std::string& key,
                                     const std::string& message) const {
    constexpr std::size_t kBlockSize = 64;
    std::string k = key;
    if (k.size() > kBlockSize) {
        std::vector<unsigned char> kHash(picosha2::k_digest_size);
        picosha2::hash256(k.begin(), k.end(), kHash.begin(), kHash.end());
        k.assign(kHash.begin(), kHash.end());
    }
    if (k.size() < kBlockSize) k.resize(kBlockSize, '\0');

    std::string ipad(kBlockSize, '\0'), opad(kBlockSize, '\0');
    for (std::size_t i = 0; i < kBlockSize; ++i) {
        ipad[i] = static_cast<char>(k[i] ^ 0x36);
        opad[i] = static_cast<char>(k[i] ^ 0x5C);
    }
    const std::string innerInput = ipad + message;
    std::vector<unsigned char> innerHash(picosha2::k_digest_size);
    picosha2::hash256(innerInput.begin(), innerInput.end(),
                      innerHash.begin(), innerHash.end());
    const std::string outerInput = opad
        + std::string(innerHash.begin(), innerHash.end());
    return picosha2::hash256_hex_string(outerInput);
}

DepositReceipt
DepositReceiptService::build(const DepositSession& session,
                             const DepositLink& link,
                             std::int64_t nowEpochSec) const {
    DepositReceipt r;
    r.id              = makeDepositReceiptId();
    r.sessionId       = session.id;
    r.linkId          = link.id;
    r.linkLabel       = link.label;
    r.depositorName   = session.depositorName;
    r.consentAccepted = session.consentAccepted;
    r.totalBytes      = session.totalBytes;
    r.createdAt       = nowEpochSec;
    r.files.reserve(session.files.size());
    for (const auto& f : session.files) {
        r.files.push_back({f.name, f.size, f.sha256});
    }
    r.signature = hmacSha256Hex(hmacSecret_, canonicalPayload(r));
    return r;
}

bool DepositReceiptService::verify(const DepositReceipt& receipt) const {
    if (receipt.signature.empty()) return false;
    const auto expected = hmacSha256Hex(hmacSecret_, canonicalPayload(receipt));
    return expected == receipt.signature;
}

std::string DepositReceiptService::toJson(const DepositReceipt& receipt) const {
    auto j = receiptToCanonicalJson(receipt);
    j["signature"] = receipt.signature;
    return j.dump(2);
}

} // namespace ltr::infra
