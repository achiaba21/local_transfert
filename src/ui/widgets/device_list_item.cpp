#include "ltr/ui/widgets/device_list_item.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

#include <SFML/Graphics/CircleShape.hpp>

namespace ltr::ui {

void DeviceListItem::handleEvent(const sf::Event& e) {
    if (e.type == sf::Event::MouseMoved) {
        hover_ = bounds_.contains(
            static_cast<float>(e.mouseMove.x),
            static_cast<float>(e.mouseMove.y));
    }
    else if (e.type == sf::Event::MouseButtonReleased &&
             e.mouseButton.button == sf::Mouse::Left) {
        if (bounds_.contains(
                static_cast<float>(e.mouseButton.x),
                static_cast<float>(e.mouseButton.y)) && cb_) {
            cb_();
        }
    }
}

void DeviceListItem::draw(sf::RenderTarget& target) const {
    const float pad = Spacing::sm;
    const sf::FloatRect inner{
        bounds_.left + pad, bounds_.top + 2.f,
        bounds_.width - 2 * pad, bounds_.height - 4.f};

    // Fond (selected / hover / default).
    if (selected_ || hover_) {
        RoundedRect rr(inner.left, inner.top, inner.width, inner.height,
                       Radius::md);
        rr.setFillColor(selected_ ? Colors::accentLight : Colors::separator);
        rr.draw(target);
    }

    // Pastille de statut (en ligne, à gauche).
    const float dotR = 6.f;
    const float dotX = inner.left + Spacing::md + dotR;
    const float dotY = inner.top  + inner.height / 2.f;
    sf::CircleShape dot(dotR);
    dot.setOrigin(dotR, dotR);
    dot.setPosition(dotX, dotY);
    dot.setFillColor(Colors::success);
    target.draw(dot);

    // Nom + plateforme / IP.
    Label name;
    name.setText(device_.name.empty() ? "(sans nom)" : device_.name)
        .setSize(FontSize::body)
        .setBold(true)
        .setColor(Colors::text)
        .setPosition(dotX + dotR + 10.f, inner.top + 8.f);
    name.draw(target);

    const std::string ipStr = device_.ip.toString();
    const bool hasPlatform  = !device_.platform.empty();
    const bool hasIp        = !ipStr.empty() && ipStr != "0.0.0.0";

    std::string subText;
    if (hasPlatform && hasIp)       subText = device_.platform + "  ·  " + ipStr;
    else if (hasPlatform)           subText = device_.platform;
    else if (hasIp)                 subText = ipStr;

    if (!subText.empty()) {
        Label sub;
        sub.setText(subText)
           .setSize(FontSize::small)
           .setColor(Colors::textSecondary)
           .setPosition(dotX + dotR + 10.f, inner.top + 28.f);
        sub.draw(target);
    }

    // Checkbox ronde à droite.
    const float chkR = 10.f;
    const float chkX = inner.left + inner.width - Spacing::md - chkR;
    const float chkY = dotY;

    // V1.1.8-UX1 : pill « Web » ou « Local » à gauche de la checkbox.
    // Symétrie visuelle : tous les peers ont un pill indiquant le canal.
    {
        const std::string pillLabel =
            (device_.kind == domain::PeerKind::Web) ? "Web" : "Local";
        constexpr float pillW = 46.f;
        constexpr float pillH = 18.f;
        const float pillRight = chkX - chkR - Spacing::md;
        const float pillLeft  = pillRight - pillW;
        const float pillTop   = chkY - pillH / 2.f;

        RoundedRect pill(pillLeft, pillTop, pillW, pillH, Radius::pill);
        pill.setFillColor(Colors::accentLight);
        pill.draw(target);

        Label pillText;
        pillText.setText(pillLabel)
                .setBold(true).setSize(FontSize::overline)
                .setColor(Colors::accent);
        const auto m = pillText.measure();
        pillText.setPosition(pillLeft + (pillW - m.x) / 2.f,
                             pillTop  + (pillH - m.y) / 2.f - 2.f);
        pillText.draw(target);
    }

    sf::CircleShape chk(chkR);
    chk.setOrigin(chkR, chkR);
    chk.setPosition(chkX, chkY);
    chk.setFillColor(selected_ ? Colors::accent : sf::Color::Transparent);
    chk.setOutlineColor(selected_ ? Colors::accent : Colors::textSecondary);
    chk.setOutlineThickness(1.5f);
    target.draw(chk);

    if (selected_) {
        sf::CircleShape inner_dot(3.5f);
        inner_dot.setOrigin(3.5f, 3.5f);
        inner_dot.setPosition(chkX, chkY);
        inner_dot.setFillColor(sf::Color::White);
        target.draw(inner_dot);
    }
}

} // namespace ltr::ui
