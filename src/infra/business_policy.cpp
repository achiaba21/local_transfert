#include "ltr/infra/business_policy.hpp"

#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

constexpr std::uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

bool parseEnabledDefault(BusinessPlan plan) {
    return plan != BusinessPlan::PersonalFree;
}

} // namespace

bool BusinessPolicy::isPaidPlan() const noexcept {
    return plan != BusinessPlan::PersonalFree;
}

const char* businessPlanToStr(BusinessPlan plan) {
    switch (plan) {
        case BusinessPlan::PersonalFree: return "personal-free";
        case BusinessPlan::Business:     return "business";
        case BusinessPlan::BusinessPlus: return "business-plus";
        case BusinessPlan::Enterprise:   return "enterprise";
    }
    return "personal-free";
}

BusinessPlan businessPlanFromStr(const std::string& value) {
    if (value == "business") return BusinessPlan::Business;
    if (value == "business-plus") return BusinessPlan::BusinessPlus;
    if (value == "enterprise") return BusinessPlan::Enterprise;
    return BusinessPlan::PersonalFree;
}

const char* quotaEnforcementToStr(QuotaEnforcement enforcement) {
    switch (enforcement) {
        case QuotaEnforcement::HardBlock:    return "hard-block";
        case QuotaEnforcement::SoftThrottle: return "soft-throttle";
    }
    return "hard-block";
}

QuotaEnforcement quotaEnforcementFromStr(const std::string& value) {
    if (value == "soft-throttle") return QuotaEnforcement::SoftThrottle;
    return QuotaEnforcement::HardBlock;
}

std::uint64_t defaultBusinessMonthlyQuotaBytes() {
    return 500ULL * kGiB;
}

JsonPolicyRepository::JsonPolicyRepository(std::filesystem::path path)
    : path_(std::move(path)) {}

BusinessPolicy JsonPolicyRepository::load() const {
    BusinessPolicy policy;

    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
        return policy;
    }

    try {
        std::ifstream in(path_);
        const auto j = nlohmann::json::parse(in);

        policy.plan = businessPlanFromStr(
            j.value("plan", std::string{"personal-free"}));
        policy.licenseKey = j.value("licenseKey", std::string{});

        const auto quota = j.value("quota", nlohmann::json::object());
        policy.quota.enabled = quota.value(
            "enabled", parseEnabledDefault(policy.plan));
        policy.quota.monthlyBytes = quota.value(
            "monthlyBytes",
            policy.isPaidPlan() ? defaultBusinessMonthlyQuotaBytes()
                                : std::uint64_t{0});
        policy.quota.resetMode = quota.value(
            "resetMode", std::string{"monthly_utc"});
        policy.quota.enforcement = quotaEnforcementFromStr(
            quota.value("enforcement", std::string{"hard-block"}));

        const auto security = j.value("security", nlohmann::json::object());
        policy.security.requireHttps = security.value("requireHttps", false);

        const auto network = j.value("network", nlohmann::json::object());
        policy.network.allowP2P = network.value("allowP2P", true);
        policy.network.allowedCidrs.clear();
        for (const auto& cidr : network.value(
                 "allowedCidrs", nlohmann::json::array())) {
            if (cidr.is_string()) policy.network.allowedCidrs.push_back(cidr.get<std::string>());
        }

        const auto retention = j.value("retention", nlohmann::json::object());
        policy.retention.historyDays = retention.value("historyDays", 180);
        policy.retention.receivedFilesDays = retention.value("receivedFilesDays", 0);

        const auto branding = j.value("branding", nlohmann::json::object());
        policy.branding.organizationName = branding.value(
            "organizationName", std::string{});
        policy.branding.logoPath = branding.value("logoPath", std::string{});
    } catch (const std::exception& e) {
        core::log_warn(std::string("Business policy parse failed: ") + e.what());
        return BusinessPolicy{};
    }

    return policy;
}

void JsonPolicyRepository::save(const BusinessPolicy& policy) const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    nlohmann::json j;
    j["plan"] = businessPlanToStr(policy.plan);
    j["licenseKey"] = policy.licenseKey;
    j["quota"] = {
        {"enabled", policy.quota.enabled},
        {"monthlyBytes", policy.quota.monthlyBytes},
        {"resetMode", policy.quota.resetMode},
        {"enforcement", quotaEnforcementToStr(policy.quota.enforcement)},
    };
    j["security"] = {{"requireHttps", policy.security.requireHttps}};
    j["network"] = {
        {"allowP2P", policy.network.allowP2P},
        {"allowedCidrs", policy.network.allowedCidrs},
    };
    j["retention"] = {
        {"historyDays", policy.retention.historyDays},
        {"receivedFilesDays", policy.retention.receivedFilesDays},
    };
    j["branding"] = {
        {"organizationName", policy.branding.organizationName},
        {"logoPath", policy.branding.logoPath.string()},
    };

    std::ofstream out(path_, std::ios::trunc);
    out << j.dump(2);
}

PolicyService::PolicyService(PolicyRepository& repository)
    : repository_(repository),
      policy_(repository_.load()) {}

const BusinessPolicy& PolicyService::reload() {
    policy_ = repository_.load();
    return policy_;
}

void PolicyService::save(const BusinessPolicy& policy) {
    repository_.save(policy);
    policy_ = policy;
}

} // namespace ltr::infra
