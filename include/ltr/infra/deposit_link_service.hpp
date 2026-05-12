#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_link_repository.hpp"
#include "ltr/infra/deposit_result.hpp"
#include "ltr/infra/deposit_token_generator.hpp"

namespace ltr::infra {

// Spécification fournie par l'UI host pour créer un lien.
struct DepositLinkSpec {
    std::string   label;
    std::string   consentText;
    std::uint64_t maxBytesPerDeposit{0};
    int           maxFilesPerDeposit{0};
    std::int64_t  expiresAt{0};
};

// CRUD + lifecycle des liens. SRP : cette classe ne sait rien du HTTP,
// de l'UI ni du filesystem hors repository.
class DepositLinkService {
public:
    using Clock = std::function<std::int64_t()>;

    DepositLinkService(DepositLinkRepository& repository,
                       PolicyService& policyService,
                       DepositTokenGenerator& tokenGen,
                       Clock clock = {});

    // Refuse si plan == PersonalFree (reason "upsell_required").
    DepositResult<DepositLink> create(const DepositLinkSpec& spec);

    // Marque revoked=true et sauvegarde. Retourne false si id inconnu.
    bool revoke(const std::string& id);

    std::optional<DepositLink> findByToken(const std::string& token);
    std::optional<DepositLink> findById(const std::string& id);

    // Snapshot trié par createdAt DESC.
    std::vector<DepositLink> list();

    // Helper d'usage : appelle depositLinkIsActive avec l'horloge injectée.
    bool isActive(const DepositLink& link);

private:
    void refreshCacheLocked();
    static std::string makeShortId();

    DepositLinkRepository&  repository_;
    PolicyService&          policyService_;
    DepositTokenGenerator&  tokenGen_;
    Clock                   clock_;
    std::mutex              mu_;
    std::vector<DepositLink> cache_;
    bool                    cacheLoaded_{false};
};

} // namespace ltr::infra
