#include "ltr/infra/deposit_link.hpp"

#include <algorithm>
#include <cctype>

namespace ltr::infra {

bool depositLinkIsActive(const DepositLink& link, std::int64_t nowEpochSec) {
    if (link.revoked) return false;
    if (link.expiresAt != 0 && nowEpochSec >= link.expiresAt) return false;
    return true;
}

std::string depositLinkShortId(const DepositLink& link) {
    return link.id.substr(0, std::min<std::size_t>(8, link.id.size()));
}

std::string sanitizeForFilesystem(const std::string& input,
                                  std::size_t maxLen) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        const auto u = static_cast<unsigned char>(c);
        const bool reserved = (c == '/' || c == '\\' || c == ':' ||
                               c == '*' || c == '?' || c == '"' ||
                               c == '<' || c == '>' || c == '|' ||
                               c == '\0');
        if (reserved || u < 0x20) {
            if (!out.empty() && out.back() != '_') out.push_back('_');
        } else {
            out.push_back(c);
        }
    }
    // Trim spaces et dots aux extrémités (Windows-friendly).
    while (!out.empty() && (out.front() == ' ' || out.front() == '.')) {
        out.erase(out.begin());
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
        out.pop_back();
    }
    if (out.empty()) out = "untitled";
    if (out.size() > maxLen) out.resize(maxLen);
    return out;
}

} // namespace ltr::infra
