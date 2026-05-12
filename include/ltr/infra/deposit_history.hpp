#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace ltr::infra {

// Historique persistant des dépôts reçus côté host. Schéma distinct de
// TransferHistory pour préserver le SRP. Agrégé par AuditExportService.
class DepositHistory {
public:
    enum class Status : std::uint8_t {
        Finalized,
        Cancelled,
        Failed,
    };

    struct Entry {
        std::string   receiptId;
        std::string   sessionId;
        std::string   linkId;
        std::string   linkLabel;
        std::string   depositorName;
        int           fileCount{0};
        std::uint64_t totalBytes{0};
        bool          consentAccepted{false};
        Status        status{Status::Finalized};
        std::int64_t  startedAt{0};
        std::int64_t  finishedAt{0};
    };

    static const char* statusToStr(Status s);
    static Status      statusFromStr(const std::string& v);

    static constexpr std::size_t MAX_ENTRIES = 2000;
};

// Interface DIP : DepositHistory dépend d'un Repository.
class DepositHistoryRepository {
public:
    virtual ~DepositHistoryRepository() = default;
    virtual std::vector<DepositHistory::Entry> loadAll() const = 0;
    virtual void saveAll(
        const std::vector<DepositHistory::Entry>& entries) const = 0;
};

class JsonDepositHistoryRepository final : public DepositHistoryRepository {
public:
    explicit JsonDepositHistoryRepository(std::filesystem::path path);

    std::vector<DepositHistory::Entry> loadAll() const override;
    void saveAll(
        const std::vector<DepositHistory::Entry>& entries) const override;

private:
    std::filesystem::path path_;
};

// Store thread-safe avec cap MAX_ENTRIES.
class DepositHistoryStore {
public:
    explicit DepositHistoryStore(DepositHistoryRepository& repository);

    // Charge depuis le repository. Idempotent.
    void load();

    void append(const DepositHistory::Entry& entry);

    // Snapshot trié par finishedAt DESC.
    std::vector<DepositHistory::Entry> snapshot() const;

    // Snapshot filtré par linkId.
    std::vector<DepositHistory::Entry> filterByLinkId(
        const std::string& linkId) const;

    std::size_t size() const;

private:
    void enforceCapLocked();

    DepositHistoryRepository& repository_;
    mutable std::mutex        mu_;
    std::vector<DepositHistory::Entry> entries_;
    bool                      loaded_{false};
};

} // namespace ltr::infra
