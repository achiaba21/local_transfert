#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ltr::infra {

enum class BusinessPlan : std::uint8_t {
    PersonalFree,
    Business,
    BusinessPlus,
    Enterprise,
};

enum class QuotaEnforcement : std::uint8_t {
    HardBlock,
    SoftThrottle,
};

struct QuotaPolicy {
    bool enabled{false};
    std::uint64_t monthlyBytes{0};
    std::string resetMode{"monthly_utc"};
    QuotaEnforcement enforcement{QuotaEnforcement::HardBlock};
};

struct SecurityPolicy {
    bool requireHttps{false};
};

struct NetworkPolicy {
    bool allowP2P{true};
    std::vector<std::string> allowedCidrs;
};

struct RetentionPolicy {
    int historyDays{180};
    int receivedFilesDays{0};
};

struct BrandingPolicy {
    std::string organizationName;
    std::filesystem::path logoPath;
};

struct BusinessPolicy {
    BusinessPlan plan{BusinessPlan::PersonalFree};
    std::string licenseKey;
    QuotaPolicy quota;
    SecurityPolicy security;
    NetworkPolicy network;
    RetentionPolicy retention;
    BrandingPolicy branding;

    bool isPaidPlan() const noexcept;
};

class PolicyRepository {
public:
    virtual ~PolicyRepository() = default;
    virtual BusinessPolicy load() const = 0;
    virtual void save(const BusinessPolicy& policy) const = 0;
};

class JsonPolicyRepository final : public PolicyRepository {
public:
    explicit JsonPolicyRepository(std::filesystem::path path);

    BusinessPolicy load() const override;
    void save(const BusinessPolicy& policy) const override;

private:
    std::filesystem::path path_;
};

class PolicyService {
public:
    explicit PolicyService(PolicyRepository& repository);

    const BusinessPolicy& current() const noexcept { return policy_; }
    const BusinessPolicy& reload();
    void save(const BusinessPolicy& policy);

private:
    PolicyRepository& repository_;
    BusinessPolicy policy_;
};

const char* businessPlanToStr(BusinessPlan plan);
BusinessPlan businessPlanFromStr(const std::string& value);
const char* quotaEnforcementToStr(QuotaEnforcement enforcement);
QuotaEnforcement quotaEnforcementFromStr(const std::string& value);

std::uint64_t defaultBusinessMonthlyQuotaBytes();

} // namespace ltr::infra
