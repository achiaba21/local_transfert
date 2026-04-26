#pragma once

#include <SFML/Graphics/Texture.hpp>

namespace ltr::ui {

// Catalogue d'icônes PNG embedded, lazy-loaded au 1er accès.
// Usage (thread-main uniquement, cohérent avec l'UI SFML) :
//     sf::Sprite s(IconLibrary::get(IconLibrary::Id::Check));
//     s.setColor(Colors::accent);  // tintage si besoin
//     s.setPosition(...);
//     target.draw(s);
class IconLibrary {
public:
    enum class Id {
        Check,
        Folder,
        Close,
        ArrowUp,
        ArrowDown,
        Radar,
        NoDevice,
        QrCode,
    };

    static const sf::Texture& get(Id id);
};

} // namespace ltr::ui
