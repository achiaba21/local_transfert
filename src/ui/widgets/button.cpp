#include "ltr/ui/widgets/button.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/utf8.hpp"

#include <SFML/Graphics/Text.hpp>

#include <cmath>

namespace ltr::ui {

Button::Button() = default;

Button& Button::setBounds(const sf::FloatRect& r)          { bounds_  = r; return *this; }
Button& Button::setLabel(const std::string& s)             { label_   = s; return *this; }
Button& Button::setVariant(Variant v)                      { variant_ = v; return *this; }
Button& Button::setEnabled(bool b)                         { enabled_ = b; return *this; }
Button& Button::onClick(std::function<void()> cb)          { cb_ = std::move(cb); return *this; }

void Button::handleEvent(const sf::Event& e) {
    if (!enabled_) { hover_ = pressed_ = false; return; }

    if (e.type == sf::Event::MouseMoved) {
        hover_ = bounds_.contains(
            static_cast<float>(e.mouseMove.x),
            static_cast<float>(e.mouseMove.y));
    }
    else if (e.type == sf::Event::MouseButtonPressed &&
             e.mouseButton.button == sf::Mouse::Left) {
        if (bounds_.contains(
                static_cast<float>(e.mouseButton.x),
                static_cast<float>(e.mouseButton.y))) {
            pressed_ = true;
        }
    }
    else if (e.type == sf::Event::MouseButtonReleased &&
             e.mouseButton.button == sf::Mouse::Left) {
        const bool inside = bounds_.contains(
            static_cast<float>(e.mouseButton.x),
            static_cast<float>(e.mouseButton.y));
        if (pressed_ && inside && cb_) cb_();
        pressed_ = false;
    }
}

void Button::draw(sf::RenderTarget& target) const {
    sf::Color fill, text;
    bool withShadow = false;

    switch (variant_) {
        case Variant::Primary:
            fill = hover_ ? Colors::accentHover : Colors::accent;
            text = sf::Color::White;
            withShadow = true;
            break;
        case Variant::Danger:
            fill = Colors::error;
            text = sf::Color::White;
            withShadow = true;
            break;
        case Variant::Secondary:
        default:
            fill = hover_ ? sf::Color(229, 231, 235) : sf::Color(243, 244, 246);
            text = Colors::text;
            break;
    }

    if (!enabled_) {
        fill = sf::Color(229, 231, 235);
        text = Colors::textSecondary;
        withShadow = false;
    }
    if (pressed_ && enabled_) {
        fill.r = static_cast<std::uint8_t>(fill.r * 0.88f);
        fill.g = static_cast<std::uint8_t>(fill.g * 0.88f);
        fill.b = static_cast<std::uint8_t>(fill.b * 0.88f);
    }

    RoundedRect rr(bounds_.left, bounds_.top,
                   bounds_.width, bounds_.height,
                   /*radius*/ 10.f);
    rr.setFillColor(fill);
    if (withShadow) rr.setShadow(sf::Color(0, 0, 0, 25), 3.f);
    if (variant_ == Variant::Secondary && !pressed_) {
        rr.setOutline(Colors::separator, 1.f);
    }
    rr.draw(target);

    sf::Text t;
    // V1.6.5+ : Inter Bold dédié pour les boutons (vraie graisse).
    t.setFont(theme_font_bold());
    t.setString(utf8(label_));
    t.setCharacterSize(FontSize::button);
    t.setFillColor(text);
    const auto b = t.getLocalBounds();
    const float x = bounds_.left + (bounds_.width  - b.width)  / 2.f - b.left;
    const float y = bounds_.top  + (bounds_.height - b.height) / 2.f - b.top;
    t.setPosition(std::round(x), std::round(y));
    target.draw(t);
}

} // namespace ltr::ui
