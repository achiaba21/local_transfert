#include "ltr/ui/widgets/deposit_link_card.hpp"

#include <SFML/Graphics/Text.hpp>

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

namespace ltr::ui {

namespace {

constexpr float kBtnHeight = 28.f;

bool contains(const sf::FloatRect& r, sf::Vector2f p) {
    return r.contains(p);
}

void drawButton(sf::RenderTarget& target, const sf::FloatRect& r,
                const std::string& label, sf::Color fg, sf::Color bg) {
    RoundedRect rect(r.left, r.top, r.width, r.height, Radius::sm);
    rect.setFillColor(bg);
    rect.draw(target);

    sf::Text txt;
    txt.setFont(theme_font());
    txt.setCharacterSize(FontSize::small);
    txt.setFillColor(fg);
    txt.setString(sf::String::fromUtf8(label.begin(), label.end()));
    const auto bounds = txt.getLocalBounds();
    txt.setPosition(
        r.left + (r.width - bounds.width) / 2.f - bounds.left,
        r.top  + (r.height - bounds.height) / 2.f - bounds.top - 1.f);
    target.draw(txt);
}

} // namespace

sf::FloatRect DepositLinkCard::copyBtnRect() const {
    const float w = 110.f;
    return {bounds_.left + Spacing::lg,
            bounds_.top  + bounds_.height - kBtnHeight - Spacing::md,
            w, kBtnHeight};
}

sf::FloatRect DepositLinkCard::qrBtnRect() const {
    const auto c = copyBtnRect();
    const float w = 60.f;
    return {c.left + c.width + Spacing::sm, c.top, w, kBtnHeight};
}

sf::FloatRect DepositLinkCard::revokeBtnRect() const {
    const float w = 100.f;
    return {bounds_.left + bounds_.width - w - Spacing::lg,
            bounds_.top  + bounds_.height - kBtnHeight - Spacing::md,
            w, kBtnHeight};
}

bool DepositLinkCard::handleClick(sf::Vector2f pos) {
    if (contains(copyBtnRect(), pos))   { if (onCopy_)   onCopy_();   return true; }
    if (contains(qrBtnRect(),   pos))   { if (onQr_)     onQr_();     return true; }
    if (contains(revokeBtnRect(), pos) && model_.active) {
        if (onRevoke_) onRevoke_();
        return true;
    }
    return false;
}

void DepositLinkCard::draw(sf::RenderTarget& target) const {
    // Fond card.
    RoundedRect cardBg(bounds_.left, bounds_.top, bounds_.width, bounds_.height,
                       Radius::md);
    cardBg.setFillColor(Colors::surface);
    cardBg.setOutline(Colors::separator, 1.f);
    cardBg.draw(target);

    sf::Text label;
    label.setFont(theme_font_bold());
    label.setCharacterSize(FontSize::h2);
    label.setFillColor(model_.active ? Colors::text : Colors::textSecondary);
    label.setString(sf::String::fromUtf8(
        model_.label.begin(), model_.label.end()));
    label.setPosition(bounds_.left + Spacing::lg,
                      bounds_.top  + Spacing::md);
    target.draw(label);

    // Badge statut (pill).
    sf::Text badgeTxt;
    badgeTxt.setFont(theme_font());
    badgeTxt.setCharacterSize(FontSize::small);
    badgeTxt.setFillColor(sf::Color::White);
    badgeTxt.setString(sf::String::fromUtf8(
        model_.statusBadge.begin(), model_.statusBadge.end()));
    const auto bb = badgeTxt.getLocalBounds();
    const float badgeW = bb.width + Spacing::md * 2.f;
    const float badgeX = bounds_.left + bounds_.width - badgeW - Spacing::lg;
    const float badgeY = bounds_.top + Spacing::md + 2.f;
    RoundedRect badge(badgeX, badgeY, badgeW, 22.f, Radius::pill);
    badge.setFillColor(model_.badgeColor);
    badge.draw(target);
    badgeTxt.setPosition(badgeX + Spacing::md - bb.left,
                         badgeY + 3.f - bb.top);
    target.draw(badgeTxt);

    // Sub-ligne.
    sf::Text sub;
    sub.setFont(theme_font());
    sub.setCharacterSize(FontSize::small);
    sub.setFillColor(Colors::textSecondary);
    sub.setString(sf::String::fromUtf8(
        model_.subline.begin(), model_.subline.end()));
    sub.setPosition(bounds_.left + Spacing::lg,
                    bounds_.top  + Spacing::md + 30.f);
    target.draw(sub);

    // URL tronquée.
    sf::Text url;
    url.setFont(theme_font());
    url.setCharacterSize(FontSize::small);
    url.setFillColor(Colors::text);
    std::string trunc = model_.url;
    if (trunc.size() > 56) trunc = trunc.substr(0, 53) + "...";
    url.setString(sf::String::fromUtf8(trunc.begin(), trunc.end()));
    url.setPosition(bounds_.left + Spacing::lg,
                    bounds_.top  + Spacing::md + 54.f);
    target.draw(url);

    // Boutons.
    drawButton(target, copyBtnRect(),   "Copier l'URL",
               sf::Color::White, Colors::accent);
    drawButton(target, qrBtnRect(),     "QR",
               Colors::text,     Colors::accentLight);
    if (model_.active) {
        drawButton(target, revokeBtnRect(), "Révoquer",
                   sf::Color::White, Colors::error);
    } else {
        drawButton(target, revokeBtnRect(), "Révoqué",
                   Colors::textSecondary, Colors::sidebar);
    }
}

} // namespace ltr::ui
