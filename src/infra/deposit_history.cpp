#include "ltr/infra/deposit_history.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

const char* DepositHistory::statusToStr(Status s) {
    switch (s) {
        case Status::Finalized: return "finalized";
        case Status::Cancelled: return "cancelled";
        case Status::Failed:    return "failed";
    }
    return "finalized";
}

DepositHistory::Status
DepositHistory::statusFromStr(const std::string& v) {
    if (v == "cancelled") return Status::Cancelled;
    if (v == "failed")    return Status::Failed;
    return Status::Finalized;
}

namespace {

nlohmann::json entryToJson(const DepositHistory::Entry& e) {
    return {
        {"receiptId",       e.receiptId},
        {"sessionId",       e.sessionId},
        {"linkId",          e.linkId},
        {"linkLabel",       e.linkLabel},
        {"depositorName",   e.depositorName},
        {"fileCount",       e.fileCount},
        {"totalBytes",      e.totalBytes},
        {"consentAccepted", e.consentAccepted},
        {"status",          DepositHistory::statusToStr(e.status)},
        {"startedAt",       e.startedAt},
        {"finishedAt",      e.finishedAt},
    };
}

DepositHistory::Entry entryFromJson(const nlohmann::json& j) {
    DepositHistory::Entry e;
    e.receiptId       = j.value("receiptId", std::string{});
    e.sessionId       = j.value("sessionId", std::string{});
    e.linkId          = j.value("linkId", std::string{});
    e.linkLabel       = j.value("linkLabel", std::string{});
    e.depositorName   = j.value("depositorName", std::string{});
    e.fileCount       = j.value("fileCount", 0);
    e.totalBytes      = j.value("totalBytes", std::uint64_t{0});
    e.consentAccepted = j.value("consentAccepted", false);
    e.status          = DepositHistory::statusFromStr(
        j.value("status", std::string{"finalized"}));
    e.startedAt       = j.value("startedAt", std::int64_t{0});
    e.finishedAt      = j.value("finishedAt", std::int64_t{0});
    return e;
}

} // namespace

JsonDepositHistoryRepository::JsonDepositHistoryRepository(
        std::filesystem::path path)
    : path_(std::move(path)) {}

std::vector<DepositHistory::Entry>
JsonDepositHistoryRepository::loadAll() const {
    std::vector<DepositHistory::Entry> out;
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) return out;

    try {
        std::ifstream in(path_);
        const auto j = nlohmann::json::parse(in);
        const auto arr = j.value("deposits", nlohmann::json::array());
        out.reserve(arr.size());
        for (const auto& item : arr) {
            if (item.is_object()) out.push_back(entryFromJson(item));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("[deposit_history] parse failed: ")
                       + e.what());
    }
    return out;
}

void JsonDepositHistoryRepository::saveAll(
        const std::vector<DepositHistory::Entry>& entries) const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries) arr.push_back(entryToJson(e));
    nlohmann::json root;
    root["deposits"] = arr;

    const auto tmp = path_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            core::log_error("[deposit_history] cannot write " + tmp);
            return;
        }
        out << root.dump(2);
    }
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        core::log_error("[deposit_history] rename failed: " + ec.message());
    }
}

DepositHistoryStore::DepositHistoryStore(DepositHistoryRepository& repository)
    : repository_(repository) {}

void DepositHistoryStore::load() {
    std::lock_guard<std::mutex> lock(mu_);
    if (loaded_) return;
    entries_ = repository_.loadAll();
    enforceCapLocked();
    loaded_ = true;
}

void DepositHistoryStore::enforceCapLocked() {
    if (entries_.size() <= DepositHistory::MAX_ENTRIES) return;
    const auto drop = entries_.size() - DepositHistory::MAX_ENTRIES;
    entries_.erase(entries_.begin(), entries_.begin() + drop);
}

void DepositHistoryStore::append(const DepositHistory::Entry& entry) {
    std::vector<DepositHistory::Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!loaded_) {
            entries_ = repository_.loadAll();
            loaded_ = true;
        }
        entries_.push_back(entry);
        enforceCapLocked();
        snapshot = entries_;
    }
    repository_.saveAll(snapshot);
}

std::vector<DepositHistory::Entry> DepositHistoryStore::snapshot() const {
    std::vector<DepositHistory::Entry> copy;
    {
        std::lock_guard<std::mutex> lock(mu_);
        copy = entries_;
    }
    std::sort(copy.begin(), copy.end(),
        [](const DepositHistory::Entry& a, const DepositHistory::Entry& b) {
            return a.finishedAt > b.finishedAt;
        });
    return copy;
}

std::vector<DepositHistory::Entry>
DepositHistoryStore::filterByLinkId(const std::string& linkId) const {
    std::vector<DepositHistory::Entry> out;
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : entries_) {
        if (e.linkId == linkId) out.push_back(e);
    }
    std::sort(out.begin(), out.end(),
        [](const DepositHistory::Entry& a, const DepositHistory::Entry& b) {
            return a.finishedAt > b.finishedAt;
        });
    return out;
}

std::size_t DepositHistoryStore::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return entries_.size();
}

} // namespace ltr::infra
