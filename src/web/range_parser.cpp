#include "ltr/web/range_parser.hpp"

namespace ltr::web {

RangeInfo parseRangeHeader(const std::string& header, std::uint64_t totalSize) {
    RangeInfo r;
    if (header.empty()) return r;
    constexpr const char* kPrefix = "bytes=";
    if (header.compare(0, 6, kPrefix) != 0) return r;
    const auto spec = header.substr(6);
    const auto dash = spec.find('-');
    if (dash == std::string::npos) return r;
    try {
        r.start = static_cast<std::uint64_t>(std::stoull(spec.substr(0, dash)));
        const auto rest = spec.substr(dash + 1);
        if (rest.empty()) {
            r.end = (totalSize == 0) ? 0 : (totalSize - 1);
        } else {
            r.end = static_cast<std::uint64_t>(std::stoull(rest));
        }
    } catch (...) {
        return r;
    }
    if (r.start > r.end || r.end >= totalSize) return r;
    r.valid = true;
    return r;
}

} // namespace ltr::web
