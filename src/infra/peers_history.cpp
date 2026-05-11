#include "ltr/infra/peers_history.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

// Rétention des pairs offline : 30 jours en secondes (décision BA Q5).
constexpr std::int64_t kRetentionSec = 30LL * 24 * 3600;

std::int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace

PeersHistory::PeersHistory(std::filesystem::path path)
    : path_(std::move(path)) {}

void PeersHistory::load() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path_, ec)) {
        core::log_info("[peers_history] aucun fichier existant");
        return;
    }
    std::ifstream ifs(path_);
    if (!ifs) {
        core::log_warn("[peers_history] open failed: " + path_.string());
        return;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();

    try {
        const auto j = nlohmann::json::parse(ss.str());
        const auto& peers = j.value("peers", nlohmann::json::object());
        std::lock_guard<std::mutex> lk(mu_);
        entries_.clear();
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            const auto& v = it.value();
            if (!v.is_object()) continue;
            Entry e;
            e.deviceId       = it.key();
            e.name           = v.value("name", "");
            e.platform       = v.value("platform", "");
            e.kind           = v.value("kind", "native");
            e.fingerprint    = v.value("fingerprint", "");
            e.firstSeen      = v.value("firstSeen", std::int64_t{0});
            e.lastSeen       = v.value("lastSeen",  std::int64_t{0});
            e.totalTransfers = v.value("totalTransfers", std::int64_t{0});
            e.totalBytes     = v.value("totalBytes", std::uint64_t{0});
            entries_.emplace(e.deviceId, std::move(e));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("[peers_history] parse fail: ") + e.what());
        return;
    }

    // Purge auto au boot.
    const auto cutoff = nowSec() - kRetentionSec;
    purgeOlderThan(cutoff);
    core::log_info("[peers_history] chargé " + std::to_string(entries_.size())
                   + " pair(s) (purge > 30j appliquée)");
}

void PeersHistory::touch(const std::string& deviceId,
                          const std::string& name,
                          const std::string& platform,
                          const std::string& kind,
                          const std::string& fingerprint) {
    if (deviceId.empty()) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = entries_.find(deviceId);
        const auto t = nowSec();
        if (it == entries_.end()) {
            Entry e;
            e.deviceId    = deviceId;
            e.name        = name;
            e.platform    = platform;
            e.kind        = kind;
            e.fingerprint = fingerprint;
            e.firstSeen   = t;
            e.lastSeen    = t;
            entries_.emplace(deviceId, std::move(e));
        } else {
            it->second.name        = name;
            it->second.platform    = platform;
            it->second.kind        = kind;
            if (!fingerprint.empty()) it->second.fingerprint = fingerprint;
            it->second.lastSeen    = t;
        }
        saveLocked();
    }
}

void PeersHistory::recordTransfer(const std::string& deviceId,
                                   std::uint64_t bytes) {
    if (deviceId.empty()) return;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = entries_.find(deviceId);
    if (it == entries_.end()) return;
    it->second.totalTransfers += 1;
    it->second.totalBytes     += bytes;
    saveLocked();
}

std::optional<PeersHistory::Entry>
PeersHistory::get(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = entries_.find(deviceId);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

std::vector<PeersHistory::Entry> PeersHistory::snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Entry> out;
    out.reserve(entries_.size());
    for (const auto& [k, v] : entries_) out.push_back(v);
    return out;
}

std::vector<PeersHistory::Entry>
PeersHistory::snapshotOffline(std::int64_t minOfflineSec,
                              const std::vector<std::string>& excludeIds) const {
    std::lock_guard<std::mutex> lk(mu_);
    const auto t = nowSec();
    std::vector<Entry> out;
    for (const auto& [k, v] : entries_) {
        if (t - v.lastSeen < minOfflineSec) continue;
        if (std::find(excludeIds.begin(), excludeIds.end(), k)
            != excludeIds.end()) continue;
        out.push_back(v);
    }
    std::sort(out.begin(), out.end(),
              [](const Entry& a, const Entry& b) {
                  return a.lastSeen > b.lastSeen;
              });
    return out;
}

void PeersHistory::forget(const std::string& deviceId) {
    std::lock_guard<std::mutex> lk(mu_);
    if (entries_.erase(deviceId) > 0) {
        core::log_info("[peers_history] forget " + deviceId.substr(0, 8) + "...");
        saveLocked();
    }
}

std::size_t PeersHistory::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return entries_.size();
}

void PeersHistory::save() const {
    std::lock_guard<std::mutex> lk(mu_);
    saveLocked();
}

void PeersHistory::saveLocked() const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);

    nlohmann::json peers = nlohmann::json::object();
    for (const auto& [k, e] : entries_) {
        peers[k] = {
            {"name",           e.name},
            {"platform",       e.platform},
            {"kind",           e.kind},
            {"fingerprint",    e.fingerprint},
            {"firstSeen",      e.firstSeen},
            {"lastSeen",       e.lastSeen},
            {"totalTransfers", e.totalTransfers},
            {"totalBytes",     e.totalBytes},
        };
    }
    nlohmann::json root;
    root["peers"] = peers;

    const auto tmp = path_.string() + ".tmp";
    std::ofstream ofs(tmp, std::ios::trunc);
    if (!ofs) return;
    ofs << root.dump(2);
    ofs.close();
    fs::rename(tmp, path_, ec);
}

void PeersHistory::purgeOlderThan(std::int64_t cutoffEpoch) {
    // Appelé sous lock dans load().
    std::size_t before = entries_.size();
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        if (it->second.lastSeen < cutoffEpoch) it = entries_.erase(it);
        else ++it;
    }
    if (entries_.size() != before) {
        saveLocked();
    }
}

}  // namespace ltr::infra
