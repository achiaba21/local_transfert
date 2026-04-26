#include "ltr/web/qr_code.hpp"

#include <algorithm>

#include <qrcodegen.hpp>

namespace ltr::web::QrCode {

sf::Image render(const std::string& text,
                 unsigned pixelSize,
                 int quietZoneModules) {
    using qrcodegen::QrCode;

    const auto qr = QrCode::encodeText(text.c_str(), QrCode::Ecc::MEDIUM);
    const int modules = qr.getSize();
    const int totalModules = modules + 2 * quietZoneModules;

    // Taille d'un module en pixels (arrondi au plus proche, au moins 1).
    unsigned moduleSize = pixelSize / static_cast<unsigned>(totalModules);
    if (moduleSize == 0) moduleSize = 1;

    const unsigned imgSize = moduleSize * static_cast<unsigned>(totalModules);

    sf::Image img;
    img.create(imgSize, imgSize, sf::Color::White);

    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (!qr.getModule(x, y)) continue;
            const unsigned px = (static_cast<unsigned>(x + quietZoneModules))
                              * moduleSize;
            const unsigned py = (static_cast<unsigned>(y + quietZoneModules))
                              * moduleSize;
            for (unsigned dy = 0; dy < moduleSize; ++dy) {
                for (unsigned dx = 0; dx < moduleSize; ++dx) {
                    img.setPixel(px + dx, py + dy, sf::Color::Black);
                }
            }
        }
    }
    return img;
}

} // namespace ltr::web::QrCode
