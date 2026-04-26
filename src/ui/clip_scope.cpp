#include "ltr/ui/clip_scope.hpp"

#include <algorithm>

namespace ltr::ui {

ClipScope::ClipScope(sf::RenderTarget& target, const sf::FloatRect& rect)
    : target_(target), previousView_(target.getView()) {
    // Une `sf::View` SFML est définie par 2 choses :
    //   1. Le rect MONDE qu'elle observe (`setCenter` / `setSize`)
    //   2. Le viewport NORMALISÉ (0..1) qui définit où le rendu s'affiche
    //      sur la surface cible.
    //
    // Pour clipper au rect `rect` en coordonnées écran, on règle :
    //   - La view monde sur `rect` (1:1 mapping)
    //   - Le viewport sur la fraction de la surface correspondant à `rect`
    sf::View clipped(rect);

    const auto sz = target.getSize();
    const float w = static_cast<float>(sz.x);
    const float h = static_cast<float>(sz.y);
    if (w > 0.f && h > 0.f) {
        // Clamp pour éviter viewport hors [0,1].
        const float vx = std::clamp(rect.left   / w, 0.f, 1.f);
        const float vy = std::clamp(rect.top    / h, 0.f, 1.f);
        const float vw = std::clamp(rect.width  / w, 0.f, 1.f - vx);
        const float vh = std::clamp(rect.height / h, 0.f, 1.f - vy);
        clipped.setViewport({vx, vy, vw, vh});
    }
    target_.setView(clipped);
}

ClipScope::~ClipScope() {
    target_.setView(previousView_);
}

} // namespace ltr::ui
