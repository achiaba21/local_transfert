#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "ltr/infra/deposit_session.hpp"

namespace ltr::infra {

class DepositSessionRepository {
public:
    virtual ~DepositSessionRepository() = default;
    virtual std::vector<DepositSession> loadAll() const = 0;
    virtual std::optional<DepositSession> findById(
        const std::string& id) const = 0;
    virtual void save(const DepositSession& session) = 0;
    virtual void remove(const std::string& id) = 0;
};

// Persistance JSON dans `cfgDir/deposit_sessions.json`.
class JsonDepositSessionRepository final : public DepositSessionRepository {
public:
    explicit JsonDepositSessionRepository(std::filesystem::path path);

    std::vector<DepositSession> loadAll() const override;
    std::optional<DepositSession> findById(
        const std::string& id) const override;
    void save(const DepositSession& session) override;
    void remove(const std::string& id) override;

private:
    std::vector<DepositSession> readAllLocked() const;
    void writeAllLocked(const std::vector<DepositSession>& sessions) const;

    std::filesystem::path path_;
};

} // namespace ltr::infra
