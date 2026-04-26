#include "ltr/ui/widgets/dropdown_menu.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

namespace ltr::ui {

namespace {
constexpr float kItemH = 32.f;
constexpr float kMenuW = 160.f;
constexpr float kMenuPad = 4.f;  // marge interne top+bottom
} // namespace

DropdownMenu& DropdownMenu::setItems(std::vector<Item> items) {
    items_ = std::move(items);
    return *this;
}

DropdownMenu& DropdownMenu::setAnchor(const sf::FloatRect& anchor) {
    anchor_ = anchor;
    return *this;
}

void DropdownMenu::openMenu() {
    open_ = true;
    hoverIdx_ = -1;
}

void DropdownMenu::close() {
    open_ = false;
    hoverIdx_ = -1;
}

bool DropdownMenu::handleEvent(const sf::Event& e) {
    if (!open_) return false;

    const float menuX = anchor_.left;
    const float menuY = anchor_.top + anchor_.height + 4.f;
    const float menuH = items_.size() * kItemH + 2 * kMenuPad;
    const sf::FloatRect menuRect{menuX, menuY, kMenuW, menuH};

    if (e.type == sf::Event::MouseMoved) {
        const float mx = static_cast<float>(e.mouseMove.x);
        const float my = static_cast<float>(e.mouseMove.y);
        if (menuRect.contains(mx, my)) {
            hoverIdx_ = static_cast<int>((my - menuY - kMenuPad) / kItemH);
            if (hoverIdx_ < 0 ||
                hoverIdx_ >= static_cast<int>(items_.size())) {
                hoverIdx_ = -1;
            }
        } else {
            hoverIdx_ = -1;
        }
        return true;  // on consomme les moves pendant qu'on est ouverts
    }

    if (e.type == sf::Event::MouseButtonReleased &&
        e.mouseButton.button == sf::Mouse::Left) {
        const float mx = static_cast<float>(e.mouseButton.x);
        const float my = static_cast<float>(e.mouseButton.y);
        if (menuRect.contains(mx, my)) {
            const int idx = static_cast<int>((my - menuY - kMenuPad) / kItemH);
            if (idx >= 0 && idx < static_cast<int>(items_.size())) {
                auto action = items_[idx].action;
                close();
                if (action) action();
            }
            return true;
        }
        // clic extérieur → fermer sans action, ne pas consommer
        close();
        return false;
    }

    // Esc ferme le menu
    if (e.type == sf::Event::KeyReleased &&
        e.key.code == sf::Keyboard::Escape) {
        close();
        return true;
    }

    return false;
}

void DropdownMenu::draw(sf::RenderTarget& target) const {
    if (!open_) return;

    const float menuX = anchor_.left;
    const float menuY = anchor_.top + anchor_.height + 4.f;
    const float menuH = items_.size() * kItemH + 2 * kMenuPad;

    // Shadow subtile (2px offset).
    RoundedRect shadow(menuX + 1.f, menuY + 2.f, kMenuW, menuH, Radius::md);
    shadow.setFillColor(Colors::shadow);
    shadow.draw(target);

    // Fond + outline.
    RoundedRect bg(menuX, menuY, kMenuW, menuH, Radius::md);
    bg.setFillColor(Colors::surface)
      .setOutline(Colors::separator, 1.f);
    bg.draw(target);

    // Items.
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const float iy = menuY + kMenuPad + i * kItemH;

        if (static_cast<int>(i) == hoverIdx_) {
            RoundedRect hoverBg(menuX + 4.f, iy + 2.f,
                                kMenuW - 8.f, kItemH - 4.f, Radius::sm);
            hoverBg.setFillColor(Colors::accentLight);
            hoverBg.draw(target);
        }

        Label l;
        l.setText(items_[i].label)
            .setSize(FontSize::body)
            .setColor(Colors::text)
            .setPosition(menuX + Spacing::md,
                         iy + (kItemH - FontSize::body) / 2.f - 2.f);
        l.draw(target);
    }
}

} // namespace ltr::ui
