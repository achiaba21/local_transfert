#include "ltr/ui/widgets/file_row.hpp"

#include <SFML/Graphics/Sprite.hpp>

#include "ltr/core/format.hpp"
#include "ltr/ui/icon_library.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

namespace ltr::ui {

namespace {
using core::formatBytes;

constexpr float kCheckboxSize = 20.f;
constexpr float kFolderIconSize = 20.f;
constexpr float kCloseIconSize  = 16.f;
constexpr float kCheckIconSize  = 24.f;
} // namespace

sf::FloatRect FileRow::closeBtnBounds() const {
    const float sz = 22.f;
    return {bounds_.left + bounds_.width - sz - Spacing::md,
            bounds_.top  + (bounds_.height - sz) / 2.f,
            sz, sz};
}

sf::FloatRect FileRow::checkboxBounds() const {
    return {bounds_.left + Spacing::md,
            bounds_.top  + (bounds_.height - kCheckboxSize) / 2.f,
            kCheckboxSize, kCheckboxSize};
}

void FileRow::handleEvent(const sf::Event& e) {
    const auto xBtn = closeBtnBounds();
    const auto cb   = checkboxBounds();

    if (e.type == sf::Event::MouseMoved) {
        hoverX_ = xBtn.contains(
            static_cast<float>(e.mouseMove.x),
            static_cast<float>(e.mouseMove.y));
    }
    else if (e.type == sf::Event::MouseButtonReleased &&
             e.mouseButton.button == sf::Mouse::Left) {
        const float mx = static_cast<float>(e.mouseButton.x);
        const float my = static_cast<float>(e.mouseButton.y);

        const sf::FloatRect hitArea{cb.left - 4.f, cb.top - 4.f,
                                    cb.width + 8.f, cb.height + 8.f};
        if (hitArea.contains(mx, my) && toggleCb_) {
            toggleCb_(!checked_);
            return;
        }

        if (xBtn.contains(mx, my) && cb_) {
            cb_();
        }
    }
}

void FileRow::draw(sf::RenderTarget& target) const {
    RoundedRect bg(bounds_.left, bounds_.top,
                   bounds_.width, bounds_.height, Radius::sm);
    bg.setFillColor(Colors::surface)
      .setOutline(Colors::separator, 1.f);
    bg.draw(target);

    // Checkbox 20×20 à gauche — V1.1.8-UX1 : sprite vectoriel blanc sur
    // fond accent quand cochée (remplace le glyphe "v" ASCII).
    const auto cb = checkboxBounds();
    RoundedRect chk(cb.left, cb.top, cb.width, cb.height, Radius::sm);
    if (checked_) {
        chk.setFillColor(Colors::accent);
        chk.draw(target);

        // Le PNG check fait 24×24, on l'affiche à 16×16 centré dans la
        // box 20×20 (2 px de marge esthétique de chaque côté).
        constexpr float kCheckDrawSize = 16.f;
        const float scale = kCheckDrawSize / kCheckIconSize;
        sf::Sprite check(IconLibrary::get(IconLibrary::Id::Check));
        check.setColor(sf::Color::White);
        check.setScale(scale, scale);
        check.setPosition(
            cb.left + (cb.width  - kCheckDrawSize) / 2.f,
            cb.top  + (cb.height - kCheckDrawSize) / 2.f);
        target.draw(check);
    } else {
        chk.setFillColor(Colors::surface)
           .setOutline(Colors::separator, 1.f);
        chk.draw(target);
    }

    // Zone de texte à droite de la checkbox.
    float textLeft = cb.left + cb.width + Spacing::md;

    // V1.1.8-UX1 : icône dossier avant le nom (remplace le préfixe "[D] ").
    if (kind_ == Kind::Folder) {
        sf::Sprite folder(IconLibrary::get(IconLibrary::Id::Folder));
        folder.setColor(Colors::accent);
        folder.setPosition(
            textLeft,
            bounds_.top + (bounds_.height - kFolderIconSize) / 2.f);
        target.draw(folder);
        textLeft += kFolderIconSize + Spacing::sm;
    }

    const float nameY = (kind_ == Kind::Folder)
        ? bounds_.top + 6.f
        : bounds_.top + (bounds_.height - 18) / 2.f;
    Label n;
    n.setText(name_)
     .setSize(FontSize::body)
     .setBold(kind_ == Kind::Folder)
     .setColor(checked_ ? Colors::text : Colors::textSecondary)
     .setPosition(textLeft, nameY);
    n.draw(target);

    if (kind_ == Kind::Folder) {
        Label sub;
        sub.setText(std::to_string(fileCount_) +
                    (fileCount_ > 1 ? " fichiers" : " fichier"))
           .setSize(FontSize::small)
           .setColor(Colors::textSecondary)
           .setPosition(textLeft, bounds_.top + 26.f);
        sub.draw(target);
    }

    Label sz;
    sz.setText(formatBytes(size_))
      .setSize(FontSize::small)
      .setColor(Colors::textSecondary);
    const auto m = sz.measure();
    sz.setPosition(
        bounds_.left + bounds_.width - m.x - 48.f,
        bounds_.top  + (bounds_.height - 14) / 2.f);
    sz.draw(target);

    // V1.1.8-UX1 : croix de suppression par icône vectorielle. Affichée en
    // textSecondary, fond hover separator.
    if (!cb_) return;
    const auto xBtn = closeBtnBounds();
    if (hoverX_) {
        RoundedRect btn(xBtn.left, xBtn.top, xBtn.width, xBtn.height, Radius::sm);
        btn.setFillColor(Colors::separator).draw(target);
    }
    sf::Sprite closeIcon(IconLibrary::get(IconLibrary::Id::Close));
    closeIcon.setColor(Colors::textSecondary);
    closeIcon.setPosition(
        xBtn.left + (xBtn.width  - kCloseIconSize) / 2.f,
        xBtn.top  + (xBtn.height - kCloseIconSize) / 2.f);
    target.draw(closeIcon);
}

} // namespace ltr::ui
