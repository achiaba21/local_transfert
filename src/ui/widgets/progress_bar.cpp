#include "ltr/ui/widgets/progress_bar.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

#include <algorithm>

namespace ltr::ui {

ProgressBar& ProgressBar::setBounds(const sf::FloatRect& r) { bounds_ = r; return *this; }
ProgressBar& ProgressBar::setProgress(double p)             { progress_ = std::clamp(p, 0.0, 1.0); return *this; }
ProgressBar& ProgressBar::setColor(sf::Color c)             { color_ = c; return *this; }

void ProgressBar::draw(sf::RenderTarget& target) const {
    const float r = bounds_.height / 2.f;

    RoundedRect bg(bounds_.left, bounds_.top, bounds_.width, bounds_.height, r);
    bg.setFillColor(Colors::separator);
    bg.draw(target);

    if (progress_ > 0.0) {
        const float w = static_cast<float>(bounds_.width * progress_);
        // Largeur minimale pour que le rayon reste cohérent.
        const float wSafe = std::max(w, 2.f * r);
        RoundedRect fg(bounds_.left, bounds_.top, wSafe, bounds_.height, r);
        fg.setFillColor(color_);
        fg.draw(target);
    }
}

} // namespace ltr::ui
