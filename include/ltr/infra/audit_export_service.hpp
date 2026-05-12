#pragma once

#include <string>
#include <vector>

#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/transfer_history.hpp"

namespace ltr::infra {

class AuditExportService {
public:
    // Phase 1 — signatures inchangées (rétrocompatibilité tests).
    std::string exportJson(
        const std::vector<TransferHistory::Entry>& entries) const;
    std::string exportCsv(
        const std::vector<TransferHistory::Entry>& entries) const;

    // Phase 2 — surcharges agrégeant transferts + dépôts dans un même export.
    std::string exportJson(
        const std::vector<TransferHistory::Entry>& transfers,
        const std::vector<DepositHistory::Entry>& deposits) const;
    std::string exportCsv(
        const std::vector<TransferHistory::Entry>& transfers,
        const std::vector<DepositHistory::Entry>& deposits) const;
};

std::string transferDirectionForKind(TransferHistory::Kind kind);

} // namespace ltr::infra
