#include "ltr/infra/deposit_link_service.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <random>

namespace ltr::infra {

namespace {

std::int64_t systemNowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

DepositLinkService::DepositLinkService(DepositLinkRepository& repository,
                                       PolicyService& policyService,
                                       DepositTokenGenerator& tokenGen,
                                       Clock clock)
    : repository_(repository),
      policyService_(policyService),
      tokenGen_(tokenGen),
      clock_(std::move(clock)) {
    if (!clock_) clock_ = systemNowSec;
}

std::string DepositLinkService::makeShortId() {
    // 12 hex = 48 bits, ample pour distinguer quelques milliers de liens.
    std::random_device rd;
    std::array<std::uint32_t, 4> seedWords{rd(), rd(), rd(), rd()};
    std::seed_seq seq(seedWords.begin(), seedWords.end());
    std::mt19937 rng(seq);
    char buf[13] = {0};
    std::snprintf(buf, sizeof(buf), "%08x%04x",
                  static_cast<unsigned>(rng()),
                  static_cast<unsigned>(rng() & 0xFFFFu));
    return std::string(buf);
}

void DepositLinkService::refreshCacheLocked() {
    cache_ = repository_.loadAll();
    cacheLoaded_ = true;
}

DepositResult<DepositLink>
DepositLinkService::create(const DepositLinkSpec& spec) {
    if (policyService_.current().plan == BusinessPlan::PersonalFree) {
        return DepositResult<DepositLink>::fail("upsell_required");
    }

    DepositLink link;
    link.id                 = makeShortId();
    link.token              = tokenGen_.generate();
    link.label              = spec.label;
    link.consentText        = spec.consentText;
    link.maxBytesPerDeposit = spec.maxBytesPerDeposit;
    link.maxFilesPerDeposit = spec.maxFilesPerDeposit;
    link.createdAt          = clock_();
    link.expiresAt          = spec.expiresAt;
    link.revoked            = false;

    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!cacheLoaded_) refreshCacheLocked();
        repository_.save(link);
        cache_.push_back(link);
    }
    return DepositResult<DepositLink>::success(std::move(link));
}

bool DepositLinkService::revoke(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cacheLoaded_) refreshCacheLocked();
    for (auto& l : cache_) {
        if (l.id == id) {
            if (l.revoked) return true;
            l.revoked = true;
            repository_.save(l);
            return true;
        }
    }
    return false;
}

std::optional<DepositLink>
DepositLinkService::findByToken(const std::string& token) {
    if (token.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mu_);
    if (!cacheLoaded_) refreshCacheLocked();
    for (const auto& l : cache_) {
        if (l.token == token) return l;
    }
    return std::nullopt;
}

std::optional<DepositLink>
DepositLinkService::findById(const std::string& id) {
    if (id.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mu_);
    if (!cacheLoaded_) refreshCacheLocked();
    for (const auto& l : cache_) {
        if (l.id == id) return l;
    }
    return std::nullopt;
}

std::vector<DepositLink> DepositLinkService::list() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cacheLoaded_) refreshCacheLocked();
    auto copy = cache_;
    std::sort(copy.begin(), copy.end(),
        [](const DepositLink& a, const DepositLink& b) {
            return a.createdAt > b.createdAt;
        });
    return copy;
}

bool DepositLinkService::isActive(const DepositLink& link) {
    return depositLinkIsActive(link, clock_());
}

} // namespace ltr::infra
