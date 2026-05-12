// Tests RetentionService : purge fichiers anciens + no-op si days=0.

#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <thread>

#include "ltr/infra/business_policy.hpp"
#include "ltr/infra/peers_history.hpp"
#include "ltr/infra/policy_enforcement.hpp"
#include "ltr/infra/retention_service.hpp"

namespace fs = std::filesystem;

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

fs::path makeTempDir() {
    return fs::temp_directory_path()
        / ("ltr-test-retention-"
           + std::to_string(std::chrono::system_clock::now()
               .time_since_epoch().count()));
}

void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// Force la mtime à un instant donné (epochSec).
// C++17 : pas de clock_cast, on décale via "now" sur les deux horloges.
void touchPast(const fs::path& p, std::int64_t epochSec) {
    const auto sysTp = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(epochSec));
    const auto fsNow  = fs::file_time_type::clock::now();
    const auto sysNow = std::chrono::system_clock::now();
    const auto fileTp = fsNow + (sysTp - sysNow);
    fs::last_write_time(p, fileTp);
}

std::int64_t nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

int main() {
    using namespace ltr::infra;

    // Setup : dossier temp avec 2 fichiers, l'un vieux, l'autre récent.
    const auto dir = makeTempDir();
    fs::create_directories(dir);
    const auto oldFile = dir / "old.txt";
    const auto newFile = dir / "new.txt";
    writeFile(oldFile, "old");
    writeFile(newFile, "new");
    const auto now = nowEpoch();
    touchPast(oldFile, now - 86400 * 60);   // 60 jours
    touchPast(newFile, now - 86400 * 5);    // 5 jours

    // Cas 1 : receivedFilesDays=0 → no-op.
    {
        BusinessPolicy p;
        p.retention.receivedFilesDays = 0;
        MemPolicyRepo repo(p);
        PolicyService ps(repo);
        PolicyEnforcementService pef(ps);
        RetentionService rs(pef, dir);
        assert(rs.purgeReceivedFiles(now) == 0);
        assert(fs::exists(oldFile));
        assert(fs::exists(newFile));
    }

    // Cas 2 : receivedFilesDays=30 → supprime old.txt.
    {
        BusinessPolicy p;
        p.retention.receivedFilesDays = 30;
        MemPolicyRepo repo(p);
        PolicyService ps(repo);
        PolicyEnforcementService pef(ps);
        RetentionService rs(pef, dir);
        assert(rs.purgeReceivedFiles(now) == 1);
        assert(!fs::exists(oldFile));
        assert(fs::exists(newFile));
    }

    // Cas 3 : skip Deposits/.receipts/.
    {
        writeFile(oldFile, "back");
        touchPast(oldFile, now - 86400 * 60);
        const auto receiptOld = dir / "Deposits" / ".receipts" / "old.json";
        writeFile(receiptOld, "{\"id\":\"x\"}");
        touchPast(receiptOld, now - 86400 * 60);

        BusinessPolicy p;
        p.retention.receivedFilesDays = 30;
        MemPolicyRepo repo(p);
        PolicyService ps(repo);
        PolicyEnforcementService pef(ps);
        RetentionService rs(pef, dir);
        const auto removed = rs.purgeReceivedFiles(now);
        // oldFile supprimé mais receipt préservé.
        assert(!fs::exists(oldFile));
        assert(fs::exists(receiptOld));
        assert(removed >= 1);
    }

    // Cas 4 : purgeHistories sur PeersHistory.
    {
        const auto histPath = dir / "peers_history.json";
        fs::remove(histPath);
        PeersHistory ph(histPath);
        ph.load();
        ph.touch("dev-A", "Alice", "macOS", "native", "");
        ph.touch("dev-B", "Bob",   "Windows", "native", "");
        assert(ph.size() == 2);

        BusinessPolicy p;
        p.retention.historyDays = 7;
        MemPolicyRepo repo(p);
        PolicyService ps(repo);
        PolicyEnforcementService pef(ps);
        RetentionService rs(pef, dir);
        // Aucune entrée n'est assez vieille → 0 supprimé.
        assert(rs.purgeHistories(ph, now) == 0);
    }

    // Cleanup.
    std::error_code ec;
    fs::remove_all(dir, ec);

    std::printf("test_retention_service OK\n");
    return 0;
}
