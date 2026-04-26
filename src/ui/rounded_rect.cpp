#include "ltr/ui/rounded_rect.hpp"

#include <cmath>

namespace ltr::ui {

namespace {

constexpr std::size_t kCornerPoints = 8;
constexpr float kTwoPi = 6.28318530718f;

// Remplit `shape` avec 4 × kCornerPoints sommets décrivant un rectangle
// de taille (w,h) aux coins arrondis de rayon r.
void buildRounded(sf::ConvexShape& shape,
                  float w, float h, float r) {
    const std::size_t n = kCornerPoints;
    shape.setPointCount(4 * n);

    // Centres des 4 arcs (TL, TR, BR, BL).
    const sf::Vector2f centers[4] = {
        {r,     r    },        // top-left
        {w - r, r    },        // top-right
        {w - r, h - r},        // bottom-right
        {r,     h - r},        // bottom-left
    };
    // Angle de départ pour chaque coin (sens trigo inversé).
    const float starts[4] = {
        kTwoPi * 0.50f,   // TL : 180°
        kTwoPi * 0.75f,   // TR : 270°
        kTwoPi * 0.00f,   // BR : 0°
        kTwoPi * 0.25f,   // BL : 90°
    };

    std::size_t idx = 0;
    for (int c = 0; c < 4; ++c) {
        for (std::size_t i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) /
                            static_cast<float>(n - 1);
            const float a = starts[c] + t * (kTwoPi / 4.f);
            const float x = centers[c].x + r * std::cos(a);
            const float y = centers[c].y + r * std::sin(a);
            shape.setPoint(idx++, {x, y});
        }
    }
}

} // namespace

RoundedRect::RoundedRect(float x, float y, float w, float h, float radius)
    : x_(x), y_(y), w_(w), h_(h), radius_(radius) {
    rebuild();
}

RoundedRect& RoundedRect::setFillColor(sf::Color c)      { fill_ = c; shape_.setFillColor(c); return *this; }
RoundedRect& RoundedRect::setRadius(float r)             { radius_ = r; rebuild(); return *this; }

RoundedRect& RoundedRect::setOutline(sf::Color c, float t) {
    outline_ = c; outlineThick_ = t;
    shape_.setOutlineColor(c);
    shape_.setOutlineThickness(t);
    return *this;
}

RoundedRect& RoundedRect::setShadow(sf::Color c, float offsetY) {
    shadow_ = c; shadowOffset_ = offsetY;
    shadowShape_.setFillColor(c);
    return *this;
}

void RoundedRect::rebuild() {
    const float r = std::min(radius_, std::min(w_, h_) / 2.f);

    buildRounded(shape_, w_, h_, r);
    shape_.setPosition(x_, y_);
    shape_.setFillColor(fill_);
    shape_.setOutlineColor(outline_);
    shape_.setOutlineThickness(outlineThick_);

    buildRounded(shadowShape_, w_, h_, r);
    shadowShape_.setFillColor(shadow_);
}

void RoundedRect::draw(sf::RenderTarget& t) const {
    if (shadow_.a > 0) {
        auto s = shadowShape_;
        s.setPosition(x_, y_ + shadowOffset_);
        t.draw(s);
    }
    t.draw(shape_);
}

} // namespace ltr::ui
