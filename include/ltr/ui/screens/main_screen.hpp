#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include "ltr/app/app_controller.hpp"
#include "ltr/ui/breakpoint.hpp"
#include "ltr/ui/screen.hpp"
#include "ltr/ui/widgets/button.hpp"
#include "ltr/ui/widgets/dropdown_menu.hpp"
#include "ltr/ui/widgets/scroll_area.hpp"
#include "ltr/ui/widgets/share_panel.hpp"
#include "ltr/ui/widgets/text_input.hpp"

namespace ltr::ui {

class MainScreen : public Screen {
public:
    explicit MainScreen(app::AppController& controller);

    void handleEvent(const sf::Event& e, const app::AppState& state) override;
    void update(const app::AppState& state, sf::Time dt) override;
    void draw(sf::RenderTarget& target) const override;

    void setViewSize(sf::Vector2u size);

    // V1.1.8-UX3 : toggle le highlight visuel de la zone centrale
    // pendant un drag OS au-dessus de la fenêtre.
    void setDragOver(bool on) { dragOver_ = on; }

private:
    void rebuildLayout();

    // Actions déclenchées depuis l'UI.
    void openFilesPicker();    // V1.1.4 : fichiers uniquement
    void openFolderPicker();   // V1.1.4 : dossier uniquement

    // Helpers de rendu — un par grande zone (cf. ui-proposal.md §6).
    void drawBackground(sf::RenderTarget& t) const;
    void drawHeader    (sf::RenderTarget& t) const;
    void drawSidebar   (sf::RenderTarget& t) const;
    void drawCenter    (sf::RenderTarget& t) const;
    void drawTransferBar(sf::RenderTarget& t) const;

    // V1.1.8-UX4 : largeur dynamique du SharePanel (40 collapsed / 240 expanded).
    float sharePanelWidth() const;

    // V1.1.8-UX2 : helpers de géométrie pour la zone Transferts. Utilisés
    // à la fois par drawTransferBar (const) et par handleEvent (clics).
    float transfersZoneLeft() const;
    float transfersZoneRight() const;
    float transfersCardY() const;
    sf::FloatRect transfersCardRect(std::size_t i) const;
    sf::FloatRect transfersTopRightBtnRect(std::size_t i, float btnW) const;
    sf::FloatRect transfersArrowL() const;
    sf::FloatRect transfersArrowR() const;
    // V1.1.9 : 2 boutons stackés vertical (Reprendre top, Ignorer bottom)
    sf::FloatRect transfersResumeBtnRect(std::size_t i) const;
    sf::FloatRect transfersIgnoreBtnRect(std::size_t i) const;
    // V1.1.9 : bouton global « Reprendre tout » dans le header TRANSFERTS
    sf::FloatRect transfersResumeAllRect() const;
    // V1.1.9-batch : badge inbox web dans le header (vide si count==0)
    sf::FloatRect inboxBadgeRect() const;
    // V1.1.10 : bouton hamburger ☰ dans le header (Compact mode uniquement)
    sf::FloatRect hamburgerRect() const;

    app::AppController& controller_;
    sf::Vector2u        viewSize_{960, 600};

    // Zones (mises à jour dans rebuildLayout).
    sf::FloatRect headerRect_;
    sf::FloatRect sidebarRect_;
    sf::FloatRect centerRect_;
    sf::FloatRect sharePanelRect_;
    sf::FloatRect bottomRect_;

    // Widgets persistants (positions recalculées à chaque frame).
    // V1.1.8-UX1 : bouton unique « Ajouter ▾ » + menu déroulant qui expose
    // les 2 choix (Fichiers / Dossier).
    Button       addBtn_;
    DropdownMenu addMenu_;
    Button sendBtn_;
    Button clearFilesBtn_;

    // Zone "Rechercher" en haut de sidebar.
    Button    rescanBtn_;
    Button    addIpBtn_;
    TextInput ipInput_;

    // Panneau de partage web (colonne droite).
    SharePanel sharePanel_;
    std::string lastSharePinApplied_;
    std::string lastShareUrlApplied_;
    std::string lastShareFpApplied_;   // V1.6.4 — Sprint Sécurité

    // Offset vertical du début de la liste des pairs (sous la zone Rechercher).
    float     peerListStartY_{0.f};

    // V1.1.5 : scroll vertical dans la liste "Fichiers à envoyer" (centre).
    float             filesScrollY_{0.f};
    // mis à jour depuis drawCenter (const) pour clamper au prochain event.
    mutable float     filesContentHeight_{0.f};

    // V1.1.8-UX1 : empty state sidebar 2 états (radar pulsant → statique).
    float emptyElapsed_{0.f};
    bool  hasEverSeenPeer_{false};

    // V1.1.8-UX2 : scroll horizontal de la zone TRANSFERTS.
    // Mutable pour que drawTransferBar (const) puisse mettre à jour la
    // largeur totale du contenu et le clamper au prochain event.
    float         transfersScrollX_{0.f};
    mutable float transfersContentW_{0.f};

    // V1.1.10 — Sprint UI Layout System
    Breakpoint    breakpoint_{Breakpoint::Regular};
    bool          compactSidebarOpen_{false};
    mutable ScrollArea peersScroll_;

    // V1.1.8-UX3 : drag OS en cours au-dessus de la fenêtre.
    bool          dragOver_{false};
};

} // namespace ltr::ui
