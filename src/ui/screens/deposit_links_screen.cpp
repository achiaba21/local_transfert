#include "ltr/ui/screens/deposit_links_screen.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <algorithm>

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"

namespace ltr::ui {

namespace {

constexpr float kHeaderHeight = 56.f;
constexpr float kCardHeight   = 124.f;
constexpr float kCardGap      = Spacing::md;
constexpr float kContentMaxW  = 720.f;

} // namespace

DepositLinksScreen::DepositLinksScreen(app::AppController& controller)
    : controller_(controller) {
    rebuildCards();
}

void DepositLinksScreen::rebuildCards() {
    cards_.clear();
    auto links = controller_.listDepositLinks();
    for (const auto& l : links) {
        DepositLinkCard card;
        DepositLinkCard::Model m;
        m.id        = l.id;
        m.label     = l.label;
        m.url       = "https://<host>/deposit/" + l.token.substr(0,
            std::min<std::size_t>(16, l.token.size())) + "...";
        m.active    = !l.revoked;
        m.statusBadge = l.revoked ? "Révoqué" : "Actif";
        m.badgeColor  = l.revoked ? Colors::textSecondary : Colors::success;
        m.subline   = "Lien créé";
        card.setModel(std::move(m));
        const auto id = l.id;
        card.onCopy([]() {});
        card.onQr  ([]() {});
        card.onRevoke([this, id]() {
            controller_.revokeDepositLink(id);
            rebuildCards();
        });
        cards_.push_back(std::move(card));
    }
}

void DepositLinksScreen::handleEvent(const sf::Event& e,
                                     const app::AppState&) {
    if (e.type == sf::Event::MouseButtonPressed &&
        e.mouseButton.button == sf::Mouse::Left) {
        const sf::Vector2f p(static_cast<float>(e.mouseButton.x),
                              static_cast<float>(e.mouseButton.y));
        for (auto& c : cards_) {
            if (c.handleClick(p)) return;
        }
    }
}

void DepositLinksScreen::update(const app::AppState&, sf::Time) {
    // No-op : la liste est rebuild après les actions (revoke/create).
}

void DepositLinksScreen::draw(sf::RenderTarget& target) const {
    // Fond plein (rectangle simple suffisant pour le fond global).
    sf::RectangleShape bg(sf::Vector2f(
        static_cast<float>(viewSize_.x),
        static_cast<float>(viewSize_.y)));
    bg.setFillColor(Colors::bg);
    target.draw(bg);

    // Header titre.
    sf::Text title;
    title.setFont(theme_font_bold());
    title.setCharacterSize(FontSize::h1);
    title.setFillColor(Colors::text);
    const std::string titleStr = "Liens de dépôt";
    title.setString(sf::String::fromUtf8(titleStr.begin(), titleStr.end()));
    title.setPosition(Spacing::xl, Spacing::lg);
    target.draw(title);

    // Layout cards.
    const float w = std::min(kContentMaxW,
        static_cast<float>(viewSize_.x) - 2.f * Spacing::xl);
    const float xOffset = (static_cast<float>(viewSize_.x) - w) / 2.f;
    float y = kHeaderHeight + Spacing::lg;

    if (cards_.empty()) {
        sf::Text empty;
        empty.setFont(theme_font());
        empty.setCharacterSize(FontSize::body);
        empty.setFillColor(Colors::textSecondary);
        const std::string msg = "Aucun lien de dépôt pour le moment.";
        empty.setString(sf::String::fromUtf8(msg.begin(), msg.end()));
        empty.setPosition(xOffset, y + 60.f);
        target.draw(empty);
        return;
    }

    for (auto& c : cards_) {
        sf::FloatRect r(xOffset, y, w, kCardHeight);
        const_cast<DepositLinkCard&>(c).setBounds(r);
        c.draw(target);
        y += kCardHeight + kCardGap;
    }
}

} // namespace ltr::ui
