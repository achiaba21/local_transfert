#pragma once

#include <string>

namespace ltr::domain {

enum class TransferStatus {
    Pending,
    WaitingAcceptance,
    Accepted,
    InProgress,
    Done,
    Rejected,
    Failed,
    Cancelled,
    Proposed,   // V1.1: desktop → web, SSE envoyé, attend clic visiteur
    Expired,    // V1.1: ticket expiré sans clic visiteur
};

inline const char* toString(TransferStatus s) noexcept {
    switch (s) {
        case TransferStatus::Pending:           return "Pending";
        case TransferStatus::WaitingAcceptance: return "WaitingAcceptance";
        case TransferStatus::Accepted:          return "Accepted";
        case TransferStatus::InProgress:        return "InProgress";
        case TransferStatus::Done:              return "Done";
        case TransferStatus::Rejected:          return "Rejected";
        case TransferStatus::Failed:            return "Failed";
        case TransferStatus::Cancelled:         return "Cancelled";
        case TransferStatus::Proposed:          return "Proposed";
        case TransferStatus::Expired:           return "Expired";
    }
    return "Unknown";
}

} // namespace ltr::domain
