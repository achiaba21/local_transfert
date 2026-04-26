#pragma once

#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

namespace ltr::ui {

// Rectangle à coins arrondis dessiné via sf::ConvexShape (aucune texture).
// Utilisé partout à la place de `sf::RectangleShape` pour un rendu moderne.
class RoundedRect {
public:
    RoundedRect(float x, float y, float w, float h, float radius = 8.f);

    RoundedRect& setFillColor(sf::Color c);
    RoundedRect& setOutline(sf::Color c, float thickness);
    RoundedRect& setRadius(float r);
    RoundedRect& setShadow(sf::Color c, float offsetY = 2.f);

    void draw(sf::RenderTarget& t) const;

private:
    void rebuild();

    float  x_, y_, w_, h_;
    float  radius_{8.f};
    sf::Color fill_{sf::Color::White};
    sf::Color outline_{sf::Color::Transparent};
    float  outlineThick_{0.f};
    sf::Color shadow_{0, 0, 0, 0};
    float  shadowOffset_{0.f};

    sf::ConvexShape shape_;
    sf::ConvexShape shadowShape_;
};

} // namespace ltr::ui
