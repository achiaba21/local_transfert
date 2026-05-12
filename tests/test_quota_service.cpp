#include "ltr/infra/quota_service.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>

using namespace ltr::infra;

namespace {

class MemoryPolicyRepository final : public PolicyRepository {
public:
    explicit MemoryPolicyRepository(BusinessPolicy policy)
        : policy_(std::move(policy)) {}

    BusinessPolicy load() const override { return policy_; }
    void save(const BusinessPolicy& policy) const override { policy_ = policy; }

private:
    mutable BusinessPolicy policy_;
};

class MemoryQuotaRepository final : public QuotaRepository {
public:
    std::optional<QuotaUsage> load() const override { return usage_; }
    void save(const QuotaUsage& usage) const override { usage_ = usage; }

private:
    mutable std::optional<QuotaUsage> usage_;
};

} // namespace

int main() {
    // 2026-05-12 00:00:00 UTC.
    std::int64_t now = 1778544000;
    assert(quotaPeriodUtc(now) == "2026-05");
    assert(quotaPeriodUtc(quotaNextResetUtc(now)) == "2026-06");

    BusinessPolicy freePolicy;
    MemoryPolicyRepository freePolicyRepo(freePolicy);
    PolicyService freePolicyService(freePolicyRepo);
    MemoryQuotaRepository freeQuotaRepo;
    QuotaService freeQuota(freeQuotaRepo, freePolicyService, [&] { return now; });
    auto freeDecision = freeQuota.tryReserve("free", TransferFlow::TcpOut, 10'000);
    assert(freeDecision.allowed);
    freeQuota.commit("free", 10'000);
    assert(freeQuota.usageSnapshot().usedBytes == 0);

    BusinessPolicy business;
    business.plan = BusinessPlan::Business;
    business.quota.enabled = true;
    business.quota.monthlyBytes = 100;
    business.quota.enforcement = QuotaEnforcement::HardBlock;
    MemoryPolicyRepository businessPolicyRepo(business);
    PolicyService businessPolicyService(businessPolicyRepo);
    MemoryQuotaRepository quotaRepo;
    QuotaService quota(quotaRepo, businessPolicyService, [&] { return now; });

    auto first = quota.tryReserve("a", TransferFlow::HttpUp, 60);
    assert(first.allowed);
    assert(quota.reservedBytesSnapshot() == 60);

    auto blocked = quota.tryReserve("b", TransferFlow::TcpOut, 50);
    assert(!blocked.allowed);
    assert(blocked.reason == "quota_exceeded");

    quota.commit("a", 55);
    assert(quota.usageSnapshot().usedBytes == 55);
    assert(quota.reservedBytesSnapshot() == 0);

    auto second = quota.tryReserve("c", TransferFlow::HttpDown, 45);
    assert(second.allowed);
    quota.release("c");
    assert(quota.reservedBytesSnapshot() == 0);

    // 2026-06-01 00:00:00 UTC.
    now = quotaNextResetUtc(now);
    assert(quota.usageSnapshot().period == "2026-06");
    assert(quota.usageSnapshot().usedBytes == 0);

    std::cout << "test_quota_service: OK\n";
    return 0;
}
