#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>

namespace ltr::ui {

// Panneau rectangulaire simple avec couleur de fond. Utilisé comme fond pour
// les sections (sidebar, offre entrante, etc.).
class Card {
public:
    Card() = default;

    Card& setBounds(const sf::FloatRect& r)   { bounds_ = r; return *this; }
    Card& setColor(sf::Color c)               { color_  = c; return *this; }
    Card& setBorderColor(sf::Color c)         { border_ = c; hasBorder_ = true; return *this; }
    Card& setRadius(float r)                  { radius_ = r; return *this; }
    Card& setShadow(bool b)                   { shadow_ = b; return *this; }

    const sf::FloatRect& bounds() const noexcept { return bounds_; }

    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    sf::Color color_{255, 255, 255};
    sf::Color border_{0, 0, 0, 0};
    bool hasBorder_{false};
    float radius_{0.f};
    bool shadow_{false};
};

} // namespace ltr::ui
