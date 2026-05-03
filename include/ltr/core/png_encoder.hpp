#pragma once

#include <cstdint>
#include <vector>

namespace ltr::core {

// V1.5 — Sprint Hardening & Polish
// Encodeur PNG minimal réutilisable : prend RGBA 8-bit top-down et
// retourne les octets PNG (IHDR + IDAT zlib via miniz + IEND).
//
// Extrait de clipboard_paste_win.cpp pour réutilisation. Pas de
// dépendance externe en plus (miniz est déjà linké côté ltr_core
// pour le streaming zip).
//
// Retourne un vector vide en cas d'échec (compression miniz fail).
std::vector<std::uint8_t> encodePng(std::uint32_t width,
                                    std::uint32_t height,
                                    const std::vector<std::uint8_t>& rgba);

} // namespace ltr::core
