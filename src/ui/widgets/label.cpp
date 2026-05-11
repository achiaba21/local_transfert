#include "ltr/ui/widgets/label.hpp"

#include "ltr/ui/label_cache.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/utf8.hpp"

#include <SFML/Graphics/Text.hpp>

namespace ltr::ui {

namespace {

// V1.6.5+ : sélectionne la police selon bold (vraie Inter Bold au lieu
// du fake-bold synthétique SFML).
const sf::Font& fontFor(bool bold) {
    return bold ? theme_font_bold() : theme_font();
}

// Mesure brute via SFML — caché dans LabelCache pour éviter la re-créa.
sf::Vector2f measureRaw(const std::string& text, unsigned size, bool bold) {
    return LabelCache::getOrCompute(
        LabelCache::Key{text, size, bold},
        [&]() -> sf::Vector2f {
            sf::Text t;
            t.setFont(fontFor(bold));
            t.setString(utf8(text));
            t.setCharacterSize(size);
            const auto b = t.getLocalBounds();
            return {b.width, b.height};
        });
}

constexpr const char* kEllipsis = "\xE2\x80\xA6";  // U+2026 …

} // namespace

Label::Label() = default;

Label& Label::setText(const std::string& s)   { text_  = s; return *this; }
Label& Label::setPosition(float x, float y)   { pos_   = {x, y};
                                                 hasBounds_ = false; return *this; }
Label& Label::setSize(unsigned pts)           { size_  = pts; return *this; }
Label& Label::setColor(sf::Color c)           { color_ = c; return *this; }
Label& Label::setBold(bool b)                 { bold_  = b; return *this; }

Label& Label::setMaxWidth(float w)            { maxWidth_ = w; return *this; }
Label& Label::setEllipsis(bool on)            { ellipsis_ = on; return *this; }
Label& Label::setAlignment(Alignment a)       { align_ = a; return *this; }

Label& Label::setBounds(const sf::FloatRect& r) {
    bounds_ = r;
    hasBounds_ = true;
    if (maxWidth_ <= 0.f) maxWidth_ = r.width;
    return *this;
}

sf::Vector2f Label::measure() const {
    return measureRaw(text_, size_, bold_);
}

std::string Label::renderedText() const {
    if (maxWidth_ <= 0.f || !ellipsis_ || text_.empty()) return text_;
    const auto natural = measureRaw(text_, size_, bold_);
    if (natural.x <= maxWidth_) return text_;

    const float ellipsisW = measureRaw(kEllipsis, size_, bold_).x;
    if (ellipsisW > maxWidth_) {
        // maxWidth trop petit même pour "…" → pas d'ellipsis utile,
        // tronquer brutalement à 1 char.
        return text_.substr(0, 1);
    }

    // Binary search sur la longueur du substring + " …".
    int lo = 0;
    int hi = static_cast<int>(text_.size());
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        const auto w = measureRaw(text_.substr(0, mid), size_, bold_).x
                       + ellipsisW;
        if (w <= maxWidth_) lo = mid;
        else                hi = mid - 1;
    }
    return text_.substr(0, lo) + kEllipsis;
}

sf::Vector2f Label::measureClamped() const {
    return measureRaw(renderedText(), size_, bold_);
}

void Label::draw(sf::RenderTarget& target) const {
    const auto rendered = renderedText();
    if (rendered.empty()) return;

    sf::Text t;
    t.setFont(fontFor(bold_));   // V1.6.5+ : Inter Bold dédiée si bold_
    t.setString(utf8(rendered));
    t.setCharacterSize(size_);
    t.setFillColor(color_);

    sf::Vector2f position = pos_;
    if (hasBounds_) {
        const auto m = measureRaw(rendered, size_, bold_);
        switch (align_) {
            case Alignment::Left:
                position = {bounds_.left, bounds_.top};
                break;
            case Alignment::Center:
                position = {bounds_.left + (bounds_.width - m.x) / 2.f,
                            bounds_.top  + (bounds_.height - m.y) / 2.f};
                break;
            case Alignment::Right:
                position = {bounds_.left + bounds_.width - m.x,
                            bounds_.top};
                break;
        }
    }
    t.setPosition(position);
    target.draw(t);
}

} // namespace ltr::ui
