#include "ltr/ui/widgets/qr_code_view.hpp"

#include <SFML/Graphics/Sprite.hpp>

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

namespace ltr::ui {

QrCodeView& QrCodeView::setImage(const sf::Image& img) {
    if (texture_.loadFromImage(img)) {
        texture_.setSmooth(false);
        ready_ = true;
    }
    return *this;
}

void QrCodeView::draw(sf::RenderTarget& t) const {
    // Fond blanc arrondi + bordure fine (cohérent Theme).
    RoundedRect bg(bounds_.left, bounds_.top,
                   bounds_.width, bounds_.height, Radius::md);
    bg.setFillColor(sf::Color::White)
      .setOutline(Colors::separator, 1.f);
    bg.draw(t);

    if (!ready_) return;

    // Dessine le sprite centré, scaled à la plus petite dimension moins padding.
    const float pad = 6.f;
    const float side = std::min(bounds_.width, bounds_.height) - 2 * pad;
    const auto texSize = texture_.getSize();
    if (texSize.x == 0 || texSize.y == 0) return;

    sf::Sprite sp(texture_);
    const float scale = side / static_cast<float>(texSize.x);
    sp.setScale(scale, scale);
    sp.setPosition(bounds_.left + (bounds_.width - side) / 2.f,
                   bounds_.top  + (bounds_.height - side) / 2.f);
    t.draw(sp);
}

} // namespace ltr::ui
