// Tests : DepositHistoryStore — append, snapshot, filter, cap MAX_ENTRIES.

#include <cassert>
#include <cstdio>
#include <filesystem>

#include "ltr/infra/deposit_history.hpp"

namespace {

std::filesystem::path tempJsonPath() {
    return std::filesystem::temp_directory_path() / "ltr-test-deposit-history.json";
}

ltr::infra::DepositHistory::Entry makeEntry(const std::string& linkId,
                                             const std::string& receiptId,
                                             std::int64_t finishedAt) {
    ltr::infra::DepositHistory::Entry e;
    e.receiptId       = receiptId;
    e.sessionId       = "session-" + receiptId;
    e.linkId          = linkId;
    e.linkLabel       = "Lien " + linkId;
    e.depositorName   = "Quelqu'un";
    e.fileCount       = 1;
    e.totalBytes      = 100;
    e.consentAccepted = true;
    e.status          = ltr::infra::DepositHistory::Status::Finalized;
    e.startedAt       = finishedAt - 10;
    e.finishedAt      = finishedAt;
    return e;
}

} // namespace

int main() {
    using namespace ltr::infra;

    const auto path = tempJsonPath();
    std::filesystem::remove(path);

    JsonDepositHistoryRepository repo(path);
    DepositHistoryStore store(repo);
    store.load();
    assert(store.size() == 0);

    store.append(makeEntry("A", "r1", 100));
    store.append(makeEntry("B", "r2", 200));
    store.append(makeEntry("A", "r3", 150));

    assert(store.size() == 3);

    const auto snap = store.snapshot();
    assert(snap.size() == 3);
    // Trié par finishedAt DESC.
    assert(snap[0].receiptId == "r2");
    assert(snap[1].receiptId == "r3");
    assert(snap[2].receiptId == "r1");

    const auto byLink = store.filterByLinkId("A");
    assert(byLink.size() == 2);
    assert(byLink[0].receiptId == "r3");
    assert(byLink[1].receiptId == "r1");

    // Persistance : nouveau store sur même repo.
    DepositHistoryStore store2(repo);
    store2.load();
    assert(store2.size() == 3);

    // Cap MAX_ENTRIES.
    JsonDepositHistoryRepository repo2(
        std::filesystem::temp_directory_path() / "ltr-test-deposit-history-cap.json");
    std::filesystem::remove(
        std::filesystem::temp_directory_path() / "ltr-test-deposit-history-cap.json");
    DepositHistoryStore store3(repo2);
    store3.load();
    for (std::size_t i = 0; i < DepositHistory::MAX_ENTRIES + 5; ++i) {
        store3.append(makeEntry("X", "r" + std::to_string(i),
                                static_cast<std::int64_t>(i)));
    }
    assert(store3.size() == DepositHistory::MAX_ENTRIES);

    std::printf("test_deposit_history OK\n");
    return 0;
}
