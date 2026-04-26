#include "ltr/ui/dpi.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/ui/label_cache.hpp"

#include <SFML/Window/VideoMode.hpp>

namespace ltr::ui {

float DpiScale::scale_ = 1.0f;

void DpiScale::detect(const sf::RenderWindow& win) {
    // Heuristique cross-platform : on compare la taille de la fenêtre en
    // pixels (ce que SFML voit pour le rendu) avec sa taille en
    // "points" supposés via le desktop mode. Sur macOS Retina, SFML
    // expose la taille pixels (2× points), donc cette heuristique
    // donne ~2.0.
    //
    // Sur backends qui ne distinguent pas points/pixels (Linux X11
    // standard), on retombe sur 1.0.
    float scale = 1.0f;
    const auto winSize = win.getSize();
    const auto desktop = sf::VideoMode::getDesktopMode();

    // Si la fenêtre rapporte une taille pixels supérieure aux dimensions
    // typiques desktop (>2000 px sur un écran 1440), c'est probablement
    // du Retina. Sinon 1×.
    // Heuristique simple — on pourrait améliorer en V2 avec API natives.
    if (winSize.x > 0 && desktop.width > 0 &&
        desktop.width >= 2560 && winSize.x > 2000) {
        scale = 2.0f;
    }

    if (scale != scale_) {
        core::log_info("[dpi] scale changed " + std::to_string(scale_)
                       + " → " + std::to_string(scale));
        scale_ = scale;
        // Invalider le cache de mesure car les tailles ont changé.
        LabelCache::clear();
    }
}

void DpiScale::setScale(float s) {
    if (s != scale_) {
        scale_ = s;
        LabelCache::clear();
    }
}

} // namespace ltr::ui
