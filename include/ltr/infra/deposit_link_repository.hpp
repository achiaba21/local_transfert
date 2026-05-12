#pragma once

#include <filesystem>
#include <vector>

#include "ltr/infra/deposit_link.hpp"

namespace ltr::infra {

// DIP — DepositLinkService dépend de cette interface, pas d'un fichier.
class DepositLinkRepository {
public:
    virtual ~DepositLinkRepository() = default;
    virtual std::vector<DepositLink> loadAll() const = 0;
    virtual void save(const DepositLink& link) = 0;
    virtual void remove(const std::string& id) = 0;
};

// Persistance JSON dans `cfgDir/deposit_links.json`.
// Écriture atomique (.tmp + rename), comme JsonQuotaRepository.
class JsonDepositLinkRepository final : public DepositLinkRepository {
public:
    explicit JsonDepositLinkRepository(std::filesystem::path path);

    std::vector<DepositLink> loadAll() const override;
    void save(const DepositLink& link) override;
    void remove(const std::string& id) override;

private:
    std::vector<DepositLink> readAllLocked() const;
    void writeAllLocked(const std::vector<DepositLink>& links) const;

    std::filesystem::path path_;
};

} // namespace ltr::infra
