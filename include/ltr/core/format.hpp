#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace ltr::core {

// Humanisations utilisées par l'UI. Regroupées ici pour éviter la
// duplication entre écrans et widgets.

std::string formatBytes(std::uint64_t n);
std::string formatSpeed(double bytesPerSec);
std::string formatEta(std::chrono::seconds s);

} // namespace ltr::core
