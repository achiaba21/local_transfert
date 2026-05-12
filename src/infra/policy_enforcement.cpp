#include "ltr/infra/policy_enforcement.hpp"

#include "ltr/core/logger.hpp"

namespace ltr::infra {

PolicyEnforcementService::PolicyEnforcementService(PolicyService& policyService)
    : policyService_(policyService) {
    reload();
}

void PolicyEnforcementService::reload() {
    std::lock_guard<std::mutex> lock(mu_);
    rebuildCidrCacheLocked();
    loaded_ = true;
}

void PolicyEnforcementService::rebuildCidrCacheLocked() {
    allowedCidrs_.clear();
    const auto& policy = policyService_.current();
    for (const auto& s : policy.network.allowedCidrs) {
        if (auto r = parseCidr(s)) {
            allowedCidrs_.push_back(*r);
        } else {
            core::log_warn("[policy] CIDR invalide ignoré : " + s);
        }
    }
}

bool PolicyEnforcementService::isP2PAllowed() const {
    return policyService_.current().network.allowP2P;
}

bool PolicyEnforcementService::httpsForced() const {
    return policyService_.current().security.requireHttps;
}

int PolicyEnforcementService::historyRetentionDays() const {
    return policyService_.current().retention.historyDays;
}

int PolicyEnforcementService::receivedFilesRetentionDays() const {
    return policyService_.current().retention.receivedFilesDays;
}

bool PolicyEnforcementService::isIpAllowed(const std::string& ip) const {
    // Override anti-brick.
    if (ip == "127.0.0.1" || ip == "::1" || ip == "localhost" ||
        ip == "::ffff:127.0.0.1") return true;

    std::lock_guard<std::mutex> lock(mu_);
    if (allowedCidrs_.empty()) return true;  // whitelist vide = tout autorisé
    for (const auto& r : allowedCidrs_) {
        if (matchCidr(r, ip)) return true;
    }
    return false;
}

std::string
PolicyEnforcementService::buildHttpsRedirect(const std::string& host,
                                             std::uint16_t httpsPort,
                                             const std::string& pathAndQuery) const {
    if (httpsPort == 0) return pathAndQuery;
    std::string h = host;
    // Strip ':<oldport>' s'il existe.
    const auto colon = h.rfind(':');
    if (colon != std::string::npos) h = h.substr(0, colon);
    std::string out = "https://" + h + ":" + std::to_string(httpsPort);
    if (!pathAndQuery.empty() && pathAndQuery.front() != '/') out += "/";
    out += pathAndQuery;
    return out;
}

} // namespace ltr::infra
