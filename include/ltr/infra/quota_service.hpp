#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "ltr/infra/business_policy.hpp"

namespace ltr::infra {

enum class TransferFlow : std::uint8_t {
    TcpOut,
    TcpIn,
    HttpUp,
    HttpDown,
};

struct QuotaUsage {
    std::string period;
    std::uint64_t usedBytes{0};
};

struct QuotaDecision {
    bool allowed{true};
    std::string reason;
    std::string period;
    std::uint64_t requestedBytes{0};
    std::uint64_t usedBytes{0};
    std::uint64_t reservedBytes{0};
    std::uint64_t limitBytes{0};
    std::int64_t resetAt{0};
};

class TransferQuota {
public:
    virtual ~TransferQuota() = default;
    virtual QuotaDecision tryReserve(const std::string& reservationId,
                                     TransferFlow flow,
                                     std::uint64_t expectedBytes) = 0;
    virtual void commit(const std::string& reservationId,
                        std::uint64_t actualBytes) = 0;
    virtual void release(const std::string& reservationId) = 0;
};

class QuotaRepository {
public:
    virtual ~QuotaRepository() = default;
    virtual std::optional<QuotaUsage> load() const = 0;
    virtual void save(const QuotaUsage& usage) const = 0;
};

class JsonQuotaRepository final : public QuotaRepository {
public:
    explicit JsonQuotaRepository(std::filesystem::path path);

    std::optional<QuotaUsage> load() const override;
    void save(const QuotaUsage& usage) const override;

private:
    std::filesystem::path path_;
};

class QuotaService final : public TransferQuota {
public:
    using Clock = std::function<std::int64_t()>;

    QuotaService(QuotaRepository& repository,
                 PolicyService& policyService,
                 Clock clock = {});

    QuotaDecision tryReserve(const std::string& reservationId,
                             TransferFlow flow,
                             std::uint64_t expectedBytes) override;
    void commit(const std::string& reservationId,
                std::uint64_t actualBytes) override;
    void release(const std::string& reservationId) override;

    QuotaUsage usageSnapshot();
    std::uint64_t reservedBytesSnapshot();

private:
    struct Reservation {
        std::string period;
        TransferFlow flow{TransferFlow::TcpOut};
        std::uint64_t bytes{0};
    };

    void ensureLoaded();
    void ensureCurrentPeriod(std::int64_t now);
    std::uint64_t reservedBytesLocked() const;

    QuotaRepository& repository_;
    PolicyService& policyService_;
    Clock clock_;
    std::mutex mu_;
    bool loaded_{false};
    QuotaUsage usage_;
    std::unordered_map<std::string, Reservation> reservations_;
};

const char* transferFlowToStr(TransferFlow flow);
std::string quotaPeriodUtc(std::int64_t epochSec);
std::int64_t quotaNextResetUtc(std::int64_t epochSec);

} // namespace ltr::infra
