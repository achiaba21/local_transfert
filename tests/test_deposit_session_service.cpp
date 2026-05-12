// Tests : DepositSessionService — begin (refus consent/name/expired),
// addFile (limites + quota), finalize (history + event), idempotent.

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "ltr/core/event_bus.hpp"
#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_link_repository.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/infra/deposit_receipt.hpp"
#include "ltr/infra/deposit_session_repository.hpp"
#include "ltr/infra/deposit_session_service.hpp"
#include "ltr/infra/deposit_token_generator.hpp"
#include "ltr/infra/quota_service.hpp"

namespace {

class MemPolicyRepo : public ltr::infra::PolicyRepository {
public:
    explicit MemPolicyRepo(ltr::infra::BusinessPolicy p) : p_(p) {}
    ltr::infra::BusinessPolicy load() const override { return p_; }
    void save(const ltr::infra::BusinessPolicy& p) const override {
        const_cast<MemPolicyRepo*>(this)->p_ = p;
    }
private:
    ltr::infra::BusinessPolicy p_;
};

class FixedTok : public ltr::infra::DepositTokenGenerator {
public:
    std::string generate() override { return "token-fixed"; }
};

std::filesystem::path tmp(const std::string& tag) {
    return std::filesystem::temp_directory_path() / ("ltr-test-dss-" + tag);
}

} // namespace

int main() {
    using namespace ltr::infra;
    using ltr::core::DepositReceivedEvent;

    const auto root = tmp("root");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    BusinessPolicy biz;
    biz.plan = BusinessPlan::Business;
    biz.quota.enabled = false;
    MemPolicyRepo policyRepo(biz);
    PolicyService policy(policyRepo);

    const auto linksPath = tmp("links.json");
    std::filesystem::remove(linksPath);
    JsonDepositLinkRepository linkRepo(linksPath);
    FixedTok tokens;
    std::int64_t now = 1000;
    DepositLinkService linkSvc(linkRepo, policy, tokens,
        [&]() -> std::int64_t { return now; });

    DepositLinkSpec spec;
    spec.label              = "Test";
    spec.consentText        = "OK";
    spec.maxBytesPerDeposit = 50;
    spec.maxFilesPerDeposit = 2;
    spec.expiresAt          = 2000;
    const auto lr = linkSvc.create(spec);
    assert(lr.ok);
    const auto link = lr.value;

    // QuotaService désactivé (plan biz mais quota.enabled=false).
    const auto quotaPath = tmp("quota.json");
    std::filesystem::remove(quotaPath);
    JsonQuotaRepository quotaRepo(quotaPath);
    QuotaService quota(quotaRepo, policy);

    const auto sessionsPath = tmp("sessions.json");
    std::filesystem::remove(sessionsPath);
    JsonDepositSessionRepository sessionRepo(sessionsPath);

    DepositReceiptService receipts("secret");

    const auto histPath = tmp("history.json");
    std::filesystem::remove(histPath);
    JsonDepositHistoryRepository histRepo(histPath);
    DepositHistoryStore history(histRepo);
    history.load();

    ltr::core::EventBus bus;
    DepositSessionService svc(sessionRepo, linkSvc, quota, receipts,
                              history, bus, root,
                              [&]() -> std::int64_t { return now; });

    // begin sans consent → refus.
    {
        const auto r = svc.begin(link.token, "Marie", false);
        assert(!r.ok);
        assert(r.reason == "consent_required");
    }
    // begin sans nom → refus.
    {
        const auto r = svc.begin(link.token, "", true);
        assert(!r.ok);
        assert(r.reason == "name_required");
    }
    // begin avec mauvais token → not_found.
    {
        const auto r = svc.begin("bad-token", "Marie", true);
        assert(!r.ok);
        assert(r.reason == "not_found");
    }
    // begin OK.
    auto sess = svc.begin(link.token, "Marie", true);
    assert(sess.ok);
    const auto sid = sess.value.id;

    // addFile 1 → OK.
    {
        std::string data(30, 'X');
        std::istringstream in(data);
        const auto r = svc.addFile(sid, "a.bin", data.size(), in);
        assert(r.ok);
        assert(r.value.size == 30);
        assert(!r.value.sha256.empty());
        assert(std::filesystem::exists(r.value.storedPath));
    }
    // addFile 2 → OK (cumul 60 mais limite est 50). Doit échouer size_limit.
    {
        std::string data(30, 'Y');
        std::istringstream in(data);
        const auto r = svc.addFile(sid, "b.bin", data.size(), in);
        assert(!r.ok);
        assert(r.reason == "size_limit");
    }
    // addFile petit (20) → OK, cumul 50 = limite. Encore OK car '> limit'.
    {
        std::string data(20, 'Z');
        std::istringstream in(data);
        const auto r = svc.addFile(sid, "c.bin", data.size(), in);
        assert(r.ok);
    }
    // 3e fichier → files_limit (maxFiles=2 a déjà été dépassé en pratique
    // après l'OK ci-dessus puisqu'on a 2 fichiers et limit=2).
    {
        std::string data(1, 'Q');
        std::istringstream in(data);
        const auto r = svc.addFile(sid, "d.bin", data.size(), in);
        assert(!r.ok);
        assert(r.reason == "files_limit");
    }

    // finalize → reçu + history + event.
    const auto fr = svc.finalize(sid);
    assert(fr.ok);
    assert(!fr.value.id.empty());
    assert(fr.value.totalBytes == 50);
    assert(receipts.verify(fr.value));

    auto events = bus.drain();
    bool sawEvent = false;
    for (const auto& ev : events) {
        std::visit([&](auto const& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, DepositReceivedEvent>) {
                if (e.depositSessionId == sid) sawEvent = true;
            }
        }, ev);
    }
    assert(sawEvent);
    assert(history.snapshot().size() == 1);

    // finalize idempotent (2nd appel doit échouer not_found).
    const auto fr2 = svc.finalize(sid);
    assert(!fr2.ok);

    std::printf("test_deposit_session_service OK\n");
    return 0;
}
