#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/cidr_matcher.hpp"

namespace ltr::infra {

// Phase 3 — Contrôle IT. Service mince qui traduit la BusinessPolicy
// (chargée par PolicyService) en décisions runtime :
//   - autoriser ou non /api/p2p/*,
//   - vérifier qu'une IP cliente est dans la whitelist,
//   - dire si HTTPS est forcé,
//   - exposer les durées de rétention.
//
// Toutes les méthodes sont thread-safe. Le cache des CIDR parsés
// est rebuild explicitement via reload() (pas de hot-reload auto).
class PolicyEnforcementService {
public:
    explicit PolicyEnforcementService(PolicyService& policyService);

    // Recharge la policy depuis PolicyService et re-parse les CIDR.
    void reload();

    // Phase 3 — décisions.
    bool isP2PAllowed() const;
    bool httpsForced()  const;
    int  historyRetentionDays()       const;
    int  receivedFilesRetentionDays() const;

    // Override implicite : localhost ('127.0.0.1', '::1') TOUJOURS true,
    // quoi qu'il arrive (anti-brick du dashboard local).
    bool isIpAllowed(const std::string& ipStr) const;

    // Construit l'URL de redirection HTTPS (host + port + path).
    // Si httpsPort=0, retourne path inchangé (no-op).
    std::string buildHttpsRedirect(const std::string& host,
                                   std::uint16_t httpsPort,
                                   const std::string& pathAndQuery) const;

private:
    void rebuildCidrCacheLocked();

    PolicyService& policyService_;
    mutable std::mutex mu_;
    std::vector<CidrRange> allowedCidrs_;
    bool          loaded_{false};
};

} // namespace ltr::infra
