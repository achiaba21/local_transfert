#include "ltr/infra/retention_service.hpp"

#include <chrono>
#include <system_error>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

RetentionService::RetentionService(PolicyEnforcementService& policy,
                                   std::filesystem::path downloadDir)
    : policy_(policy), downloadDir_(std::move(downloadDir)) {}

int RetentionService::purgeHistories(PeersHistory& peers,
                                     std::int64_t nowEpochSec) {
    const auto days = policy_.historyRetentionDays();
    if (days <= 0) return 0;
    const auto cutoff = nowEpochSec - static_cast<std::int64_t>(days) * 86400;

    int removed = 0;
    // PeersHistory : snapshot + forget pour les entrées trop anciennes.
    for (const auto& e : peers.snapshot()) {
        if (e.lastSeen > 0 && e.lastSeen < cutoff) {
            peers.forget(e.deviceId);
            ++removed;
        }
    }
    if (removed > 0) {
        core::log_info("[retention] purge histories : "
                       + std::to_string(removed) + " entrée(s) (cutoff="
                       + std::to_string(cutoff) + ")");
    }
    return removed;
}

int RetentionService::purgeReceivedFiles(std::int64_t nowEpochSec) {
    const auto days = policy_.receivedFilesRetentionDays();
    if (days <= 0) return 0;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(downloadDir_, ec)) return 0;

    const auto cutoff = nowEpochSec - static_cast<std::int64_t>(days) * 86400;
    int removed = 0;

    const auto skipDir = downloadDir_ / "Deposits" / ".receipts";

    for (auto it = fs::recursive_directory_iterator(
            downloadDir_, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const auto& path = it->path();
        // Skip le dossier des reçus (conservés pour audit).
        const auto rel = fs::relative(path, downloadDir_, ec);
        if (!ec) {
            const auto sRel = rel.string();
            if (sRel.find("Deposits/.receipts") != std::string::npos) continue;
        }
        std::error_code statEc;
        if (!fs::is_regular_file(path, statEc)) continue;
        const auto ftime = fs::last_write_time(path, statEc);
        if (statEc) continue;
        // C++17 : pas de std::chrono::file_clock. On approxime en
        // décalant via "now" sur les deux horloges.
        const auto fsNow  = fs::file_time_type::clock::now();
        const auto sysNow = std::chrono::system_clock::now();
        const auto sysTime = sysNow + (ftime - fsNow);
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            sysTime.time_since_epoch()).count();
        if (secs < cutoff) {
            std::error_code rmEc;
            fs::remove(path, rmEc);
            if (!rmEc) ++removed;
        }
    }
    if (removed > 0) {
        core::log_info("[retention] purge files : "
                       + std::to_string(removed)
                       + " fichier(s) (cutoff=" + std::to_string(cutoff) + ")");
    }
    return removed;
}

} // namespace ltr::infra
