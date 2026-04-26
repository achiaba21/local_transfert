#include "ltr/ui/screens/main_screen.hpp"

#include "ltr/core/format.hpp"
#include "ltr/ui/breakpoint.hpp"
#include "ltr/ui/clip_scope.hpp"
#include "ltr/ui/icon_library.hpp"
#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/card.hpp"
#include "ltr/ui/widgets/device_list_item.hpp"
#include "ltr/ui/widgets/file_row.hpp"
#include "ltr/ui/widgets/label.hpp"
#include "ltr/ui/widgets/progress_bar.hpp"
#include "ltr/web/qr_code.hpp"

#include <SFML/Graphics/Sprite.hpp>

#include <cmath>
#include <filesystem>
#include <sstream>

#include <tinyfiledialogs.h>

namespace ltr::ui {

namespace {

constexpr float kHeaderH      = 64.f;
constexpr float kSidebarW     = 300.f;
// V1.1.8-UX4 : largeurs selon collapsed state
constexpr float kSharePanelExpandedW = 240.f;
constexpr float kSharePanelCollapsedW = 40.f;
constexpr float kBottomH      = 104.f;
constexpr float kItemH        = 64.f;
constexpr float kFileRowH     = 48.f;

// Zone "Rechercher" au-dessus de la liste des pairs.
constexpr float kSearchH      = 132.f;
constexpr float kControlH     = 36.f;
constexpr float kAddBtnW      = 40.f;
constexpr float kListHeaderH  = 32.f; // "APPAREILS · N" + marge
constexpr float kItemGap      = 4.f;

using core::formatBytes;
using core::formatSpeed;
using core::formatEta;

} // namespace

MainScreen::MainScreen(app::AppController& controller)
    : controller_(controller) {
    // V1.1.8-UX1 : bouton unique « Ajouter ▾ » + menu déroulant à 2 items.
    addBtn_.setLabel("Ajouter  \xE2\x96\xBE")  // UTF-8 ▾ (U+25BE)
        .setVariant(Button::Variant::Secondary)
        .onClick([this]{
            addMenu_.setAnchor(addBtn_.bounds()).openMenu();
        });

    addMenu_.setItems({
        {"Fichiers...", [this]{ openFilesPicker(); }},
        {"Dossier...",  [this]{ openFolderPicker(); }},
    });

    sendBtn_.setLabel("ENVOYER").setVariant(Button::Variant::Primary)
        .onClick([this]{ controller_.requestSend(); });

    clearFilesBtn_.setLabel("Vider").setVariant(Button::Variant::Secondary)
        .onClick([this]{ controller_.clearFiles(); });

    rescanBtn_.setLabel("Rescanner").setVariant(Button::Variant::Secondary)
        .onClick([this]{
            controller_.rescan();
            // V1.1.8-UX1 : reset empty state → radar pulsant à nouveau.
            emptyElapsed_ = 0.f;
            hasEverSeenPeer_ = false;
        });

    addIpBtn_.setLabel("+").setVariant(Button::Variant::Primary)
        .onClick([this]{
            if (!ipInput_.value().empty()) {
                controller_.probePeer(ipInput_.value());
                ipInput_.clear();
            }
        });

    // V1.1.8-UX4 : toggle SharePanel plier/déplier + re-layout.
    sharePanel_.onToggle([this]{
        controller_.toggleSharePanel();
        rebuildLayout();
    });

    ipInput_.setPlaceholder("192.168.1.x")
        .setMaxLength(15)
        .setAllowedChars("0123456789.")
        .onSubmit([this](const std::string& v){
            if (!v.empty()) {
                controller_.probePeer(v);
                ipInput_.clear();
            }
        });
}

void MainScreen::setViewSize(sf::Vector2u size) {
    viewSize_ = size;
    rebuildLayout();
}

void MainScreen::rebuildLayout() {
    const float w = static_cast<float>(viewSize_.x);
    const float h = static_cast<float>(viewSize_.y);

    // V1.1.10 — Breakpoint responsive.
    breakpoint_ = detectBreakpoint(viewSize_.x);
    const auto m = metricsFor(breakpoint_);
    const float sidebarW =
        (breakpoint_ == Breakpoint::Compact && !compactSidebarOpen_)
            ? 0.f : m.sidebarW;

    headerRect_     = {0.f, 0.f, w, kHeaderH};
    sidebarRect_    = {0.f, kHeaderH, sidebarW, h - kHeaderH - kBottomH};
    const float spW = sharePanelWidth();
    sharePanelRect_ = {w - spW, kHeaderH,
                       spW, h - kHeaderH - kBottomH};
    centerRect_     = {sidebarW, kHeaderH,
                       w - sidebarW - spW,
                       h - kHeaderH - kBottomH};
    bottomRect_     = {0.f, h - kBottomH, w, kBottomH};

    sharePanel_.setBounds(sharePanelRect_)
               .setCollapsed(controller_.isSharePanelCollapsed());

    // V1.1.8-UX1 : un seul bouton Ajouter ▾ + Vider + ENVOYER à droite.
    const float btnH = 44.f;
    const float btnY = centerRect_.top + centerRect_.height - btnH - Spacing::xl;
    const float gap  = Spacing::sm;

    const float addW    = 140.f;
    const float clearW  = 75.f;
    const float sendW   = 180.f;

    const float baseX = centerRect_.left + Spacing::xl;
    addBtn_.setBounds({baseX, btnY, addW, btnH});
    clearFilesBtn_.setBounds({baseX + addW + gap, btnY, clearW, btnH});

    sendBtn_.setBounds({
        centerRect_.left + centerRect_.width - sendW - Spacing::xl,
        btnY, sendW, btnH});

    // Zone "Rechercher" en haut de la sidebar.
    const float zoneLeft = sidebarRect_.left + Spacing::md;
    const float zoneW    = sidebarRect_.width - 2 * Spacing::md;

    // Ligne 1 : bouton Rescanner pleine largeur, sous l'overline.
    const float rescanY = sidebarRect_.top + Spacing::xl + Spacing::sm;
    rescanBtn_.setBounds({zoneLeft, rescanY, zoneW, kControlH});

    // Ligne 2 : TextInput (flex) + bouton "+".
    const float ipRowY  = rescanY + kControlH + Spacing::sm;
    const float inputW  = zoneW - kAddBtnW - Spacing::sm;
    ipInput_.setBounds({zoneLeft, ipRowY, inputW, kControlH});
    addIpBtn_.setBounds({zoneLeft + inputW + Spacing::sm, ipRowY,
                         kAddBtnW, kControlH});

    peerListStartY_ = sidebarRect_.top + kSearchH + kListHeaderH;
}

void MainScreen::openFilesPicker() {
    // V1.1.4 : sélection de fichiers uniquement (multi-sélection).
    const char* res = tinyfd_openFileDialog(
        "Sélectionner des fichiers à envoyer",
        "", 0, nullptr, nullptr, 1 /* allowMultipleSelects */);
    if (!res) return; // user a annulé

    // res est une liste de chemins séparés par '|'.
    std::vector<std::filesystem::path> paths;
    std::string all = res;
    std::size_t start = 0;
    while (start <= all.size()) {
        const auto pipe = all.find('|', start);
        const auto end = (pipe == std::string::npos) ? all.size() : pipe;
        if (end > start) paths.emplace_back(all.substr(start, end - start));
        if (pipe == std::string::npos) break;
        start = pipe + 1;
    }
    controller_.addFiles(paths);
}

void MainScreen::openFolderPicker() {
    // V1.1.4 : sélection d'un dossier (FilesystemService::enumerate
    // préservera l'arborescence à l'arrivée).
    const char* dir = tinyfd_selectFolderDialog(
        "Sélectionner un dossier à envoyer", "");
    if (!dir || !*dir) return;
    controller_.addFiles({std::filesystem::path(dir)});
}

void MainScreen::handleEvent(const sf::Event& e, const app::AppState& state) {
    // V1.1.8-UX1 : le menu a priorité s'il est ouvert (capte clic + moves).
    if (addMenu_.isOpen()) {
        if (addMenu_.handleEvent(e)) return;
    }

    // Propager aux boutons.
    addBtn_.handleEvent(e);
    clearFilesBtn_.handleEvent(e);
    sendBtn_.setEnabled(controller_.canSend());
    sendBtn_.handleEvent(e);

    // Zone Rechercher : TextInput d'abord (captation focus), puis boutons.
    ipInput_.handleEvent(e);
    rescanBtn_.handleEvent(e);
    addIpBtn_.handleEvent(e);

    // Panneau de partage web (bouton Copier).
    sharePanel_.handleEvent(e);

    // V1.1.5 : molette = scroll de la liste des fichiers dans le centre.
    // V1.1.8-UX2 : molette dans la zone TRANSFERTS = scroll horizontal
    // (utilise la molette verticale ET horizontale, shift non nécessaire).
    if (e.type == sf::Event::MouseWheelScrolled) {
        const float mx = static_cast<float>(e.mouseWheelScroll.x);
        const float my = static_cast<float>(e.mouseWheelScroll.y);
        if (centerRect_.contains(mx, my) &&
            e.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
            filesScrollY_ -= e.mouseWheelScroll.delta * 24.f;
            if (filesScrollY_ < 0.f) filesScrollY_ = 0.f;
            const float listH = centerRect_.height - Spacing::xl - 64.f - 112.f;
            const float maxScroll =
                std::max(0.f, filesContentHeight_ - listH);
            if (filesScrollY_ > maxScroll) filesScrollY_ = maxScroll;
        }
        else if (bottomRect_.contains(mx, my)) {
            // Molette verticale OU horizontale dans la zone transferts →
            // défilement horizontal des cards.
            transfersScrollX_ -= e.mouseWheelScroll.delta * 32.f;
            if (transfersScrollX_ < 0.f) transfersScrollX_ = 0.f;
            const float zoneW = transfersZoneRight() - transfersZoneLeft();
            const float maxScroll = std::max(0.f, transfersContentW_ - zoneW);
            if (transfersScrollX_ > maxScroll) transfersScrollX_ = maxScroll;
        }
    }

    // Clics dans la sidebar → (dé)sélection des pairs — uniquement sous la
    // zone Rechercher, pour ne pas interférer avec les contrôles du haut.
    if (e.type == sf::Event::MouseButtonReleased &&
        e.mouseButton.button == sf::Mouse::Left) {
        const float mx = static_cast<float>(e.mouseButton.x);
        const float my = static_cast<float>(e.mouseButton.y);

        // V1.1.9-batch : clic sur badge inbox → toggle modale.
        if (!state.webInbox.empty() &&
            inboxBadgeRect().contains(mx, my)) {
            controller_.toggleWebInboxModal();
            return;
        }
        // V1.1.10 : clic sur hamburger ☰ (Compact mode) → toggle sidebar.
        if (breakpoint_ == Breakpoint::Compact
            && hamburgerRect().contains(mx, my)) {
            compactSidebarOpen_ = !compactSidebarOpen_;
            rebuildLayout();
            return;
        }

        if (sidebarRect_.contains(mx, my) && my >= peerListStartY_) {
            const int idx = static_cast<int>(
                (my - peerListStartY_) / (kItemH + kItemGap));
            if (idx >= 0 && idx < static_cast<int>(state.peers.size())) {
                controller_.toggleSelectPeer(state.peers[idx].id);
            }
        }

        // V1.1.8-UX2 : clics zone TRANSFERTS — flèches L/R + boutons card.
        if (bottomRect_.contains(mx, my)) {
            // Flèches scroll L/R (visibles uniquement si débordement).
            const float zoneW = transfersZoneRight() - transfersZoneLeft();
            const bool overflow = transfersContentW_ > zoneW;
            if (overflow) {
                if (transfersArrowL().contains(mx, my)
                    && transfersScrollX_ > 0.f) {
                    transfersScrollX_ = std::max(0.f,
                        transfersScrollX_ - (340.f + Spacing::md));
                    return;
                }
                if (transfersArrowR().contains(mx, my)) {
                    const float maxScroll =
                        std::max(0.f, transfersContentW_ - zoneW);
                    transfersScrollX_ = std::min(maxScroll,
                        transfersScrollX_ + (340.f + Spacing::md));
                    return;
                }
            }

            // V1.1.9 : bouton global « Reprendre tout ».
            int resumableCount = 0;
            for (const auto& t : state.transfers) {
                if (t.status == domain::TransferStatus::Failed && t.resumable) {
                    ++resumableCount;
                }
            }
            if (resumableCount > 0 &&
                transfersResumeAllRect().contains(mx, my)) {
                controller_.resumeAllTransfers();
                return;
            }

            // Boutons contextuels card.
            for (std::size_t i = 0; i < state.transfers.size(); ++i) {
                const auto& t = state.transfers[i];
                const bool isCancellable =
                    t.status == domain::TransferStatus::Proposed ||
                    t.status == domain::TransferStatus::Accepted ||
                    t.status == domain::TransferStatus::InProgress ||
                    t.status == domain::TransferStatus::WaitingAcceptance;
                const bool canOpenFolder =
                    t.status == domain::TransferStatus::Done &&
                    t.direction == app::TransferDirection::Incoming;
                const bool isResumable =
                    t.status == domain::TransferStatus::Failed && t.resumable;

                if (isCancellable) {
                    if (transfersTopRightBtnRect(i, 90.f).contains(mx, my)) {
                        if (t.status == domain::TransferStatus::Proposed) {
                            controller_.cancelPending(t.sessionId);
                        } else {
                            controller_.cancelSession(t.sessionId);
                        }
                        return;
                    }
                } else if (canOpenFolder) {
                    if (transfersTopRightBtnRect(i, 140.f).contains(mx, my)) {
                        controller_.openDownloadDir();
                        return;
                    }
                } else if (isResumable) {
                    if (transfersResumeBtnRect(i).contains(mx, my)) {
                        controller_.resumeTransfer(t.sessionId);
                        return;
                    }
                    if (transfersIgnoreBtnRect(i).contains(mx, my)) {
                        controller_.ignoreTransfer(t.sessionId);
                        return;
                    }
                }
            }
        }

        // V1.1 : clics sur les checkboxes des fichiers (zone centre).
        // V1.1.5 : tient compte du scroll vertical actuel.
        if (centerRect_.contains(mx, my)) {
            const float listX = centerRect_.left + Spacing::xl;
            const float listY = centerRect_.top  + Spacing::xl + 64.f;
            const float fy0   = listY + Spacing::md;

            const float rowStride = kFileRowH + 6.f;
            // Coordonnée "contenu" = y visible + scroll actuel.
            const float contentY = my - fy0 + filesScrollY_;
            const int idx = static_cast<int>(contentY / rowStride);
            if (idx >= 0 && idx < static_cast<int>(state.selectedFiles.size())) {
                const float rowX = listX + Spacing::md + Spacing::md;
                if (mx >= rowX - 4.f && mx <= rowX + 20.f + 4.f) {
                    controller_.toggleFileCheck(
                        state.selectedFiles[idx].absolutePath);
                }
            }
        }
    }
}

void MainScreen::update(const app::AppState& state, sf::Time dt) {
    sendBtn_.setEnabled(controller_.canSend());
    rescanBtn_.setLabel(controller_.isScanning() ? "Scan…" : "Rescanner");

    // V1.1.8-UX1 : tick pour l'animation radar.
    emptyElapsed_ += dt.asSeconds();
    if (!state.peers.empty()) hasEverSeenPeer_ = true;

    // Synchronise la SharePanel avec les infos du WebService. Le QR n'est
    // regénéré que si l'URL a changé (évite de recompiler à chaque frame).
    const auto info = controller_.webShareInfo();
    if (info.url != lastShareUrlApplied_) {
        sharePanel_.setUrl(info.url);
        if (!info.url.empty()) {
            sharePanel_.setQrImage(web::QrCode::render(info.url, 240));
        }
        lastShareUrlApplied_ = info.url;
    }
    if (info.pin != lastSharePinApplied_) {
        sharePanel_.setPin(info.pin);
        lastSharePinApplied_ = info.pin;
    }

    // V1.1.8-UX4 : visitor count (pairs kind==Web) + collapsed state sync.
    int webPeers = 0;
    for (const auto& p : state.peers) {
        if (p.kind == domain::PeerKind::Web) ++webPeers;
    }
    sharePanel_.setVisitorCount(webPeers);
    sharePanel_.setCollapsed(controller_.isSharePanelCollapsed());

    // V1.1.9-batch : fade du badge inbox via state non-const du controller.
    auto& mst = controller_.state();
    if (mst.webInbox.empty()) mst.webInboxFadeSec += dt.asSeconds();
    else                       mst.webInboxFadeSec = 0.f;
}

void MainScreen::draw(sf::RenderTarget& target) const {
    drawBackground(target);

    // V1.1.10 — ClipScope par zone garantit qu'aucun dessin ne déborde.
    {
        ClipScope clip(target, headerRect_);
        drawHeader(target);
    }
    if (sidebarRect_.width > 0.f) {
        ClipScope clip(target, sidebarRect_);
        drawSidebar(target);
    }
    {
        ClipScope clip(target, centerRect_);
        drawCenter(target);
    }
    {
        ClipScope clip(target, sharePanelRect_);
        sharePanel_.draw(target);
    }
    {
        ClipScope clip(target, bottomRect_);
        drawTransferBar(target);
    }

    // V1.1.8-UX1 : le menu est dessiné en DERNIER pour passer par-dessus tout
    // (PAS de ClipScope — il doit pouvoir déborder de la zone centrale).
    addMenu_.draw(target);
}

void MainScreen::drawBackground(sf::RenderTarget& target) const {
    Card{}.setBounds({0.f, 0.f,
            static_cast<float>(viewSize_.x),
            static_cast<float>(viewSize_.y)})
        .setColor(Colors::bg).draw(target);
}

void MainScreen::drawHeader(sf::RenderTarget& target) const {
    const auto& st = controller_.state();

    Card{}.setBounds(headerRect_).setColor(Colors::surface).draw(target);

    // V1.1.10 : bouton ☰ hamburger en Compact mode (à gauche, avant le titre).
    float titleX = Spacing::xl;
    if (breakpoint_ == Breakpoint::Compact) {
        const auto h = hamburgerRect();
        RoundedRect bg(h.left, h.top, h.width, h.height, Radius::sm);
        bg.setFillColor(compactSidebarOpen_ ? Colors::accentLight
                                                : Colors::surface);
        bg.draw(target);
        // 3 traits horizontaux pour le pictogramme ☰
        for (int i = 0; i < 3; ++i) {
            RoundedRect bar(h.left + 8.f, h.top + 9.f + i * 6.f,
                            h.width - 16.f, 2.f, 1.f);
            bar.setFillColor(Colors::text);
            bar.draw(target);
        }
        titleX = h.left + h.width + Spacing::md + 12.f;
    } else {
        // Point indigo décoratif (mode Regular/Large)
        RoundedRect dot(Spacing::xl, (kHeaderH - 10) / 2.f, 10.f, 10.f, 5.f);
        dot.setFillColor(Colors::accent).draw(target);
    }

    Label title;
    title.setText("LocalTransfer")
         .setBold(true).setSize(FontSize::h1)
         .setColor(Colors::text)
         .setPosition(titleX + (breakpoint_ == Breakpoint::Compact ? 0.f : 22.f),
                       (kHeaderH - FontSize::h1) / 2.f - 4.f);
    title.draw(target);

    // Pill indiquant l'appareil courant (droite).
    Label selfName;
    selfName.setText(st.self.name)
            .setSize(FontSize::small)
            .setBold(true)
            .setColor(Colors::accent);
    const auto m = selfName.measure();
    const float pillW = m.x + 2 * Spacing::md;
    const float pillH = 28.f;
    const float pillX = viewSize_.x - pillW - Spacing::xl;
    const float pillY = (kHeaderH - pillH) / 2.f;
    RoundedRect pill(pillX, pillY, pillW, pillH, pillH / 2.f);
    pill.setFillColor(Colors::accentLight).draw(target);
    selfName.setPosition(pillX + Spacing::md, pillY + 7.f);
    selfName.draw(target);

    // V1.1.9-batch : badge inbox proéminent à gauche de la pill self.
    // Plus grand (FontSize::body) + couleur accent qui ressort sur
    // fond surface clair. Cliquable → ouvre la modale.
    const int inboxCount = static_cast<int>(st.webInbox.size());
    const bool fading = inboxCount == 0 && st.webInboxFadeSec < 3.f
                         && st.webInboxFadeSec > 0.f;
    if (inboxCount > 0 || fading) {
        float alpha = 1.f;
        if (fading) alpha = std::max(0.f, 1.f - st.webInboxFadeSec / 3.f);

        const std::string text = (inboxCount > 0)
            ? (std::to_string(inboxCount) + " demande"
               + (inboxCount > 1 ? "s" : "") + " entrante"
               + (inboxCount > 1 ? "s" : ""))
            : "\xE2\x9C\x93 Tout trait\xC3\xA9";

        Label lbl;
        lbl.setText(text).setBold(true).setSize(FontSize::body);

        sf::Color textCol = sf::Color::White;
        textCol.a = static_cast<sf::Uint8>(alpha * 255);
        lbl.setColor(textCol);

        const auto bm = lbl.measure();
        const float bpillW = bm.x + 2 * Spacing::lg;
        const float bpillX = pillX - bpillW - Spacing::md;
        const float bpillY = pillY;

        RoundedRect bpill(bpillX, bpillY, bpillW, pillH, pillH / 2.f);
        sf::Color bg = (inboxCount > 0) ? Colors::accent : Colors::success;
        bg.a = static_cast<sf::Uint8>(alpha * 255);
        bpill.setFillColor(bg).draw(target);

        lbl.setPosition(bpillX + Spacing::lg, bpillY + 6.f);
        lbl.draw(target);
    }

    // Séparateur fin sous le header.
    Card{}.setBounds({0, kHeaderH, static_cast<float>(viewSize_.x), 1.f})
        .setColor(Colors::separator).draw(target);
}

void MainScreen::drawSidebar(sf::RenderTarget& target) const {
    const auto& st = controller_.state();

    Card{}.setBounds(sidebarRect_).setColor(Colors::sidebar).draw(target);

    // ----- Zone "Rechercher" (haut de sidebar)
    Label searchHeader;
    searchHeader.setText("RECHERCHER")
                .setBold(true).setSize(FontSize::overline)
                .setColor(Colors::textSecondary)
                .setPosition(sidebarRect_.left + Spacing::md,
                             sidebarRect_.top  + Spacing::md);
    searchHeader.draw(target);

    rescanBtn_.draw(target);
    ipInput_.draw(target);
    addIpBtn_.draw(target);

    // Séparateur fin entre zone Rechercher et liste des pairs.
    const float sepY = sidebarRect_.top + kSearchH - 1.f;
    Card{}.setBounds({sidebarRect_.left + Spacing::md, sepY,
                      sidebarRect_.width - 2 * Spacing::md, 1.f})
        .setColor(Colors::separator).draw(target);

    // ----- Label "APPAREILS · N"
    Label section;
    section.setText("APPAREILS · " + std::to_string(st.peers.size()))
           .setBold(true).setSize(FontSize::overline)
           .setColor(Colors::textSecondary)
           .setPosition(sidebarRect_.left + Spacing::md,
                        sidebarRect_.top  + kSearchH + Spacing::md);
    section.draw(target);

    if (st.peers.empty()) {
        // V1.1.8-UX1 : empty state 2 états —
        //   searching = radar pulsant (accent) pendant 5 s après lancement/rescan
        //   sinon     = globe barré (textSecondary) statique
        const float zoneTop    = sidebarRect_.top + kSearchH;
        const float zoneHeight = sidebarRect_.height - kSearchH;
        const float cx = sidebarRect_.left + sidebarRect_.width / 2.f;
        const float cy = zoneTop + zoneHeight / 2.f - 20.f;

        const bool searching = emptyElapsed_ < 5.f && !hasEverSeenPeer_;

        constexpr float kEmptyIconBase = 48.f;
        const float iconSize = searching
            ? kEmptyIconBase + std::sin(emptyElapsed_ * 5.f) * 4.f
            : kEmptyIconBase;

        const auto iconId = searching
            ? IconLibrary::Id::Radar
            : IconLibrary::Id::NoDevice;

        sf::Sprite ico(IconLibrary::get(iconId));
        const float scale = iconSize / kEmptyIconBase;
        ico.setScale(scale, scale);
        ico.setColor(searching ? Colors::accent : Colors::textSecondary);
        ico.setPosition(cx - iconSize / 2.f, cy - iconSize / 2.f);
        target.draw(ico);

        Label empty;
        empty.setText(searching ? "Recherche..."
                                : "Aucun appareil détecté")
             .setSize(FontSize::body)
             .setColor(Colors::textSecondary);
        const auto em = empty.measure();
        empty.setPosition(cx - em.x / 2.f, cy + kEmptyIconBase / 2.f + 10.f);
        empty.draw(target);
        return;
    }

    float y = peerListStartY_;
    for (const auto& d : st.peers) {
        DeviceListItem item;
        item.setBounds({sidebarRect_.left, y, sidebarRect_.width, kItemH})
            .setDevice(d)
            .setSelected(st.selectedPeerIds.count(d.id) > 0);
        item.draw(target);
        y += kItemH + kItemGap;
    }
}

void MainScreen::drawCenter(sf::RenderTarget& target) const {
    const auto& st = controller_.state();
    Card{}.setBounds(centerRect_).setColor(Colors::bg).draw(target);

    // ----- Titre dynamique
    const std::string targetLabel =
        st.selectedPeerIds.empty() ?
            "Sélectionnez un appareil" :
            (st.selectedPeerIds.size() == 1
                ? "Envoyer à 1 appareil"
                : "Envoyer à " +
                   std::to_string(st.selectedPeerIds.size()) + " appareils");

    Label h;
    h.setText(targetLabel).setBold(true).setSize(FontSize::h1)
     .setColor(Colors::text)
     .setPosition(centerRect_.left + Spacing::xl,
                  centerRect_.top  + Spacing::xl);
    h.draw(target);

    Label sub;
    sub.setText("FICHIERS À ENVOYER")
       .setSize(FontSize::overline).setBold(true)
       .setColor(Colors::textSecondary)
       .setPosition(centerRect_.left + Spacing::xl,
                    centerRect_.top  + Spacing::xl + 40.f);
    sub.draw(target);

    // ----- Zone liste fichiers
    const float listX = centerRect_.left + Spacing::xl;
    const float listY = centerRect_.top  + Spacing::xl + 64.f;
    const float listW = centerRect_.width - 2 * Spacing::xl;
    const float listH = centerRect_.height - (listY - centerRect_.top) - 112.f;

    RoundedRect listBg(listX, listY, listW, listH, Radius::md);
    listBg.setFillColor(Colors::surface)
          .setOutline(Colors::separator, 1.f);
    listBg.draw(target);

    if (st.selectedFiles.empty()) {
        Label empty;
        empty.setText("Aucun fichier sélectionné")
             .setBold(true).setSize(FontSize::body)
             .setColor(Colors::text);
        const auto em = empty.measure();
        empty.setPosition(listX + (listW - em.x) / 2.f,
                          listY + listH / 2.f - 20.f);
        empty.draw(target);

        Label hint;
        hint.setText("Cliquez sur « Ajouter » pour choisir des fichiers ou un dossier")
            .setSize(FontSize::small)
            .setColor(Colors::textSecondary);
        const auto hm = hint.measure();
        hint.setPosition(listX + (listW - hm.x) / 2.f,
                         listY + listH / 2.f + 4.f);
        hint.draw(target);
    } else {
        // V1.1.5 : scroll vertical. On applique un décalage Y, on clip en
        // masquant (skip) les rows hors-zone visible, et on mémorise la
        // hauteur totale pour clamper le scroll au handleEvent suivant.
        const float fx = listX + Spacing::md;
        const float fw = listW - 2 * Spacing::md;
        const float rowStride = kFileRowH + 6.f;
        const float fy0 = listY + Spacing::md;
        const float visibleTop = listY + Spacing::md;
        const float maxY = listY + listH - Spacing::md;
        const float scroll = filesScrollY_;

        // Met à jour la hauteur totale pour le clamp.
        filesContentHeight_ =
            static_cast<float>(st.selectedFiles.size()) * rowStride;

        float fy = fy0 - scroll;

        for (const auto& sf : st.selectedFiles) {
            // Skip rows entièrement hors-zone.
            if (fy + kFileRowH < visibleTop) { fy += rowStride; continue; }
            if (fy > maxY) break;
            FileRow r;
            const auto absPath = sf.absolutePath;
            const bool isFolder =
                (sf.kind == app::SelectedFile::Kind::Folder);
            r.setBounds({fx, fy, fw, kFileRowH})
             .setName(sf.displayName.empty()
                      ? absPath.filename().string()
                      : sf.displayName)
             .setSize(sf.size)
             .setKind(isFolder ? FileRow::Kind::Folder
                               : FileRow::Kind::File)
             .setFileCount(sf.fileCount)
             .setChecked(sf.checked)
             .onToggle([this, absPath](bool /*newState*/) {
                 controller_.toggleFileCheck(absPath);
             });
            r.draw(target);
            fy += rowStride;
        }

        // V1.1.5 : mini scrollbar à droite si débordement.
        if (filesContentHeight_ > listH) {
            const float barTrackX = listX + listW - 5.f;
            const float barTrackY = listY + Spacing::md;
            const float barTrackH = listH - 2 * Spacing::md;
            RoundedRect track(barTrackX, barTrackY, 3.f, barTrackH, 1.5f);
            track.setFillColor(Colors::separator).draw(target);

            const float thumbH = std::max(
                30.f, barTrackH * (listH / filesContentHeight_));
            const float thumbY = barTrackY +
                (barTrackH - thumbH) *
                (scroll / std::max(1.f, filesContentHeight_ - listH));
            RoundedRect thumb(barTrackX, thumbY, 3.f, thumbH, 1.5f);
            thumb.setFillColor(Colors::textSecondary).draw(target);
        }
    }

    // ----- Ligne récap (uniquement les cochés comptent pour l'envoi)
    Label total;
    total.setText("À envoyer : " + formatBytes(st.selectedFilesCheckedTotal) +
                  "  ·  " + std::to_string(st.selectedFilesCheckedCount) +
                  " fichier(s)")
         .setSize(FontSize::small)
         .setColor(Colors::textSecondary)
         .setPosition(centerRect_.left + Spacing::xl,
                      centerRect_.top  + centerRect_.height - 64.f - 22.f);
    total.draw(target);

    addBtn_.draw(target);
    clearFilesBtn_.draw(target);
    sendBtn_.draw(target);

    // V1.1.8-UX3 : highlight drag OS en cours au-dessus de la fenêtre.
    if (dragOver_) {
        RoundedRect overlay(
            centerRect_.left + 4.f, centerRect_.top + 4.f,
            centerRect_.width - 8.f, centerRect_.height - 8.f,
            Radius::md);
        sf::Color fill = Colors::accentLight;
        fill.a = 40;
        overlay.setFillColor(fill)
               .setOutline(Colors::accent, 2.f);
        overlay.draw(target);

        Label drop;
        drop.setText("D\xC3\xA9poser pour ajouter")
            .setBold(true).setSize(FontSize::h1)
            .setColor(Colors::accent);
        const auto m = drop.measure();
        drop.setPosition(
            centerRect_.left + (centerRect_.width  - m.x) / 2.f,
            centerRect_.top  + (centerRect_.height - m.y) / 2.f - 20.f);
        drop.draw(target);
    }
}

namespace {

// V1.1.8-UX2 : dimensions constantes pour garder draw et hit-test en
// phase (main_screen_click_handler_ les réutilise).
constexpr float kTCardW    = 340.f;
constexpr float kTCardH    = 64.f;
constexpr float kTCardGap  = Spacing::md;
constexpr float kTArrowNav = 22.f;  // boutons ◀ / ▶ scroll
constexpr float kTBtnH     = 22.f;
constexpr float kTBtnCancelW = 90.f;
constexpr float kTBtnOpenW   = 140.f;
// V1.1.9 — Sprint Transfer Resume : 2 boutons stackés vertical top-right
// pour les cards Failed resumable (Reprendre en haut, Ignorer en bas).
constexpr float kTBtnResumeW = 90.f;
constexpr float kTBtnIgnoreW = 90.f;
// Bouton global « Reprendre tout » dans le header TRANSFERTS.
constexpr float kTBtnResumeAllW = 140.f;

} // namespace

float MainScreen::sharePanelWidth() const {
    const auto m = metricsFor(breakpoint_);
    // V1.1.10 : Compact force le SharePanel à l'état collapsed
    // (économie d'espace écran).
    const bool collapsed = controller_.isSharePanelCollapsed()
                            || m.forceSharePanelCollapsed;
    return collapsed ? kSharePanelCollapsedW : m.sharePanelExpandedW;
}

float MainScreen::transfersZoneLeft() const {
    return bottomRect_.left + Spacing::xl;
}
float MainScreen::transfersZoneRight() const {
    return bottomRect_.left + bottomRect_.width - Spacing::xl;
}
float MainScreen::transfersCardY() const {
    return bottomRect_.top + 34.f;
}

sf::FloatRect MainScreen::transfersCardRect(std::size_t i) const {
    const float x = transfersZoneLeft()
                  + static_cast<float>(i) * (kTCardW + kTCardGap)
                  - transfersScrollX_;
    return {x, transfersCardY(), kTCardW, kTCardH};
}

sf::FloatRect MainScreen::transfersTopRightBtnRect(std::size_t i,
                                                    float btnW) const {
    const auto r = transfersCardRect(i);
    return {r.left + r.width - btnW - Spacing::md,
            r.top + Spacing::sm,
            btnW, kTBtnH};
}

sf::FloatRect MainScreen::transfersArrowL() const {
    return {transfersZoneRight() - 2 * (kTArrowNav + Spacing::sm),
            bottomRect_.top + Spacing::md - 2.f,
            kTArrowNav, kTArrowNav};
}
sf::FloatRect MainScreen::transfersArrowR() const {
    return {transfersZoneRight() - (kTArrowNav),
            bottomRect_.top + Spacing::md - 2.f,
            kTArrowNav, kTArrowNav};
}

// V1.1.9 : 2 boutons stackés vertical pour cards Failed resumable.
sf::FloatRect MainScreen::transfersResumeBtnRect(std::size_t i) const {
    const auto r = transfersCardRect(i);
    return {r.left + r.width - kTBtnResumeW - Spacing::md,
            r.top + Spacing::xs,
            kTBtnResumeW, kTBtnH};
}
sf::FloatRect MainScreen::transfersIgnoreBtnRect(std::size_t i) const {
    const auto r = transfersCardRect(i);
    return {r.left + r.width - kTBtnIgnoreW - Spacing::md,
            r.top + Spacing::xs + kTBtnH + Spacing::xs,
            kTBtnIgnoreW, kTBtnH};
}
sf::FloatRect MainScreen::hamburgerRect() const {
    // Bouton ☰ visible uniquement en Compact mode, à gauche du titre.
    constexpr float kSize = 32.f;
    return {Spacing::xl, (kHeaderH - kSize) / 2.f, kSize, kSize};
}

sf::FloatRect MainScreen::inboxBadgeRect() const {
    // Approximation grossière : zone à gauche de la pill selfName de
    // ~150 px. Le rendu réel calcule la largeur précise selon le label.
    const auto& st = controller_.state();
    Label selfName;
    selfName.setText(st.self.name)
            .setSize(FontSize::small).setBold(true);
    const auto m = selfName.measure();
    const float selfPillW = m.x + 2 * Spacing::md;
    const float selfPillX = viewSize_.x - selfPillW - Spacing::xl;
    const float pillH = 28.f;
    const float pillY = (kHeaderH - pillH) / 2.f;
    // Badge à gauche, on suppose une largeur max de ~160 px
    constexpr float kBadgeMaxW = 160.f;
    return {selfPillX - kBadgeMaxW - Spacing::sm, pillY,
            kBadgeMaxW, pillH};
}

sf::FloatRect MainScreen::transfersResumeAllRect() const {
    // Positionné avant les flèches L/R dans le header (ou seul si pas de scroll).
    const float y = bottomRect_.top + Spacing::md - 4.f;
    const float rightAnchor = transfersZoneRight()
        - 2 * (kTArrowNav + Spacing::sm) - Spacing::md;
    return {rightAnchor - kTBtnResumeAllW, y,
            kTBtnResumeAllW, kTBtnH + 4.f};
}

void MainScreen::drawTransferBar(sf::RenderTarget& target) const {
    const auto& st = controller_.state();

    Card{}.setBounds({0, bottomRect_.top,
            static_cast<float>(viewSize_.x), 1.f})
        .setColor(Colors::separator).draw(target);

    Card{}.setBounds(bottomRect_).setColor(Colors::surface).draw(target);

    Label transfersH;
    transfersH.setText("TRANSFERTS · " +
                std::to_string(st.transfers.size()))
              .setBold(true).setSize(FontSize::overline)
              .setColor(Colors::textSecondary)
              .setPosition(transfersZoneLeft(),
                           bottomRect_.top + Spacing::md);
    transfersH.draw(target);

    // V1.1.9 — bouton « Reprendre tout » global si ≥1 card resumable.
    int resumableCount = 0;
    for (const auto& t : st.transfers) {
        if (t.status == domain::TransferStatus::Failed && t.resumable) {
            ++resumableCount;
        }
    }
    if (resumableCount > 0) {
        const auto rb = transfersResumeAllRect();
        RoundedRect bg(rb.left, rb.top, rb.width, rb.height, Radius::sm);
        bg.setFillColor(Colors::accent);
        bg.draw(target);
        Label lbl;
        lbl.setText("Reprendre tout (" + std::to_string(resumableCount) + ")")
           .setSize(FontSize::overline).setBold(true)
           .setColor(sf::Color::White);
        const auto m = lbl.measure();
        lbl.setPosition(rb.left + (rb.width - m.x) / 2.f,
                        rb.top  + (rb.height - m.y) / 2.f - 2.f);
        lbl.draw(target);
    }

    if (st.transfers.empty()) {
        Label hint;
        hint.setText("Aucun transfert en cours.")
            .setSize(FontSize::small)
            .setColor(Colors::textSecondary)
            .setPosition(transfersZoneLeft(), bottomRect_.top + 50.f);
        hint.draw(target);
        transfersContentW_ = 0.f;
        return;
    }

    // Largeur totale du contenu (pour clamp scroll côté handleEvent).
    transfersContentW_ =
        static_cast<float>(st.transfers.size()) * (kTCardW + kTCardGap);
    const float zoneW = transfersZoneRight() - transfersZoneLeft();
    const bool overflow = transfersContentW_ > zoneW;

    // Flèches L/R de scroll (header, à droite).
    if (overflow) {
        const auto drawArrow = [&](const sf::FloatRect& r, bool left,
                                    bool enabled) {
            RoundedRect bg(r.left, r.top, r.width, r.height, Radius::sm);
            bg.setFillColor(enabled ? Colors::accentLight : Colors::sidebar);
            bg.draw(target);

            sf::Sprite a(IconLibrary::get(IconLibrary::Id::ArrowUp));
            const auto lb = a.getLocalBounds();
            a.setOrigin(lb.width / 2.f, lb.height / 2.f);
            a.setRotation(left ? -90.f : 90.f);
            a.setPosition(r.left + r.width / 2.f, r.top + r.height / 2.f);
            a.setColor(enabled ? Colors::accent : Colors::textSecondary);
            target.draw(a);
        };
        const bool canLeft  = transfersScrollX_ > 0.f;
        const bool canRight = transfersScrollX_ < transfersContentW_ - zoneW;
        drawArrow(transfersArrowL(), /*left*/ true,  canLeft);
        drawArrow(transfersArrowR(), /*left*/ false, canRight);
    }

    const float cardY = transfersCardY();

    for (std::size_t i = 0; i < st.transfers.size(); ++i) {
        const auto& t = st.transfers[i];
        const auto r = transfersCardRect(i);

        // Skip les cards entièrement hors-zone visible.
        if (r.left + r.width < transfersZoneLeft() ||
            r.left > transfersZoneRight()) continue;

        RoundedRect tile(r.left, r.top, r.width, r.height, Radius::md);
        tile.setFillColor(Colors::sidebar).draw(target);

        // Icône directionnelle (V1.1.8-UX1 hérité).
        constexpr float kArrowSize = 18.f;
        const bool outgoing =
            (t.direction == app::TransferDirection::Outgoing);
        sf::Sprite arrow(IconLibrary::get(
            outgoing ? IconLibrary::Id::ArrowUp
                     : IconLibrary::Id::ArrowDown));
        arrow.setColor(outgoing ? Colors::accent : Colors::success);
        arrow.setPosition(r.left + Spacing::md,
                          r.top + Spacing::sm
                          + (FontSize::body - kArrowSize) / 2.f);
        target.draw(arrow);

        // Nom du peer.
        Label l1;
        l1.setText(t.peerName)
          .setSize(FontSize::body).setBold(true)
          .setColor(Colors::text)
          .setPosition(r.left + Spacing::md + kArrowSize + Spacing::sm,
                       r.top + Spacing::sm);
        l1.draw(target);

        // Progression.
        const double prog = (t.totalBytes > 0)
            ? (static_cast<double>(t.bytesTransferred)
               / static_cast<double>(t.totalBytes))
            : 0.0;

        // Ligne 2 : hiérarchie % / speed/eta selon status.
        const float l2Y = r.top + cardY + 30.f - cardY;
        switch (t.status) {
            case domain::TransferStatus::Done: {
                Label done;
                done.setText("\xE2\x9C\x93 Termin\xC3\xA9")  // ✓ Terminé
                    .setSize(FontSize::body).setBold(true)
                    .setColor(Colors::success)
                    .setPosition(r.left + Spacing::md, l2Y);
                done.draw(target);
                break;
            }
            case domain::TransferStatus::Failed: {
                Label f;
                f.setText("\xC3\x89" "chec : " + t.error) // « Échec »
                 .setSize(FontSize::small)
                 .setColor(Colors::error)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            case domain::TransferStatus::Rejected: {
                Label f;
                f.setText("Refus\xC3\xA9")
                 .setSize(FontSize::small).setBold(true)
                 .setColor(Colors::error)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            case domain::TransferStatus::Cancelled: {
                Label f;
                f.setText("Annul\xC3\xA9")
                 .setSize(FontSize::small).setBold(true)
                 .setColor(Colors::textSecondary)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            case domain::TransferStatus::Expired: {
                Label f;
                f.setText("Expir\xC3\xA9")
                 .setSize(FontSize::small).setBold(true)
                 .setColor(Colors::error)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            case domain::TransferStatus::WaitingAcceptance: {
                Label f;
                f.setText("En attente...")
                 .setSize(FontSize::small)
                 .setColor(Colors::textSecondary)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            case domain::TransferStatus::Proposed: {
                Label f;
                f.setText("En attente du visiteur\xE2\x80\xA6") // …
                 .setSize(FontSize::small)
                 .setColor(Colors::warning)
                 .setPosition(r.left + Spacing::md, l2Y);
                f.draw(target);
                break;
            }
            default: {
                // InProgress / Accepted : % en grand + speed/eta en petit
                Label pct;
                pct.setText(std::to_string(static_cast<int>(prog * 100.0))
                            + " %")
                   .setSize(FontSize::h2).setBold(true)
                   .setColor(Colors::text)
                   .setPosition(r.left + Spacing::md, r.top + 28.f);
                pct.draw(target);
                const auto pctM = pct.measure();

                std::ostringstream sub;
                sub << formatSpeed(t.speedBps) << " \xC2\xB7 "
                    << formatEta(t.eta);
                Label su;
                su.setText(sub.str())
                  .setSize(FontSize::small)
                  .setColor(Colors::textSecondary)
                  .setPosition(r.left + Spacing::md + pctM.x + Spacing::md,
                               r.top + 28.f + (pctM.y - FontSize::small) - 2.f);
                su.draw(target);
                break;
            }
        }

        // Boutons contextuels top-right.
        const bool isCancellable =
            t.status == domain::TransferStatus::Proposed ||
            t.status == domain::TransferStatus::Accepted ||
            t.status == domain::TransferStatus::InProgress ||
            t.status == domain::TransferStatus::WaitingAcceptance;
        const bool canOpenFolder =
            t.status == domain::TransferStatus::Done &&
            t.direction == app::TransferDirection::Incoming;
        // V1.1.9 : Failed resumable → 2 boutons stackés (Reprendre + Ignorer).
        const bool isResumable =
            t.status == domain::TransferStatus::Failed && t.resumable;

        auto drawBtn = [&](const sf::FloatRect& b, const std::string& label,
                           sf::Color bgColor, sf::Color textColor,
                           bool withOutline) {
            RoundedRect bg(b.left, b.top, b.width, b.height, Radius::sm);
            bg.setFillColor(bgColor);
            if (withOutline) bg.setOutline(Colors::separator, 1.f);
            bg.draw(target);
            Label lbl;
            lbl.setText(label)
               .setSize(FontSize::overline).setBold(true)
               .setColor(textColor);
            const auto m = lbl.measure();
            lbl.setPosition(b.left + (b.width - m.x) / 2.f,
                            b.top  + (b.height - m.y) / 2.f - 2.f);
            lbl.draw(target);
        };

        if (isCancellable) {
            drawBtn(transfersTopRightBtnRect(i, kTBtnCancelW), "Annuler",
                    Colors::accentLight, Colors::accent, false);
        } else if (canOpenFolder) {
            drawBtn(transfersTopRightBtnRect(i, kTBtnOpenW),
                    "Ouvrir le dossier",
                    Colors::accent, sf::Color::White, false);
        } else if (isResumable) {
            drawBtn(transfersResumeBtnRect(i), "Reprendre",
                    Colors::accent, sf::Color::White, false);
            drawBtn(transfersIgnoreBtnRect(i), "Ignorer",
                    Colors::surface, Colors::textSecondary, true);
        }

        // Progression 6 px.
        ProgressBar pb;
        pb.setBounds({r.left + Spacing::md,
                      r.top + r.height - 8.f,
                      r.width - 2 * Spacing::md, 6.f});
        pb.setProgress(prog);
        switch (t.status) {
            case domain::TransferStatus::Done:
                pb.setColor(Colors::success); break;
            case domain::TransferStatus::Failed:
            case domain::TransferStatus::Rejected:
                pb.setColor(Colors::error); break;
            case domain::TransferStatus::Proposed:
                pb.setColor(Colors::warning); break;
            case domain::TransferStatus::Expired:
            case domain::TransferStatus::Cancelled:
                pb.setColor(Colors::separator); break;
            default:
                pb.setColor(Colors::accent); break;
        }
        pb.draw(target);
    }
}

} // namespace ltr::ui
