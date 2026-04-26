#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/View.hpp>

namespace ltr::ui {

// RAII : limite le dessin à un rect. À la construction, push une
// sf::View qui clip le rendu au rect spécifié (en coordonnées écran).
// À la destruction, restore la view précédente.
//
// Sprint UI Layout System : appliqué autour de chaque zone (header,
// sidebar, centre, share, bottom transferts, modale inbox) pour
// garantir qu'aucun dessin ne déborde.
//
// Usage :
//   {
//       ClipScope clip(target, sidebarRect_);
//       drawSidebar(target);   // tout dessin clippé à sidebarRect_
//   }   // view restaurée auto
class ClipScope {
public:
    ClipScope(sf::RenderTarget& target, const sf::FloatRect& rect);
    ~ClipScope();

    ClipScope(const ClipScope&)            = delete;
    ClipScope& operator=(const ClipScope&) = delete;

private:
    sf::RenderTarget& target_;
    sf::View          previousView_;
};

} // namespace ltr::ui
