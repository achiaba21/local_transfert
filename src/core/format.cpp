#include "ltr/core/format.hpp"

#include <iomanip>
#include <sstream>

namespace ltr::core {

std::string formatBytes(std::uint64_t n) {
    constexpr double k = 1024.0;
    const double b = static_cast<double>(n);
    std::ostringstream os;
    os << std::fixed << std::setprecision(1);
    if (b < k)                 os << b << " o";
    else if (b < k * k)        os << (b / k) << " Ko";
    else if (b < k * k * k)    os << (b / (k * k)) << " Mo";
    else                       os << (b / (k * k * k)) << " Go";
    return os.str();
}

std::string formatSpeed(double bytesPerSec) {
    return formatBytes(static_cast<std::uint64_t>(bytesPerSec)) + "/s";
}

std::string formatEta(std::chrono::seconds s) {
    const auto n = s.count();
    if (n <= 0)       return "-";
    if (n < 60)       return std::to_string(n) + " s";
    if (n < 3600)     return std::to_string(n / 60) + " min";
    return std::to_string(n / 3600) + " h";
}

} // namespace ltr::core
