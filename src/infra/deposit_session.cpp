#include "ltr/infra/deposit_session.hpp"

#include <array>
#include <cstdio>
#include <random>

namespace ltr::infra {

const char* depositSessionStatusToStr(DepositSession::Status status) {
    switch (status) {
        case DepositSession::Status::Open:      return "open";
        case DepositSession::Status::Finalized: return "finalized";
        case DepositSession::Status::Cancelled: return "cancelled";
        case DepositSession::Status::Failed:    return "failed";
    }
    return "open";
}

DepositSession::Status
depositSessionStatusFromStr(const std::string& value) {
    if (value == "finalized") return DepositSession::Status::Finalized;
    if (value == "cancelled") return DepositSession::Status::Cancelled;
    if (value == "failed")    return DepositSession::Status::Failed;
    return DepositSession::Status::Open;
}

std::string makeDepositSessionId() {
    std::random_device rd;
    std::array<std::uint32_t, 4> seedWords{rd(), rd(), rd(), rd()};
    std::seed_seq seq(seedWords.begin(), seedWords.end());
    std::mt19937 rng(seq);
    char buf[17] = {0};
    std::snprintf(buf, sizeof(buf), "%08x%08x",
                  static_cast<unsigned>(rng()),
                  static_cast<unsigned>(rng()));
    return std::string(buf);
}

} // namespace ltr::infra
