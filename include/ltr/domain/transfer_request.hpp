#pragma once

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "ltr/domain/file_meta.hpp"

namespace ltr::domain {

struct TransferRequest {
    std::string sessionId;     // UUID v4
    std::string senderId;      // Device id
    std::string senderName;
    std::string pinCode;       // 4-6 digits
    std::vector<FileMeta> files;

    std::uint64_t totalSize() const noexcept {
        std::uint64_t sum = 0;
        for (const auto& f : files) sum += f.size;
        return sum;
    }
};

} // namespace ltr::domain
