#pragma once

#include <string>

#include <SFML/Graphics/Image.hpp>

namespace ltr::web {

// Wrapper mince autour de qrcodegen produisant une sf::Image.
namespace QrCode {

// Rend le texte en QR code sur une image SFML de `pixelSize` × `pixelSize`.
// La correction d'erreur est ECC::MEDIUM (compromis densité/résilience).
// `quietZoneModules` = modules blancs en bordure (standard = 4).
sf::Image render(const std::string& text,
                 unsigned pixelSize = 320,
                 int quietZoneModules = 4);

} // namespace QrCode

} // namespace ltr::web
