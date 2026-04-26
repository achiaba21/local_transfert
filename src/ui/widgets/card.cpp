#include "ltr/ui/widgets/card.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

namespace ltr::ui {

void Card::draw(sf::RenderTarget& target) const {
    if (radius_ > 0.f) {
        RoundedRect rr(bounds_.left, bounds_.top,
                       bounds_.width, bounds_.height, radius_);
        rr.setFillColor(color_);
        if (hasBorder_) rr.setOutline(border_, 1.f);
        if (shadow_)    rr.setShadow(Colors::shadow, 4.f);
        rr.draw(target);
        return;
    }

    sf::RectangleShape r({bounds_.width, bounds_.height});
    r.setPosition(bounds_.left, bounds_.top);
    r.setFillColor(color_);
    if (hasBorder_) {
        r.setOutlineColor(border_);
        r.setOutlineThickness(1.f);
    }
    target.draw(r);
}

} // namespace ltr::ui
