#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>

namespace ltr::ui {

class ProgressBar {
public:
    ProgressBar() = default;

    ProgressBar& setBounds(const sf::FloatRect& r);
    ProgressBar& setProgress(double p);   // [0, 1]
    ProgressBar& setColor(sf::Color c);

    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    double progress_{0.0};
    sf::Color color_{79, 70, 229};
};

} // namespace ltr::ui
