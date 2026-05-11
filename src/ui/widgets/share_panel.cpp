#include "ltr/ui/widgets/share_panel.hpp"

#include <chrono>
#include <string>

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Clipboard.hpp>

#include "ltr/ui/icon_library.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/utf8.hpp"
#include "ltr/ui/widgets/card.hpp"
#include "ltr/ui/widgets/label.hpp"

namespace ltr::ui {

namespace {

constexpr float kQrSize        = 180.f;   // V1.6.4 : 220 → 180 pour faire place à l'empreinte
constexpr float kCopyBtnH      = 32.f;
constexpr float kCloseBtnSize  = 16.f;
constexpr unsigned kPinSize    = 44;      // V1.1.8-UX4 : 28 → 44
constexpr float kFpCopyBtnW    = 76.f;    // V1.6.4 : petit bouton inline « Copier »
constexpr float kFpCopyBtnH    = 22.f;

float monotonicSeconds() {
    using namespace std::chrono;
    return static_cast<float>(
        duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count()) / 1000.f;
}

} // namespace

SharePanel::SharePanel() {
    copyUrlBtn_.setLabel("Copier URL")
               .setVariant(Button::Variant::Secondary)
               .onClick([this]{
                   if (!url_.empty()) {
                       sf::Clipboard::setString(utf8(url_));
                       copiedUrlUntil_ = monotonicSeconds() + 2.f;
                   }
               });

    copyPinBtn_.setLabel("Copier PIN")
               .setVariant(Button::Variant::Secondary)
               .onClick([this]{
                   if (!pin_.empty()) {
                       sf::Clipboard::setString(utf8(pin_));
                       copiedPinUntil_ = monotonicSeconds() + 2.f;
                   }
               });

    // V1.6.4 — petit bouton inline « Copier » à droite de l'overline EMPREINTE
    // (full-width écraserait le PIN, panel trop court).
    copyFpBtn_.setLabel("Copier")
              .setVariant(Button::Variant::Secondary)
              .onClick([this]{
                  if (!fingerprint_.empty()) {
                      sf::Clipboard::setString(utf8(fingerprint_));
                      copiedFpUntil_ = monotonicSeconds() + 2.f;
                  }
              });
}

SharePanel& SharePanel::setBounds(const sf::FloatRect& r) {
    bounds_ = r;
    layoutChildren();
    return *this;
}

SharePanel& SharePanel::setUrl(const std::string& url) {
    url_ = url;
    return *this;
}

SharePanel& SharePanel::setPin(const std::string& pin6) {
    pin_ = pin6;
    return *this;
}

SharePanel& SharePanel::setFingerprint(const std::string& fp) {
    if (fp != fingerprint_) {
        fingerprint_ = fp;
        layoutChildren();
    }
    return *this;
}

SharePanel& SharePanel::setQrImage(const sf::Image& img) {
    qr_.setImage(img);
    return *this;
}

SharePanel& SharePanel::setCollapsed(bool c) {
    collapsed_ = c;
    layoutChildren();
    return *this;
}

SharePanel& SharePanel::setVisitorCount(int n) {
    visitorCount_ = n < 0 ? 0 : n;
    return *this;
}

SharePanel& SharePanel::onToggle(std::function<void()> cb) {
    toggleCb_ = std::move(cb);
    return *this;
}

void SharePanel::layoutChildren() {
    if (collapsed_) {
        // Rien à layouter en collapsed (dessin direct + icône).
        return;
    }

    const float left = bounds_.left + Spacing::lg;
    const float right = bounds_.left + bounds_.width - Spacing::lg;
    const float width = right - left;

    // QR centré sous l'overline + bouton fermer.
    const float qrY = bounds_.top + 56.f;
    qr_.setBounds({ bounds_.left + (bounds_.width - kQrSize) / 2.f,
                    qrY, kQrSize, kQrSize });

    // 2 boutons Copier (empilés) sous l'URL.
    const float btn1Y = qrY + kQrSize + Spacing::xl + 48.f;
    copyUrlBtn_.setBounds({ left, btn1Y, width, kCopyBtnH });
    const float btn2Y = btn1Y + kCopyBtnH + Spacing::sm;
    copyPinBtn_.setBounds({ left, btn2Y, width, kCopyBtnH });

    // V1.6.4 — Le bouton « Copier » de l'empreinte est positionné en
    // drawExpanded (alignement sur l'overline EMPREINTE). Ici on lui
    // donne juste sa taille pour que handleEvent connaisse les bornes.
    if (!fingerprint_.empty()) {
        // Le X exact est calculé dans drawExpanded ; on met une valeur
        // par défaut alignée à droite du panel.
        const float fpBtnX = bounds_.left + bounds_.width - Spacing::lg - kFpCopyBtnW;
        copyFpBtn_.setBounds({ fpBtnX, 0.f, kFpCopyBtnW, kFpCopyBtnH });
    }
}

sf::FloatRect SharePanel::collapseBtnRect() const {
    return { bounds_.left + bounds_.width - Spacing::lg - kCloseBtnSize,
             bounds_.top + Spacing::xl - 2.f,
             kCloseBtnSize, kCloseBtnSize };
}

void SharePanel::handleEvent(const sf::Event& e) {
    if (collapsed_) {
        // Tout clic sur le rail = toggle.
        if (e.type == sf::Event::MouseButtonReleased &&
            e.mouseButton.button == sf::Mouse::Left) {
            const float mx = static_cast<float>(e.mouseButton.x);
            const float my = static_cast<float>(e.mouseButton.y);
            if (bounds_.contains(mx, my) && toggleCb_) {
                toggleCb_();
            }
        }
        return;
    }

    // Expanded : croix haut-droit + boutons Copier.
    if (e.type == sf::Event::MouseButtonReleased &&
        e.mouseButton.button == sf::Mouse::Left) {
        const float mx = static_cast<float>(e.mouseButton.x);
        const float my = static_cast<float>(e.mouseButton.y);
        if (collapseBtnRect().contains(mx, my) && toggleCb_) {
            toggleCb_();
            return;
        }
    }
    copyUrlBtn_.handleEvent(e);
    copyPinBtn_.handleEvent(e);
    if (!fingerprint_.empty()) copyFpBtn_.handleEvent(e);
}

void SharePanel::draw(sf::RenderTarget& target) const {
    if (collapsed_) { drawCollapsed(target); return; }
    drawExpanded(target);
}

void SharePanel::drawExpanded(sf::RenderTarget& target) const {
    // Fond + bordure gauche.
    Card{}.setBounds(bounds_).setColor(Colors::surface).draw(target);
    Card{}.setBounds({ bounds_.left, bounds_.top, 1.f, bounds_.height })
          .setColor(Colors::separator).draw(target);

    // Overline "PARTAGE WEB"
    Label title;
    title.setText("PARTAGE WEB")
         .setBold(true).setSize(FontSize::overline)
         .setColor(Colors::textSecondary)
         .setPosition(bounds_.left + Spacing::lg,
                      bounds_.top + Spacing::xl);
    title.draw(target);

    // Bouton fermer (×) haut-droit — sprite vectoriel Close.
    const auto cbr = collapseBtnRect();
    sf::Sprite close(IconLibrary::get(IconLibrary::Id::Close));
    close.setColor(Colors::textSecondary);
    close.setPosition(cbr.left, cbr.top);
    target.draw(close);

    // QR
    qr_.draw(target);

    // Label "URL" + URL affichée
    const float urlY = qr_.bounds().top + qr_.bounds().height + Spacing::lg;

    Label urlOverline;
    urlOverline.setText("URL")
               .setBold(true).setSize(FontSize::overline)
               .setColor(Colors::textSecondary)
               .setPosition(bounds_.left + Spacing::lg, urlY);
    urlOverline.draw(target);

    Label urlText;
    const std::string displayUrl = url_.empty() ? "\xE2\x80\x94" : url_;
    const float urlMaxW = bounds_.width - 2 * Spacing::lg;
    urlText.setText(displayUrl)
           .setBold(true).setSize(FontSize::body)
           .setColor(Colors::text)
           .setMaxWidth(urlMaxW)
           .setEllipsis(true)
           .setPosition(bounds_.left + Spacing::lg, urlY + 16.f);
    urlText.draw(target);

    // Boutons Copier URL / Copier PIN (avec feedback "Copi\xC3\xA9 !").
    const float now = monotonicSeconds();
    auto drawBtn = [&](const Button& src, float until, const std::string& hot){
        if (now < until) {
            Button ephemeral;
            ephemeral.setLabel(hot)
                     .setVariant(Button::Variant::Secondary)
                     .setBounds(src.bounds())
                     .setEnabled(false);
            ephemeral.draw(target);
        } else {
            src.draw(target);
        }
    };
    drawBtn(copyUrlBtn_, copiedUrlUntil_, "Copi\xC3\xA9 !");
    drawBtn(copyPinBtn_, copiedPinUntil_, "Copi\xC3\xA9 !");

    // Section PIN — « CODE D'ACC\xC3\x88S » avec accent correct V1.1.8-UX4.
    const float pinLabelY = copyPinBtn_.bounds().top
                          + copyPinBtn_.bounds().height + Spacing::xl;

    Label pinOverline;
    pinOverline.setText("CODE D'ACC\xC3\x88S")
               .setBold(true).setSize(FontSize::overline)
               .setColor(Colors::textSecondary)
               .setPosition(bounds_.left + Spacing::lg, pinLabelY);
    pinOverline.draw(target);

    // PIN formaté avec espaces (kerning simple).
    std::string pinSpaced;
    for (std::size_t i = 0; i < pin_.size(); ++i) {
        if (i) pinSpaced.push_back(' ');
        pinSpaced.push_back(pin_[i]);
    }
    if (pinSpaced.empty()) pinSpaced = "\xE2\x80\x94 \xE2\x80\x94 \xE2\x80\x94 "
                                        "\xE2\x80\x94 \xE2\x80\x94 \xE2\x80\x94";

    Label pinLabel;
    pinLabel.setText(pinSpaced)
            .setBold(true).setSize(kPinSize)
            .setColor(Colors::accent)
            .setPosition(bounds_.left + Spacing::lg, pinLabelY + 22.f);
    pinLabel.draw(target);

    // V1.6.4 — Section EMPREINTE SHA-256 compacte (HTTPS uniquement).
    // Overline + bouton « Copier » inline à droite + valeur tronquée
    // sur 1 ligne (10 octets ~ 30 chars en monospace 11 px).
    // pinLabelY : Y de l'overline « CODE D'ACCÈS »
    // pinLabelY + 22 : Y de la valeur PIN (kPinSize=44 px)
    // → l'empreinte commence 8 px sous la fin du PIN.
    if (!fingerprint_.empty()) {
        const float fpLabelY = pinLabelY + 22.f + kPinSize + 8.f;

        Label fpOverline;
        fpOverline.setText("EMPREINTE SHA-256")
                  .setBold(true).setSize(FontSize::overline)
                  .setColor(Colors::textSecondary)
                  .setPosition(bounds_.left + Spacing::lg, fpLabelY);
        fpOverline.draw(target);

        // Bouton « Copier » à droite, aligné verticalement sur l'overline.
        const float fpBtnX = bounds_.left + bounds_.width
                           - Spacing::lg - kFpCopyBtnW;
        const float fpBtnY = fpLabelY - 4.f;
        copyFpBtn_.setBounds({ fpBtnX, fpBtnY, kFpCopyBtnW, kFpCopyBtnH });
        if (monotonicSeconds() < copiedFpUntil_) {
            Button ephemeral;
            ephemeral.setLabel("Copi\xC3\xA9 !")
                     .setVariant(Button::Variant::Secondary)
                     .setBounds({ fpBtnX, fpBtnY, kFpCopyBtnW, kFpCopyBtnH })
                     .setEnabled(false);
            ephemeral.draw(target);
        } else {
            copyFpBtn_.draw(target);
        }

        // Valeur tronquée (10 octets, soit "AB:CD:EF:GH:IJ:KL:MN:OP:QR:ST…").
        std::string truncated = fingerprint_.substr(0, std::min<size_t>(29, fingerprint_.size()));
        if (fingerprint_.size() > 29) truncated += "\xE2\x80\xA6";  // …

        Label fpValue;
        fpValue.setText(truncated)
               .setSize(11)
               .setColor(Colors::text)
               .setPosition(bounds_.left + Spacing::lg, fpLabelY + 16.f);
        fpValue.draw(target);
    }
}

void SharePanel::drawCollapsed(sf::RenderTarget& target) const {
    // Fond + bordure gauche.
    Card{}.setBounds(bounds_).setColor(Colors::surface).draw(target);
    Card{}.setBounds({ bounds_.left, bounds_.top, 1.f, bounds_.height })
          .setColor(Colors::separator).draw(target);

    // Icône QR centrée en haut.
    constexpr float kIconSize = 20.f;
    sf::Sprite qrIcon(IconLibrary::get(IconLibrary::Id::QrCode));
    const auto lb = qrIcon.getLocalBounds();
    const float scale = kIconSize / lb.width;
    qrIcon.setScale(scale, scale);
    qrIcon.setColor(Colors::accent);
    qrIcon.setPosition(
        bounds_.left + (bounds_.width - kIconSize) / 2.f,
        bounds_.top + 80.f);
    target.draw(qrIcon);

    // Badge numérique si visiteur(s) web connecté(s).
    if (visitorCount_ > 0) {
        const float pillW = 24.f;
        const float pillH = 18.f;
        const float pillX = bounds_.left + (bounds_.width - pillW) / 2.f;
        const float pillY = bounds_.top + 80.f + kIconSize + 8.f;
        RoundedRect pill(pillX, pillY, pillW, pillH, Radius::pill);
        pill.setFillColor(Colors::accent);
        pill.draw(target);

        Label cnt;
        cnt.setText(std::to_string(visitorCount_))
           .setBold(true).setSize(FontSize::overline)
           .setColor(sf::Color::White);
        const auto m = cnt.measure();
        cnt.setPosition(pillX + (pillW - m.x) / 2.f,
                         pillY + (pillH - m.y) / 2.f - 2.f);
        cnt.draw(target);
    }
}

} // namespace ltr::ui
