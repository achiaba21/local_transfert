#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

namespace ltr::ui {

// Sprint UI Layout System : détection DPI/Retina + scaling factor.
//
// Sur macOS Retina : scale = 2.0 typiquement (point = 2 pixels).
// Sur Windows HiDPI : 1.25/1.5/2.0 selon DPI utilisateur.
// Sur Linux/X11 : déduit de Xft.dpi ou fallback 1.0.
//
// Toutes les tailles UI (FontSize, Spacing, Radius) DOIVENT passer par
// `DpiScale::scaled(value)` pour s'adapter automatiquement.
class DpiScale {
public:
    // Détecte le scale factor à partir de la fenêtre (et de l'écran
    // qui la contient si possible). Appelé au lancement et à chaque
    // resize (au cas où la fenêtre change d'écran).
    static void detect(const sf::RenderWindow& win);

    // Force un scale (utile pour tests ou user override).
    static void setScale(float s);

    static float scale() { return scale_; }

    static unsigned scaled(unsigned px) {
        return static_cast<unsigned>(static_cast<float>(px) * scale_ + 0.5f);
    }
    static float scaled(float px) { return px * scale_; }

private:
    static float scale_;
};

} // namespace ltr::ui
