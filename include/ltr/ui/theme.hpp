#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>

namespace ltr::ui {

// Palette (option B — desktop clair, accent indigo, plus contrasté).
struct Colors {
    static const sf::Color bg;            // #FAFAFB — fond général
    static const sf::Color surface;       // #FFFFFF — cards, boutons secondaires
    static const sf::Color sidebar;       // #F1F3F6 — fond sidebar
    static const sf::Color accent;        // #6366F1 — indigo moderne
    static const sf::Color accentHover;   // #4F46E5
    static const sf::Color accentLight;   // #EEF2FF — bg selected
    static const sf::Color text;          // #0F172A
    static const sf::Color textSecondary; // #64748B
    static const sf::Color separator;     // #E2E8F0
    static const sf::Color success;       // #10B981
    static const sf::Color error;         // #EF4444
    static const sf::Color warning;       // #F59E0B
    static const sf::Color overlay;       // 0,0,0,140
    static const sf::Color shadow;        // 0,0,0,25
};

// Tokens d'espacement (grille 4 px).
namespace Spacing {
    constexpr float xs  = 4.f;
    constexpr float sm  = 8.f;
    constexpr float md  = 12.f;
    constexpr float lg  = 16.f;
    constexpr float xl  = 24.f;
    constexpr float xxl = 32.f;
    constexpr float xxxl= 48.f;
}

// Tailles de police (plus grandes pour un rendu moderne).
namespace FontSize {
    constexpr unsigned h1      = 24;
    constexpr unsigned h2      = 16;
    constexpr unsigned body    = 14;
    constexpr unsigned small   = 12;
    constexpr unsigned overline= 11; // tout en capitales
    constexpr unsigned button  = 14;
    constexpr unsigned pin     = 56;
}

// Rayons de coins arrondis.
namespace Radius {
    constexpr float sm   = 6.f;
    constexpr float md   = 10.f;
    constexpr float lg   = 14.f;
    constexpr float pill = 999.f;
}

// Chargement paresseux de la police (depuis assets ou système).
// V1.6.6+ : Geist Regular copiée dans le dossier de build ; fallback
// Inter puis système si elle manque.
const sf::Font& theme_font();

// V1.6.6+ : Geist Bold (vraie variante grasse, pas le fake-bold SFML
// synthétique). Les widgets qui appellent setBold(true) chargent cette police
// séparée pour un rendu plus net.
const sf::Font& theme_font_bold();

} // namespace ltr::ui
