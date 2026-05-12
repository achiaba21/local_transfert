#include "ltr/infra/business_policy.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

namespace fs = std::filesystem;
using namespace ltr::infra;

namespace {

fs::path makeTempPath() {
    static std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
            .time_since_epoch().count())};
    return fs::temp_directory_path()
         / ("ltr_policy_" + std::to_string(rng()) + ".json");
}

} // namespace

int main() {
    assert(businessPlanFromStr("business") == BusinessPlan::Business);
    assert(businessPlanFromStr("business-plus") == BusinessPlan::BusinessPlus);
    assert(businessPlanFromStr("enterprise") == BusinessPlan::Enterprise);
    assert(businessPlanFromStr("unknown") == BusinessPlan::PersonalFree);
    assert(defaultBusinessMonthlyQuotaBytes() == 500ULL * 1024ULL * 1024ULL * 1024ULL);

    const auto path = makeTempPath();
    {
        JsonPolicyRepository repo(path);
        BusinessPolicy policy;
        policy.plan = BusinessPlan::Business;
        policy.licenseKey = "signed-key";
        policy.quota.enabled = true;
        policy.quota.monthlyBytes = 42;
        policy.quota.enforcement = QuotaEnforcement::HardBlock;
        policy.security.requireHttps = true;
        policy.network.allowP2P = false;
        policy.network.allowedCidrs = {"192.168.1.0/24"};
        policy.retention.historyDays = 90;
        policy.branding.organizationName = "Cabinet Test";
        repo.save(policy);
    }
    {
        JsonPolicyRepository repo(path);
        const auto loaded = repo.load();
        assert(loaded.plan == BusinessPlan::Business);
        assert(loaded.licenseKey == "signed-key");
        assert(loaded.quota.enabled);
        assert(loaded.quota.monthlyBytes == 42);
        assert(loaded.security.requireHttps);
        assert(!loaded.network.allowP2P);
        assert(loaded.network.allowedCidrs.size() == 1);
        assert(loaded.retention.historyDays == 90);
        assert(loaded.branding.organizationName == "Cabinet Test");
    }

    std::error_code ec;
    fs::remove(path, ec);
    std::cout << "test_business_policy: OK\n";
    return 0;
}
