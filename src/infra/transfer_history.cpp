#include "ltr/infra/transfer_history.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

constexpr std::size_t kMaxEntries  = 1000;
// Auto-purge entries finished > 6 mois (~180 jours) au load.
constexpr std::int64_t kRetentionSec = 180LL * 24 * 3600;

std::int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace

const char* TransferHistory::kindToStr(Kind k) {
    switch (k) {
        case Kind::TcpOut:   return "tcp-out";
        case Kind::TcpIn:    return "tcp-in";
        case Kind::HttpUp:   return "http-up";
        case Kind::HttpDown: return "http-down";
    }
    return "unknown";
}

TransferHistory::Kind TransferHistory::kindFromStr(const std::string& s) {
    if (s == "tcp-in")    return Kind::TcpIn;
    if (s == "http-up")   return Kind::HttpUp;
    if (s == "http-down") return Kind::HttpDown;
    return Kind::TcpOut;  // default
}

const char* TransferHistory::statusToStr(Status s) {
    switch (s) {
        case Status::Pending:   return "pending";
        case Status::Ok:        return "ok";
        case Status::Failed:    return "failed";
        case Status::Cancelled: return "cancelled";
    }
    return "unknown";
}

TransferHistory::Status TransferHistory::statusFromStr(const std::string& s) {
    if (s == "ok")        return Status::Ok;
    if (s == "failed")    return Status::Failed;
    if (s == "cancelled") return Status::Cancelled;
    return Status::Pending;
}

TransferHistory::TransferHistory(std::filesystem::path path)
    : path_(std::move(path)) {}

void TransferHistory::load() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path_, ec)) {
        core::log_info("[transfer_history] aucun fichier existant");
        return;
    }
    std::ifstream ifs(path_);
    if (!ifs) {
        core::log_warn("[transfer_history] open failed: " + path_.string());
        return;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();

    try {
        const auto j = nlohmann::json::parse(ss.str());
        const auto& transfers = j.value("transfers", nlohmann::json::array());
        std::lock_guard<std::mutex> lk(mu_);
        entries_.clear();
        for (const auto& v : transfers) {
            if (!v.is_object()) continue;
            Entry e;
            e.sessionId    = v.value("sessionId", "");
            e.peerDeviceId = v.value("peerDeviceId", "");
            e.peerName     = v.value("peerName", "");
            e.kind         = kindFromStr(v.value("kind", "tcp-out"));
            e.fileCount    = v.value("fileCount", 0);
            e.totalBytes   = v.value("totalBytes", std::uint64_t{0});
            e.status       = statusFromStr(v.value("status", "pending"));
            e.startedAt    = v.value("startedAt", std::int64_t{0});
            e.finishedAt   = v.value("finishedAt", std::int64_t{0});
            e.error        = v.value("error", "");
            entries_.push_back(std::move(e));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("[transfer_history] parse fail: ") + e.what());
        return;
    }

    purgeOlderThan(nowSec() - kRetentionSec);
    enforceCap();
    core::log_info("[transfer_history] chargé "
                   + std::to_string(entries_.size()) + " entrée(s)");
}

void TransferHistory::insert(const Entry& entry) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.sessionId == entry.sessionId; });
    if (it == entries_.end()) {
        entries_.push_back(entry);
        if (entries_.back().startedAt == 0) {
            entries_.back().startedAt = nowSec();
        }
        enforceCap();
    } else {
        // Met à jour métadonnées si déjà connu (cas resume same sid).
        it->peerDeviceId = entry.peerDeviceId;
        it->peerName     = entry.peerName;
        it->kind         = entry.kind;
        if (entry.fileCount  > 0) it->fileCount  = entry.fileCount;
        if (entry.totalBytes > 0) it->totalBytes = entry.totalBytes;
    }
    saveLocked();
}

void TransferHistory::updateProgress(const std::string& sessionId,
                                      std::uint64_t totalBytes) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.sessionId == sessionId; });
    if (it == entries_.end()) return;
    if (totalBytes > it->totalBytes) it->totalBytes = totalBytes;
    // Pas de save sur progress (trop fréquent) — save uniquement à markDone.
}

void TransferHistory::markDone(const std::string& sessionId,
                                Status status,
                                std::uint64_t totalBytes,
                                const std::string& error) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.sessionId == sessionId; });
    if (it == entries_.end()) return;
    it->status     = status;
    it->finishedAt = nowSec();
    if (totalBytes > it->totalBytes) it->totalBytes = totalBytes;
    if (!error.empty()) it->error = error;
    saveLocked();
}

std::optional<TransferHistory::Entry>
TransferHistory::get(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.sessionId == sessionId; });
    if (it == entries_.end()) return std::nullopt;
    return *it;
}

std::vector<TransferHistory::Entry> TransferHistory::snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Entry> out = entries_;
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        const auto ta = a.finishedAt > 0 ? a.finishedAt : a.startedAt;
        const auto tb = b.finishedAt > 0 ? b.finishedAt : b.startedAt;
        return ta > tb;
    });
    return out;
}

std::size_t TransferHistory::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return entries_.size();
}

void TransferHistory::save() const {
    std::lock_guard<std::mutex> lk(mu_);
    saveLocked();
}

void TransferHistory::saveLocked() const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries_) {
        arr.push_back({
            {"sessionId",    e.sessionId},
            {"peerDeviceId", e.peerDeviceId},
            {"peerName",     e.peerName},
            {"kind",         kindToStr(e.kind)},
            {"fileCount",    e.fileCount},
            {"totalBytes",   e.totalBytes},
            {"status",       statusToStr(e.status)},
            {"startedAt",    e.startedAt},
            {"finishedAt",   e.finishedAt},
            {"error",        e.error},
        });
    }
    nlohmann::json root;
    root["transfers"] = arr;

    const auto tmp = path_.string() + ".tmp";
    std::ofstream ofs(tmp, std::ios::trunc);
    if (!ofs) return;
    ofs << root.dump(2);
    ofs.close();
    fs::rename(tmp, path_, ec);
}

void TransferHistory::enforceCap() {
    // Appelé sous lock.
    if (entries_.size() <= kMaxEntries) return;
    const auto drop = entries_.size() - kMaxEntries;
    entries_.erase(entries_.begin(), entries_.begin() + drop);
}

void TransferHistory::purgeOlderThan(std::int64_t cutoffEpoch) {
    // Appelé sous lock dans load().
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [cutoffEpoch](const Entry& e) {
                const auto t = e.finishedAt > 0 ? e.finishedAt : e.startedAt;
                return t > 0 && t < cutoffEpoch;
            }),
        entries_.end());
}

}  // namespace ltr::infra
