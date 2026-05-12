// Tests : DepositReceiptService — build, verify, tampering détecté,
// stabilité de la signature.

#include <cassert>
#include <cstdio>

#include "ltr/infra/deposit_link.hpp"
#include "ltr/infra/deposit_receipt.hpp"
#include "ltr/infra/deposit_session.hpp"

int main() {
    using namespace ltr::infra;

    DepositLink link;
    link.id    = "link-abc";
    link.label = "Pièces Dupont 2026";

    DepositSession s;
    s.id            = "session-xyz";
    s.linkId        = link.id;
    s.depositorName = "Marie Dupont";
    s.consentAccepted = true;
    s.startedAt     = 1000;
    DepositSessionFile f1;
    f1.name = "devis.pdf";
    f1.size = 1234;
    f1.sha256 = "deadbeef";
    s.files.push_back(f1);
    s.totalBytes = 1234;

    DepositReceiptService svc("secret-hmac");
    const auto r1 = svc.build(s, link, 2000);
    assert(!r1.id.empty());
    assert(r1.linkId == link.id);
    assert(r1.linkLabel == link.label);
    assert(r1.depositorName == "Marie Dupont");
    assert(r1.files.size() == 1);
    assert(r1.totalBytes == 1234);
    assert(!r1.signature.empty());

    // verify OK
    assert(svc.verify(r1));

    // tampering → verify KO
    DepositReceipt tampered = r1;
    tampered.depositorName = "Quelqu'un Autre";
    assert(!svc.verify(tampered));

    // tampering signature
    DepositReceipt tampered2 = r1;
    tampered2.signature[0] = (tampered2.signature[0] == 'a' ? 'b' : 'a');
    assert(!svc.verify(tampered2));

    // Signature stable : si on rebuild avec mêmes inputs (sauf id et
    // createdAt qui sont uniques), on doit obtenir une signature
    // déterministe pour les MÊMES champs canoniques. On force ici la
    // même id + createdAt pour vérifier l'invariant.
    DepositReceipt copy = r1;
    DepositReceiptService svc2("secret-hmac");
    assert(svc2.verify(copy));

    // Secret différent → verify KO.
    DepositReceiptService bad("other-secret");
    assert(!bad.verify(r1));

    // toJson contient bien le champ signature.
    const auto json = svc.toJson(r1);
    assert(json.find("\"signature\"") != std::string::npos);
    assert(json.find(r1.signature) != std::string::npos);

    std::printf("test_deposit_receipt_service OK\n");
    return 0;
}
