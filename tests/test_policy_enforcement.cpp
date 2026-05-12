// Tests PolicyEnforcementService : whitelist vide, exclusive, localhost
// override, flags lus depuis BusinessPolicy.

#include <cassert>
#include <cstdio>

#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/policy_enforcement.hpp"

namespace {

class MemPolicyRepo : public ltr::infra::PolicyRepository {
public:
    explicit MemPolicyRepo(ltr::infra::BusinessPolicy p) : p_(p) {}
    ltr::infra::BusinessPolicy load() const override { return p_; }
    void save(const ltr::infra::BusinessPolicy& p) const override {
        const_cast<MemPolicyRepo*>(this)->p_ = p;
    }
    void mutate(ltr::infra::BusinessPolicy p) { p_ = p; }
private:
    ltr::infra::BusinessPolicy p_;
};

} // namespace

int main() {
    using namespace ltr::infra;

    // Cas 1 : policy par défaut (PersonalFree) — whitelist vide,
    // allowP2P=true, httpsForced=false.
    {
        BusinessPolicy def;
        MemPolicyRepo repo(def);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(pef.isP2PAllowed());
        assert(!pef.httpsForced());
        assert(pef.isIpAllowed("8.8.8.8"));         // whitelist vide = tout OK
        assert(pef.isIpAllowed("127.0.0.1"));
        assert(pef.isIpAllowed("::1"));
    }

    // Cas 2 : policy Business avec whitelist exclusive.
    {
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.network.allowedCidrs = {"10.0.0.0/8", "192.168.1.0/24"};
        MemPolicyRepo repo(biz);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(pef.isIpAllowed("10.0.0.5"));         // in 10/8
        assert(pef.isIpAllowed("192.168.1.42"));     // in 192.168.1/24
        assert(!pef.isIpAllowed("8.8.8.8"));         // not in whitelist
        assert(!pef.isIpAllowed("192.168.2.1"));     // not in whitelist
        assert(pef.isIpAllowed("127.0.0.1"));        // override localhost
        assert(pef.isIpAllowed("::1"));              // override localhost v6
    }

    // Cas 3 : allowP2P=false.
    {
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.network.allowP2P = false;
        MemPolicyRepo repo(biz);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(!pef.isP2PAllowed());
    }

    // Cas 4 : httpsForced + URL build.
    {
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.security.requireHttps = true;
        MemPolicyRepo repo(biz);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(pef.httpsForced());
        const auto url = pef.buildHttpsRedirect("192.168.1.42:45456",
                                                 45457, "/api/foo?bar=1");
        assert(url == "https://192.168.1.42:45457/api/foo?bar=1");
    }

    // Cas 5 : reload après mutation.
    {
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.network.allowedCidrs = {"10.0.0.0/8"};
        MemPolicyRepo repo(biz);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(pef.isIpAllowed("10.5.5.5"));
        assert(!pef.isIpAllowed("172.16.0.1"));

        // Mutate policy puis reload.
        BusinessPolicy bizNew = biz;
        bizNew.network.allowedCidrs = {"172.16.0.0/12"};
        repo.mutate(bizNew);
        policy.reload();
        pef.reload();
        assert(!pef.isIpAllowed("10.5.5.5"));
        assert(pef.isIpAllowed("172.16.0.1"));
    }

    // Cas 6 : CIDR invalide ignoré, les autres restent.
    {
        BusinessPolicy biz;
        biz.plan = BusinessPlan::Business;
        biz.network.allowedCidrs = {"INVALID", "10.0.0.0/8"};
        MemPolicyRepo repo(biz);
        PolicyService policy(repo);
        PolicyEnforcementService pef(policy);
        assert(pef.isIpAllowed("10.1.2.3"));
    }

    std::printf("test_policy_enforcement OK\n");
    return 0;
}
