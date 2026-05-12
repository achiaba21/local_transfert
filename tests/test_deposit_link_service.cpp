// Tests : DepositLinkService — création, refus PersonalFree, révocation,
// expiration, lookup par token et id.

#include <cassert>
#include <cstdio>
#include <filesystem>

#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_link_repository.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/infra/deposit_token_generator.hpp"

namespace {

class MemoryPolicyRepository : public ltr::infra::PolicyRepository {
public:
    explicit MemoryPolicyRepository(ltr::infra::BusinessPolicy p)
        : policy_(std::move(p)) {}
    ltr::infra::BusinessPolicy load() const override { return policy_; }
    void save(const ltr::infra::BusinessPolicy& p) const override {
        const_cast<MemoryPolicyRepository*>(this)->policy_ = p;
    }
private:
    ltr::infra::BusinessPolicy policy_;
};

class FixedTokenGen : public ltr::infra::DepositTokenGenerator {
public:
    std::string generate() override {
        return "tok_" + std::to_string(++counter_);
    }
private:
    int counter_{0};
};

std::filesystem::path tempJsonPath(const std::string& tag) {
    return std::filesystem::temp_directory_path()
        / ("ltr-test-deposit-link-" + tag + ".json");
}

} // namespace

int main() {
    using namespace ltr::infra;

    {
        // Plan PersonalFree → upsell_required.
        const auto path = tempJsonPath("free");
        std::filesystem::remove(path);
        BusinessPolicy free;
        free.plan = BusinessPlan::PersonalFree;
        MemoryPolicyRepository policyRepo(free);
        PolicyService policy(policyRepo);
        JsonDepositLinkRepository linkRepo(path);
        FixedTokenGen tokens;
        DepositLinkService svc(linkRepo, policy, tokens,
            []() -> std::int64_t { return 1000; });

        DepositLinkSpec spec;
        spec.label = "Test";
        const auto r = svc.create(spec);
        assert(!r.ok);
        assert(r.reason == "upsell_required");
        assert(svc.list().empty());
    }

    {
        // Plan Business → ok.
        const auto path = tempJsonPath("biz");
        std::filesystem::remove(path);
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.quota.enabled = false;
        MemoryPolicyRepository policyRepo(biz);
        PolicyService policy(policyRepo);
        JsonDepositLinkRepository linkRepo(path);
        FixedTokenGen tokens;
        std::int64_t now = 1000;
        DepositLinkService svc(linkRepo, policy, tokens,
            [&]() -> std::int64_t { return now; });

        DepositLinkSpec spec;
        spec.label              = "Pièces Dupont 2026";
        spec.consentText        = "J'accepte.";
        spec.maxBytesPerDeposit = 100;
        spec.maxFilesPerDeposit = 10;
        spec.expiresAt          = 2000;
        const auto r = svc.create(spec);
        assert(r.ok);
        assert(!r.value.id.empty());
        assert(!r.value.token.empty());
        assert(r.value.label == "Pièces Dupont 2026");
        assert(!r.value.revoked);

        // findByToken + findById.
        const auto byTok = svc.findByToken(r.value.token);
        assert(byTok && byTok->id == r.value.id);
        const auto byId = svc.findById(r.value.id);
        assert(byId && byId->token == r.value.token);

        // Active maintenant, inactive après expiresAt.
        assert(svc.isActive(r.value));
        now = 3000;
        assert(!svc.isActive(r.value));

        // Revoke.
        now = 1500;
        assert(svc.revoke(r.value.id));
        const auto reread = svc.findById(r.value.id);
        assert(reread && reread->revoked);
        assert(!svc.isActive(*reread));

        // Persistance entre instances : nouveau service avec même repo.
        JsonDepositLinkRepository repo2(path);
        DepositLinkService svc2(repo2, policy, tokens);
        assert(svc2.list().size() == 1);
        assert(svc2.list().front().revoked);
    }

    std::printf("test_deposit_link_service OK\n");
    return 0;
}
