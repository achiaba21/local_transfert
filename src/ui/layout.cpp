#include "ltr/ui/layout.hpp"

#include <algorithm>

namespace ltr::ui {

Box& Box::spacing(float s) { spacing_ = s; return *this; }

Box& Box::padding(float p) {
    padH_ = padV_ = p;
    return *this;
}

Box& Box::padding(float horizontal, float vertical) {
    padH_ = horizontal;
    padV_ = vertical;
    return *this;
}

Box& Box::fixed(float size, DrawFn draw) {
    Child c;
    c.fixed     = true;
    c.fixedSize = size;
    c.draw      = std::move(draw);
    children_.push_back(std::move(c));
    return *this;
}

Box& Box::expanded(int weight, DrawFn draw) {
    Child c;
    c.fixed  = false;
    c.weight = std::max(1, weight);
    c.draw   = std::move(draw);
    children_.push_back(std::move(c));
    return *this;
}

Box& Box::spacer(float size) {
    Child c;
    c.fixed     = true;
    c.fixedSize = size;
    c.draw      = nullptr;
    children_.push_back(std::move(c));
    return *this;
}

std::vector<sf::FloatRect> Box::computeBounds(
    const sf::FloatRect& parent) const {

    const sf::FloatRect inner{
        parent.left + padH_,
        parent.top  + padV_,
        std::max(0.f, parent.width  - 2 * padH_),
        std::max(0.f, parent.height - 2 * padV_)
    };

    const bool isH = (direction_ == Direction::Horizontal);

    // 1. Somme des tailles fixées + total des poids des expanded.
    float fixedTotal = 0.f;
    int   weightTotal = 0;
    for (const auto& c : children_) {
        if (c.fixed) fixedTotal += c.fixedSize;
        else         weightTotal += c.weight;
    }

    // 2. Espace dispo pour les expanded.
    const float gaps = spacing_ * std::max<int>(
        0, static_cast<int>(children_.size()) - 1);
    const float axisLen = isH ? inner.width : inner.height;
    const float available = std::max(0.f, axisLen - fixedTotal - gaps);
    const float perWeight = (weightTotal > 0)
                            ? available / static_cast<float>(weightTotal)
                            : 0.f;

    // 3. Placement séquentiel.
    std::vector<sf::FloatRect> rects;
    rects.reserve(children_.size());
    float cursor = isH ? inner.left : inner.top;
    for (const auto& c : children_) {
        const float size = c.fixed ? c.fixedSize : (perWeight * c.weight);
        if (isH) {
            rects.push_back({cursor, inner.top, size, inner.height});
        } else {
            rects.push_back({inner.left, cursor, inner.width, size});
        }
        cursor += size + spacing_;
    }
    return rects;
}

void Box::layout(const sf::FloatRect& parent, sf::RenderTarget& target) {
    const auto bounds = computeBounds(parent);
    for (std::size_t i = 0; i < children_.size(); ++i) {
        if (children_[i].draw) {
            children_[i].draw(target, bounds[i]);
        }
    }
}

} // namespace ltr::ui
