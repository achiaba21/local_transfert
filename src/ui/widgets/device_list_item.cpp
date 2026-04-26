#include "ltr/ui/widgets/device_list_item.hpp"

#include "ltr/ui/layout.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

#include <SFML/Graphics/CircleShape.hpp>

namespace ltr::ui {

namespace {
constexpr float kStatusDotSize = 12.f;
constexpr float kPillW         = 46.f;
constexpr float kPillH         = 18.f;
constexpr float kCheckRadius   = 10.f;
} // namespace

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

    // Layout via HBox :
    //   [pastille verte 12] [nom + sub] [pill Web/Local] [checkbox ronde]
    HBox{}
        .padding(Spacing::md, 0.f)
        .spacing(Spacing::md)
        .fixed(kStatusDotSize, [](sf::RenderTarget& t,
                                    const sf::FloatRect& r) {
            const float dotR = kStatusDotSize / 2.f;
            sf::CircleShape dot(dotR);
            dot.setOrigin(dotR, dotR);
            dot.setPosition(r.left + dotR, r.top + r.height / 2.f);
            dot.setFillColor(Colors::success);
            t.draw(dot);
        })
        .expanded(1, [this](sf::RenderTarget& t,
                              const sf::FloatRect& r) {
            // Nom + sous-titre dans un VBox vertical centré.
            VBox{}
                .padding(0.f, (r.height - 40.f) / 2.f)
                .fixed(20.f, [this](sf::RenderTarget& tt,
                                      const sf::FloatRect& rr) {
                    Label{}
                        .setText(device_.name.empty()
                                 ? std::string{"(sans nom)"}
                                 : device_.name)
                        .setSize(FontSize::body)
                        .setBold(true)
                        .setColor(Colors::text)
                        .setBounds(rr)
                        .setMaxWidth(rr.width)
                        .setEllipsis(true)
                        .draw(tt);
                })
                .fixed(18.f, [this](sf::RenderTarget& tt,
                                      const sf::FloatRect& rr) {
                    const std::string ipStr = device_.ip.toString();
                    const bool hasPlatform = !device_.platform.empty();
                    const bool hasIp = !ipStr.empty() && ipStr != "0.0.0.0";
                    std::string sub;
                    if (hasPlatform && hasIp) sub = device_.platform + "  \xC2\xB7  " + ipStr;
                    else if (hasPlatform)     sub = device_.platform;
                    else if (hasIp)           sub = ipStr;
                    if (sub.empty()) return;
                    Label{}
                        .setText(sub)
                        .setSize(FontSize::small)
                        .setColor(Colors::textSecondary)
                        .setBounds(rr)
                        .setMaxWidth(rr.width)
                        .setEllipsis(true)
                        .draw(tt);
                })
                .layout(r, t);
        })
        .fixed(kPillW, [this](sf::RenderTarget& t,
                                const sf::FloatRect& r) {
            const std::string label =
                (device_.kind == domain::PeerKind::Web) ? "Web" : "Local";
            const float pillY = r.top + (r.height - kPillH) / 2.f;
            RoundedRect pill(r.left, pillY, kPillW, kPillH, Radius::pill);
            pill.setFillColor(Colors::accentLight).draw(t);
            Label{}
                .setText(label)
                .setBold(true).setSize(FontSize::overline)
                .setColor(Colors::accent)
                .setBounds({r.left, pillY, kPillW, kPillH})
                .setAlignment(Label::Alignment::Center)
                .draw(t);
        })
        .fixed(kCheckRadius * 2 + Spacing::sm,
               [this](sf::RenderTarget& t, const sf::FloatRect& r) {
            const float chkX = r.left + r.width - kCheckRadius;
            const float chkY = r.top + r.height / 2.f;
            sf::CircleShape chk(kCheckRadius);
            chk.setOrigin(kCheckRadius, kCheckRadius);
            chk.setPosition(chkX, chkY);
            chk.setFillColor(selected_ ? Colors::accent
                                          : sf::Color::Transparent);
            chk.setOutlineColor(selected_ ? Colors::accent
                                           : Colors::textSecondary);
            chk.setOutlineThickness(1.5f);
            t.draw(chk);

            if (selected_) {
                sf::CircleShape inner(3.5f);
                inner.setOrigin(3.5f, 3.5f);
                inner.setPosition(chkX, chkY);
                inner.setFillColor(sf::Color::White);
                t.draw(inner);
            }
        })
        .layout(inner, target);
}

} // namespace ltr::ui
