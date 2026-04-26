// Vérifie que QrCode::render produit une image valide.

#include "ltr/web/qr_code.hpp"

#include <cassert>
#include <iostream>

int main() {
    const auto img = ltr::web::QrCode::render(
        "http://192.168.1.42:45456", 320);

    const auto sz = img.getSize();
    // L'image doit être carrée et non vide.
    assert(sz.x > 0);
    assert(sz.y > 0);
    assert(sz.x == sz.y);

    // La taille doit être proche du pixelSize demandé (pas identique
    // car arrondie au plus proche multiple du nombre de modules).
    // Vérifie juste qu'elle est raisonnable : entre 100 et 600 px.
    assert(sz.x >= 100 && sz.x <= 600);

    std::cout << "test_qr_code OK — image " << sz.x << "x" << sz.y << "\n";
    return 0;
}
