#include "ltr/ui/widgets/file_row.hpp"

#include <SFML/Graphics/Sprite.hpp>

#include "ltr/core/format.hpp"
#include "ltr/ui/icon_library.hpp"
#include "ltr/ui/layout.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

namespace ltr::ui {

namespace {
using core::formatBytes;

constexpr float kCheckboxSize   = 20.f;
constexpr float kFolderIconSize = 20.f;
constexpr float kCloseIconSize  = 16.f;
constexpr float kCheckIconSize  = 24.f;
constexpr float kCloseBtnSize   = 22.f;
constexpr float kSizeColW       = 90.f;
} // namespace

sf::FloatRect FileRow::closeBtnBounds() const {
    return {bounds_.left + bounds_.width - kCloseBtnSize - Spacing::md,
            bounds_.top  + (bounds_.height - kCloseBtnSize) / 2.f,
            kCloseBtnSize, kCloseBtnSize};
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
        if (xBtn.contains(mx, my) && cb_) cb_();
    }
}

void FileRow::draw(sf::RenderTarget& target) const {
    // Fond de la row.
    RoundedRect bg(bounds_.left, bounds_.top,
                   bounds_.width, bounds_.height, Radius::sm);
    bg.setFillColor(Colors::surface)
      .setOutline(Colors::separator, 1.f);
    bg.draw(target);

    // Layout via HBox : checkbox | (icône dossier?) | nom+sous-ligne | taille | croix
    HBox{}
        .padding(Spacing::md, 0.f)
        .spacing(Spacing::md)
        .fixed(kCheckboxSize, [this](sf::RenderTarget& t,
                                       const sf::FloatRect& r) {
            const float y = r.top + (r.height - kCheckboxSize) / 2.f;
            const sf::FloatRect cb{r.left, y, kCheckboxSize, kCheckboxSize};
            RoundedRect chk(cb.left, cb.top, cb.width, cb.height, Radius::sm);
            if (checked_) {
                chk.setFillColor(Colors::accent);
                chk.draw(t);
                constexpr float kCheckDrawSize = 16.f;
                const float scale = kCheckDrawSize / kCheckIconSize;
                sf::Sprite check(IconLibrary::get(IconLibrary::Id::Check));
                check.setColor(sf::Color::White);
                check.setScale(scale, scale);
                check.setPosition(
                    cb.left + (cb.width  - kCheckDrawSize) / 2.f,
                    cb.top  + (cb.height - kCheckDrawSize) / 2.f);
                t.draw(check);
            } else {
                chk.setFillColor(Colors::surface)
                   .setOutline(Colors::separator, 1.f);
                chk.draw(t);
            }
        })
        .fixed(kind_ == Kind::Folder
               ? (kFolderIconSize + Spacing::sm) : 0.f,
               [this](sf::RenderTarget& t, const sf::FloatRect& r) {
            if (kind_ != Kind::Folder) return;
            sf::Sprite folder(IconLibrary::get(IconLibrary::Id::Folder));
            folder.setColor(Colors::accent);
            folder.setPosition(
                r.left,
                r.top + (r.height - kFolderIconSize) / 2.f);
            t.draw(folder);
        })
        .expanded(1, [this](sf::RenderTarget& t, const sf::FloatRect& r) {
            // Nom + sous-ligne (folder file count) en VBox.
            const bool folder = (kind_ == Kind::Folder);
            VBox{}
                .padding(0.f, folder ? 6.f : (r.height - 18.f) / 2.f)
                .fixed(folder ? 18.f : 18.f,
                       [this, folder](sf::RenderTarget& tt,
                                       const sf::FloatRect& rr) {
                    Label{}
                        .setText(name_)
                        .setSize(FontSize::body)
                        .setBold(folder)
                        .setColor(checked_ ? Colors::text
                                            : Colors::textSecondary)
                        .setBounds(rr)
                        .setMaxWidth(rr.width)
                        .setEllipsis(true)
                        .draw(tt);
                })
                .fixed(folder ? 16.f : 0.f,
                       [this, folder](sf::RenderTarget& tt,
                                       const sf::FloatRect& rr) {
                    if (!folder) return;
                    Label{}
                        .setText(std::to_string(fileCount_) +
                                 (fileCount_ > 1 ? " fichiers" : " fichier"))
                        .setSize(FontSize::small)
                        .setColor(Colors::textSecondary)
                        .setBounds(rr)
                        .setMaxWidth(rr.width)
                        .setEllipsis(true)
                        .draw(tt);
                })
                .layout(r, t);
        })
        .fixed(kSizeColW, [this](sf::RenderTarget& t,
                                  const sf::FloatRect& r) {
            Label{}
                .setText(formatBytes(size_))
                .setSize(FontSize::small)
                .setColor(Colors::textSecondary)
                .setBounds(r)
                .setAlignment(Label::Alignment::Right)
                .setMaxWidth(r.width)
                .setEllipsis(true)
                .draw(t);
        })
        .fixed(cb_ ? kCloseBtnSize : 0.f,
               [this](sf::RenderTarget& t, const sf::FloatRect& r) {
            if (!cb_) return;
            const float y = r.top + (r.height - kCloseBtnSize) / 2.f;
            const sf::FloatRect xBtn{r.left, y,
                                      kCloseBtnSize, kCloseBtnSize};
            if (hoverX_) {
                RoundedRect btn(xBtn.left, xBtn.top, xBtn.width,
                                 xBtn.height, Radius::sm);
                btn.setFillColor(Colors::separator).draw(t);
            }
            sf::Sprite closeIcon(IconLibrary::get(IconLibrary::Id::Close));
            closeIcon.setColor(Colors::textSecondary);
            closeIcon.setPosition(
                xBtn.left + (xBtn.width  - kCloseIconSize) / 2.f,
                xBtn.top  + (xBtn.height - kCloseIconSize) / 2.f);
            t.draw(closeIcon);
        })
        .layout(bounds_, target);
}

} // namespace ltr::ui
