#include "ltr/ui/screens/incoming_offer_screen.hpp"

#include "ltr/core/format.hpp"
#include "ltr/core/logger.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/card.hpp"
#include "ltr/ui/widgets/label.hpp"

#include <sstream>

namespace ltr::ui {

namespace {
using core::formatBytes;
} // namespace

IncomingOfferScreen::IncomingOfferScreen(app::AppController& controller)
    : controller_(controller) {
    // V1.1.2 : les boutons aiguillent selon le type d'offre active
    // (native avec PIN ou web sans PIN).
    rejectBtn_.setLabel("Refuser")
              .setVariant(Button::Variant::Secondary)
              .onClick([this]{
                  if (controller_.state().pendingWebOffer) {
                      controller_.refuseWebUpload();
                  } else {
                      controller_.rejectIncoming();
                  }
              });
    acceptBtn_.setLabel("Accepter")
              .setVariant(Button::Variant::Primary)
              .onClick([this]{
                  if (controller_.state().pendingWebOffer) {
                      controller_.acceptWebUpload();
                  } else {
                      controller_.acceptIncoming();
                  }
              });
}

void IncomingOfferScreen::setViewSize(sf::Vector2u size) {
    viewSize_ = size;
    layout();
}

void IncomingOfferScreen::layout() {
    const float w = 480.f, h = 360.f;
    cardRect_ = {(viewSize_.x - w) / 2.f, (viewSize_.y - h) / 2.f, w, h};

    const float btnW = 180.f, btnH = 44.f;
    rejectBtn_.setBounds({cardRect_.left + Spacing::xl,
                          cardRect_.top  + h - btnH - Spacing::xl,
                          btnW, btnH});
    acceptBtn_.setBounds({cardRect_.left + w - btnW - Spacing::xl,
                          cardRect_.top  + h - btnH - Spacing::xl,
                          btnW, btnH});
}

void IncomingOfferScreen::handleEvent(const sf::Event& e,
                                      const app::AppState& state) {
    // V1.1.9-batch : mode inbox list. On gère N rangées avec hit tests
    // manuels (boutons [Refuser]/[Accepter] par row + globaux + fermer).
    if (state.webInboxModalOpen && !state.webInbox.empty()
        && !state.incomingOffer && !state.pendingWebOffer) {
        if (e.type == sf::Event::MouseButtonReleased &&
            e.mouseButton.button == sf::Mouse::Left) {
            const float mx = static_cast<float>(e.mouseButton.x);
            const float my = static_cast<float>(e.mouseButton.y);

            const float cardX = cardRect_.left;
            const float cardY = cardRect_.top;
            const float cardW = cardRect_.width;
            const float cardH = cardRect_.height;

            // Header buttons : Refuser tout / Accepter tout
            constexpr float kBtnW = 110.f, kBtnH = 28.f;
            const float headerY = cardY + Spacing::xl + 6.f;
            const float refuseAllX = cardX + cardW - 2 * (kBtnW + Spacing::sm)
                                      - Spacing::lg;
            const float acceptAllX = cardX + cardW - kBtnW - Spacing::lg;
            const sf::FloatRect refuseAllR{refuseAllX, headerY, kBtnW, kBtnH};
            const sf::FloatRect acceptAllR{acceptAllX, headerY, kBtnW, kBtnH};
            if (refuseAllR.contains(mx, my)) {
                core::log_info("[inbox/diag] CLICK refuseAll at ("
                               + std::to_string(mx) + ", "
                               + std::to_string(my) + ")");
                controller_.refuseAllWebUploads();
                if (state.webInbox.empty()) controller_.toggleWebInboxModal();
                return;
            }
            if (acceptAllR.contains(mx, my)) {
                core::log_info("[inbox/diag] CLICK acceptAll at ("
                               + std::to_string(mx) + ", "
                               + std::to_string(my) + ")");
                controller_.acceptAllWebUploads();
                if (state.webInbox.empty()) controller_.toggleWebInboxModal();
                return;
            }

            // Per-row buttons
            const float listY0 = cardY + 80.f;
            constexpr float kRowH = 60.f;
            constexpr float kRowBtnW = 90.f, kRowBtnH = 28.f;
            for (std::size_t i = 0; i < state.webInbox.size(); ++i) {
                const float ry = listY0 + i * (kRowH + 6.f);
                if (ry + kRowH > cardY + cardH - 60.f) break; // overflow
                const float refuseX = cardX + cardW - 2 * (kRowBtnW + Spacing::sm)
                                       - Spacing::lg;
                const float acceptX = cardX + cardW - kRowBtnW - Spacing::lg;
                const float btnY = ry + (kRowH - kRowBtnH) / 2.f;
                const sf::FloatRect refuseR{refuseX, btnY, kRowBtnW, kRowBtnH};
                const sf::FloatRect acceptR{acceptX, btnY, kRowBtnW, kRowBtnH};
                if (refuseR.contains(mx, my)) {
                    core::log_info("[inbox/diag] CLICK refuse row=" + std::to_string(i)
                                   + " uploadId=" + state.webInbox[i].uploadId.substr(0, 8)
                                   + " at (" + std::to_string(mx) + ", "
                                   + std::to_string(my) + ")");
                    controller_.refuseWebUpload(state.webInbox[i].uploadId);
                    if (state.webInbox.empty()) controller_.toggleWebInboxModal();
                    return;
                }
                if (acceptR.contains(mx, my)) {
                    core::log_info("[inbox/diag] CLICK accept row=" + std::to_string(i)
                                   + " uploadId=" + state.webInbox[i].uploadId.substr(0, 8)
                                   + " at (" + std::to_string(mx) + ", "
                                   + std::to_string(my) + ")");
                    controller_.acceptWebUpload(state.webInbox[i].uploadId);
                    if (state.webInbox.empty()) controller_.toggleWebInboxModal();
                    return;
                }
            }

            // Footer: Fermer
            constexpr float kCloseW = 160.f;
            const float closeX = cardX + (cardW - kCloseW) / 2.f;
            const float closeY = cardY + cardH - kBtnH - Spacing::lg;
            const sf::FloatRect closeR{closeX, closeY, kCloseW, kBtnH};
            if (closeR.contains(mx, my)) {
                controller_.toggleWebInboxModal();
                return;
            }
        }
        return;  // mode inbox : ne pas propager aux boutons legacy
    }

    rejectBtn_.handleEvent(e);
    acceptBtn_.handleEvent(e);
}

void IncomingOfferScreen::update(const app::AppState& /*state*/,
                                 sf::Time /*dt*/) {}

void IncomingOfferScreen::draw(sf::RenderTarget& target) const {
    const auto& st = controller_.state();
    const bool showInbox = st.webInboxModalOpen && !st.webInbox.empty()
                            && !st.incomingOffer && !st.pendingWebOffer;
    if (!st.incomingOffer && !st.pendingWebOffer && !showInbox) return;

    if (showInbox) {
        // V1.1.9-batch : rendu mode inbox liste.
        // Overlay sombre.
        Card{}.setBounds({0, 0, (float)viewSize_.x, (float)viewSize_.y})
            .setColor(Colors::overlay).draw(target);

        // V1.1.10 : limite portée à 8 rows visibles (scroll vrai = V2).
        // Card large (640 px).
        const float w = 640.f;
        const float maxRows = 8.f;
        constexpr float kRowH = 60.f;
        const float rowsCount = static_cast<float>(
            std::min<std::size_t>(st.webInbox.size(),
                                  static_cast<std::size_t>(maxRows)));
        const float h = 80.f + rowsCount * (kRowH + 6.f) + 80.f;
        const float cardX = (viewSize_.x - w) / 2.f;
        const float cardY = (viewSize_.y - h) / 2.f;
        const_cast<sf::FloatRect&>(cardRect_) = {cardX, cardY, w, h};

        RoundedRect card(cardX, cardY, w, h, Radius::lg);
        card.setFillColor(Colors::surface)
            .setShadow(sf::Color(0, 0, 0, 60), 8.f);
        card.draw(target);

        RoundedRect stripe(cardX, cardY, w, 4.f, Radius::lg);
        stripe.setFillColor(Colors::accent).draw(target);

        // Header : titre + boutons globaux.
        Label h1;
        h1.setText("DEMANDES ENTRANTES \xC2\xB7 "
                   + std::to_string(st.webInbox.size()))
          .setBold(true).setSize(FontSize::overline)
          .setColor(Colors::textSecondary)
          .setPosition(cardX + Spacing::lg, cardY + Spacing::xl + 4.f);
        h1.draw(target);

        constexpr float kBtnW = 110.f, kBtnH = 28.f;
        const float headerY = cardY + Spacing::xl + 6.f;
        const float refuseAllX = cardX + w - 2 * (kBtnW + Spacing::sm)
                                  - Spacing::lg;
        const float acceptAllX = cardX + w - kBtnW - Spacing::lg;
        auto drawHeaderBtn = [&](float x, const std::string& label,
                                  sf::Color bg, sf::Color textCol) {
            RoundedRect b(x, headerY, kBtnW, kBtnH, Radius::sm);
            b.setFillColor(bg).draw(target);
            Label lbl;
            lbl.setText(label).setBold(true).setSize(FontSize::overline)
               .setColor(textCol);
            const auto m = lbl.measure();
            lbl.setPosition(x + (kBtnW - m.x) / 2.f,
                            headerY + (kBtnH - m.y) / 2.f - 2.f);
            lbl.draw(target);
        };
        drawHeaderBtn(refuseAllX, "Refuser tout",
                      Colors::surface, Colors::textSecondary);
        // outline pour le bouton refuser
        RoundedRect refuseAllOutline(refuseAllX, headerY,
                                      kBtnW, kBtnH, Radius::sm);
        refuseAllOutline.setFillColor(sf::Color::Transparent)
                        .setOutline(Colors::separator, 1.f).draw(target);
        drawHeaderBtn(acceptAllX, "Accepter tout",
                      Colors::accent, sf::Color::White);

        // Lignes des demandes.
        const float listY0 = cardY + 80.f;
        constexpr float kRowBtnW = 90.f, kRowBtnH = 28.f;
        for (std::size_t i = 0; i < st.webInbox.size()
                                && i < static_cast<std::size_t>(maxRows);
             ++i) {
            const auto& entry = st.webInbox[i];
            const float ry = listY0 + i * (kRowH + 6.f);

            RoundedRect rowBg(cardX + Spacing::lg, ry,
                              w - 2 * Spacing::lg, kRowH, Radius::sm);
            rowBg.setFillColor(Colors::sidebar).draw(target);

            // Titre row : peer + nom fichier
            Label l1;
            l1.setText("\xE2\x86\x93 " + entry.senderName + "  \xC2\xB7  "
                       + entry.firstFileName)
              .setBold(true).setSize(FontSize::body)
              .setColor(Colors::text)
              .setPosition(cardX + Spacing::lg + Spacing::md, ry + 8.f);
            l1.draw(target);

            // Sous-titre : taille + count
            std::ostringstream sub;
            sub << formatBytes(entry.totalBytes) << "  \xC2\xB7  "
                << entry.filesCount << " fichier"
                << (entry.filesCount > 1 ? "s" : "");
            Label l2;
            l2.setText(sub.str())
              .setSize(FontSize::small)
              .setColor(Colors::textSecondary)
              .setPosition(cardX + Spacing::lg + Spacing::md, ry + 32.f);
            l2.draw(target);

            // Boutons row
            const float refuseX = cardX + w - 2 * (kRowBtnW + Spacing::sm)
                                   - Spacing::lg;
            const float acceptX = cardX + w - kRowBtnW - Spacing::lg;
            const float btnY = ry + (kRowH - kRowBtnH) / 2.f;
            auto drawRowBtn = [&](float x, const std::string& label,
                                   sf::Color bg, sf::Color textCol,
                                   bool outline) {
                RoundedRect b(x, btnY, kRowBtnW, kRowBtnH, Radius::sm);
                b.setFillColor(bg);
                if (outline) b.setOutline(Colors::separator, 1.f);
                b.draw(target);
                Label lbl;
                lbl.setText(label).setBold(true).setSize(FontSize::overline)
                   .setColor(textCol);
                const auto m = lbl.measure();
                lbl.setPosition(x + (kRowBtnW - m.x) / 2.f,
                                btnY + (kRowBtnH - m.y) / 2.f - 2.f);
                lbl.draw(target);
            };
            drawRowBtn(refuseX, "Refuser",
                       Colors::surface, Colors::textSecondary, true);
            drawRowBtn(acceptX, "Accepter",
                       Colors::accent, sf::Color::White, false);
        }

        // Si plus de 5 entrées, afficher un indicateur.
        if (st.webInbox.size() > static_cast<std::size_t>(maxRows)) {
            Label more;
            more.setText("\xE2\x80\xA6 et "
                + std::to_string(st.webInbox.size()
                                 - static_cast<std::size_t>(maxRows))
                + " autre(s) en attente")
                .setSize(FontSize::small)
                .setColor(Colors::textSecondary)
                .setPosition(cardX + Spacing::lg,
                              listY0 + maxRows * (kRowH + 6.f) + 4.f);
            more.draw(target);
        }

        // Footer : Fermer sans agir
        constexpr float kCloseW = 160.f;
        const float closeX = cardX + (w - kCloseW) / 2.f;
        const float closeY = cardY + h - kBtnH - Spacing::lg;
        RoundedRect closeBg(closeX, closeY, kCloseW, kBtnH, Radius::sm);
        closeBg.setFillColor(Colors::surface)
               .setOutline(Colors::separator, 1.f).draw(target);
        Label closeLbl;
        closeLbl.setText("Fermer sans agir")
                .setBold(true).setSize(FontSize::overline)
                .setColor(Colors::textSecondary);
        const auto cm = closeLbl.measure();
        closeLbl.setPosition(closeX + (kCloseW - cm.x) / 2.f,
                              closeY + (kBtnH - cm.y) / 2.f - 2.f);
        closeLbl.draw(target);

        return;  // mode inbox terminé
    }


    // Overlay sombre.
    Card{}.setBounds({0, 0, (float)viewSize_.x, (float)viewSize_.y})
        .setColor(Colors::overlay).draw(target);

    // Carte centrale arrondie + ombre portée.
    RoundedRect card(cardRect_.left, cardRect_.top,
                     cardRect_.width, cardRect_.height, Radius::lg);
    card.setFillColor(Colors::surface)
        .setShadow(sf::Color(0, 0, 0, 60), 8.f);
    card.draw(target);

    // Ligne d'accent indigo en haut de la carte.
    RoundedRect stripe(cardRect_.left, cardRect_.top,
                       cardRect_.width, 4.f, Radius::lg);
    stripe.setFillColor(Colors::accent).draw(target);

    const bool isWeb = st.pendingWebOffer.has_value();

    Label h1;
    h1.setText("Transfert entrant")
      .setBold(true).setSize(FontSize::h1)
      .setColor(Colors::text)
      .setPosition(cardRect_.left + Spacing::xl,
                   cardRect_.top  + Spacing::xl);
    h1.draw(target);

    const std::string senderName = isWeb
        ? st.pendingWebOffer->senderName
        : st.incomingOffer->senderName;
    Label from;
    from.setText("de « " + senderName + " »")
        .setSize(FontSize::body)
        .setColor(Colors::textSecondary)
        .setPosition(cardRect_.left + Spacing::xl,
                     cardRect_.top  + Spacing::xl + 32.f);
    from.draw(target);

    if (isWeb) {
        // V1.1.2 : pas de PIN pour les offres web (session déjà auth).
        // Libellé "Via l'interface web" à la place du code PIN.
        const float iconBoxH = 80.f;
        RoundedRect iconBg(cardRect_.left + Spacing::xl,
                           cardRect_.top  + 96.f,
                           cardRect_.width - 2 * Spacing::xl,
                           iconBoxH, Radius::md);
        iconBg.setFillColor(Colors::accentLight).draw(target);

        Label info;
        info.setText("Via l'interface web")
            .setBold(true).setSize(FontSize::h1)
            .setColor(Colors::accent);
        const auto im = info.measure();
        info.setPosition(
            cardRect_.left + (cardRect_.width - im.x) / 2.f,
            cardRect_.top  + 96.f + (iconBoxH - im.y) / 2.f - 6.f);
        info.draw(target);

        std::ostringstream summary;
        summary << st.pendingWebOffer->filesCount << " fichier(s)  ·  "
                << formatBytes(st.pendingWebOffer->totalBytes);
        Label s;
        s.setText(summary.str())
         .setSize(FontSize::body)
         .setColor(Colors::text);
        const auto sm = s.measure();
        s.setPosition(
            cardRect_.left + (cardRect_.width - sm.x) / 2.f,
            cardRect_.top  + 200.f);
        s.draw(target);

        Label r;
        r.setText("  • " + st.pendingWebOffer->firstFileName
                  + (st.pendingWebOffer->filesCount > 1
                     ? ("  (+ " + std::to_string(
                            st.pendingWebOffer->filesCount - 1) + " autre)")
                     : std::string{}))
         .setSize(FontSize::small)
         .setColor(Colors::textSecondary)
         .setPosition(cardRect_.left + Spacing::xl,
                      cardRect_.top  + 224.f);
        r.draw(target);
    } else {
        // Flow natif : PIN en grand.
        const float pinH = 80.f;
        RoundedRect pinBg(cardRect_.left + Spacing::xl,
                          cardRect_.top  + 96.f,
                          cardRect_.width - 2 * Spacing::xl,
                          pinH, Radius::md);
        pinBg.setFillColor(Colors::accentLight).draw(target);

        Label pin;
        pin.setText(st.incomingPinDisplay.empty()
                    ? st.incomingOffer->pinCode
                    : st.incomingPinDisplay)
           .setBold(true).setSize(FontSize::pin)
           .setColor(Colors::accent);
        const auto m = pin.measure();
        pin.setPosition(
            cardRect_.left + (cardRect_.width - m.x) / 2.f,
            cardRect_.top  + 96.f + (pinH - m.y) / 2.f - 8.f);
        pin.draw(target);

        std::ostringstream summary;
        summary << st.incomingOffer->files.size() << " fichier(s)  ·  "
                << formatBytes(st.incomingOffer->totalSize());
        Label s;
        s.setText(summary.str())
         .setSize(FontSize::body)
         .setColor(Colors::text);
        const auto sm = s.measure();
        s.setPosition(
            cardRect_.left + (cardRect_.width - sm.x) / 2.f,
            cardRect_.top  + 200.f);
        s.draw(target);

        float ly = cardRect_.top + 224.f;
        int shown = 0;
        for (const auto& f : st.incomingOffer->files) {
            if (shown >= 2) {
                Label more;
                more.setText("  ...et " +
                    std::to_string(st.incomingOffer->files.size() - shown) +
                    " autre(s)")
                    .setSize(FontSize::small)
                    .setColor(Colors::textSecondary)
                    .setPosition(cardRect_.left + Spacing::xl, ly);
                more.draw(target);
                break;
            }
            Label r;
            r.setText("  • " + f.relativePath)
             .setSize(FontSize::small)
             .setColor(Colors::textSecondary)
             .setPosition(cardRect_.left + Spacing::xl, ly);
            r.draw(target);
            ly += 18.f;
            ++shown;
        }
    }

    rejectBtn_.draw(target);
    acceptBtn_.draw(target);
}

} // namespace ltr::ui
