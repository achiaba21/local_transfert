#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <vector>

#include "ltr/app/app_controller.hpp"
#include "ltr/ui/screen.hpp"
#include "ltr/ui/widgets/deposit_link_card.hpp"
#include "ltr/ui/widgets/scroll_area.hpp"

namespace ltr::ui {

// Écran desktop pour gérer les liens de dépôt (Phase 2).
// Implémente l'option B1 de ui-proposal.md :
//   - liste verticale de DepositLinkCard pleine largeur,
//   - header avec bouton "Nouveau lien",
//   - état vide + état Personal Free intégrés.
//
// La création/QR/révocation passent par AppController qui les délègue
// à DepositLinkService (couche infra).
class DepositLinksScreen : public Screen {
public:
    explicit DepositLinksScreen(app::AppController& controller);

    void handleEvent(const sf::Event& e, const app::AppState& state) override;
    void update(const app::AppState& state, sf::Time dt) override;
    void draw(sf::RenderTarget& target) const override;

    void setViewSize(sf::Vector2u size) { viewSize_ = size; }

private:
    void rebuildCards();

    app::AppController& controller_;
    sf::Vector2u        viewSize_{960, 600};
    std::vector<DepositLinkCard> cards_;
    mutable ScrollArea  scroll_;
};

} // namespace ltr::ui
