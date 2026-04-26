#include "ltr/ui/widgets/scroll_area.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

#include <algorithm>

namespace ltr::ui {

namespace {
constexpr float kScrollbarThickness = 4.f;
constexpr float kScrollbarMargin    = 2.f;
constexpr float kWheelStep          = 32.f;
} // namespace

ScrollArea& ScrollArea::setBounds(const sf::FloatRect& r) {
    bounds_ = r;
    clamp();
    return *this;
}

ScrollArea& ScrollArea::setDirection(Direction d) {
    dir_ = d;
    return *this;
}

ScrollArea& ScrollArea::setContentSize(float w, float h) {
    contentW_ = w;
    contentH_ = h;
    clamp();
    return *this;
}

ScrollArea& ScrollArea::showScrollbar(bool b) {
    showScrollbar_ = b;
    return *this;
}

void ScrollArea::clamp() {
    const float maxX = std::max(0.f, contentW_ - bounds_.width);
    const float maxY = std::max(0.f, contentH_ - bounds_.height);
    if (scrollX_ < 0.f) scrollX_ = 0.f;
    if (scrollY_ < 0.f) scrollY_ = 0.f;
    if (scrollX_ > maxX) scrollX_ = maxX;
    if (scrollY_ > maxY) scrollY_ = maxY;
}

bool ScrollArea::handleEvent(const sf::Event& e) {
    if (e.type != sf::Event::MouseWheelScrolled) return false;
    const float mx = static_cast<float>(e.mouseWheelScroll.x);
    const float my = static_cast<float>(e.mouseWheelScroll.y);
    if (!bounds_.contains(mx, my)) return false;

    const float delta = e.mouseWheelScroll.delta * kWheelStep;
    const bool isHorizontalWheel =
        (e.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel);

    if (dir_ == Direction::Vertical) {
        scrollY_ -= delta;
    } else if (dir_ == Direction::Horizontal) {
        // Sur axe horizontal pur, on accepte la molette verticale aussi
        // (l'utilisateur peut ne pas avoir de molette horizontale).
        scrollX_ -= delta;
    } else {
        // Both
        if (isHorizontalWheel) scrollX_ -= delta;
        else                    scrollY_ -= delta;
    }
    clamp();
    return true;
}

void ScrollArea::draw(sf::RenderTarget& target) const {
    if (!showScrollbar_) return;

    // Vertical scrollbar (à droite)
    if ((dir_ == Direction::Vertical || dir_ == Direction::Both)
        && contentH_ > bounds_.height) {
        const float trackX = bounds_.left + bounds_.width
                              - kScrollbarThickness - kScrollbarMargin;
        const float trackY = bounds_.top + kScrollbarMargin;
        const float trackH = bounds_.height - 2 * kScrollbarMargin;

        RoundedRect track(trackX, trackY, kScrollbarThickness, trackH,
                          kScrollbarThickness / 2.f);
        track.setFillColor(Colors::separator).draw(target);

        const float thumbH = std::max(
            30.f, trackH * (bounds_.height / contentH_));
        const float maxScrollY = std::max(1.f, contentH_ - bounds_.height);
        const float thumbY = trackY +
            (trackH - thumbH) * (scrollY_ / maxScrollY);
        RoundedRect thumb(trackX, thumbY, kScrollbarThickness, thumbH,
                          kScrollbarThickness / 2.f);
        thumb.setFillColor(Colors::textSecondary).draw(target);
    }

    // Horizontal scrollbar (en bas)
    if ((dir_ == Direction::Horizontal || dir_ == Direction::Both)
        && contentW_ > bounds_.width) {
        const float trackX = bounds_.left + kScrollbarMargin;
        const float trackY = bounds_.top + bounds_.height
                              - kScrollbarThickness - kScrollbarMargin;
        const float trackW = bounds_.width - 2 * kScrollbarMargin;

        RoundedRect track(trackX, trackY, trackW, kScrollbarThickness,
                          kScrollbarThickness / 2.f);
        track.setFillColor(Colors::separator).draw(target);

        const float thumbW = std::max(
            30.f, trackW * (bounds_.width / contentW_));
        const float maxScrollX = std::max(1.f, contentW_ - bounds_.width);
        const float thumbX = trackX +
            (trackW - thumbW) * (scrollX_ / maxScrollX);
        RoundedRect thumb(thumbX, trackY, thumbW, kScrollbarThickness,
                          kScrollbarThickness / 2.f);
        thumb.setFillColor(Colors::textSecondary).draw(target);
    }
}

} // namespace ltr::ui
