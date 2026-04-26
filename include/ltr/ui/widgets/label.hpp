#pragma once

#include <string>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

namespace ltr::ui {

class Label {
public:
    Label();

    Label& setText(const std::string& s);
    Label& setPosition(float x, float y);
    Label& setSize(unsigned pts);
    Label& setColor(sf::Color c);
    Label& setBold(bool b);

    sf::Vector2f measure() const;

    void draw(sf::RenderTarget& t) const;

private:
    std::string text_;
    sf::Vector2f pos_{0, 0};
    unsigned size_{13};
    sf::Color color_{0, 0, 0};
    bool bold_{false};
};

} // namespace ltr::ui
