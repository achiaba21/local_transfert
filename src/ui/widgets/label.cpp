#include "ltr/ui/widgets/label.hpp"

#include "ltr/ui/theme.hpp"
#include "ltr/ui/utf8.hpp"

#include <SFML/Graphics/Text.hpp>

namespace ltr::ui {

Label::Label() = default;

Label& Label::setText(const std::string& s)   { text_  = s; return *this; }
Label& Label::setPosition(float x, float y)   { pos_   = {x, y}; return *this; }
Label& Label::setSize(unsigned pts)           { size_  = pts; return *this; }
Label& Label::setColor(sf::Color c)           { color_ = c; return *this; }
Label& Label::setBold(bool b)                 { bold_  = b; return *this; }

sf::Vector2f Label::measure() const {
    sf::Text t;
    t.setFont(theme_font());
    t.setString(utf8(text_));
    t.setCharacterSize(size_);
    if (bold_) t.setStyle(sf::Text::Bold);
    const auto b = t.getLocalBounds();
    return {b.width, b.height};
}

void Label::draw(sf::RenderTarget& target) const {
    sf::Text t;
    t.setFont(theme_font());
    t.setString(utf8(text_));
    t.setCharacterSize(size_);
    t.setFillColor(color_);
    if (bold_) t.setStyle(sf::Text::Bold);
    t.setPosition(pos_);
    target.draw(t);
}

} // namespace ltr::ui
