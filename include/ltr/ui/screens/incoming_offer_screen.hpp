#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include "ltr/app/app_controller.hpp"
#include "ltr/ui/screen.hpp"
#include "ltr/ui/widgets/button.hpp"

namespace ltr::ui {

// Overlay modal affiché par-dessus le MainScreen quand une offre entre.
class IncomingOfferScreen : public Screen {
public:
    explicit IncomingOfferScreen(app::AppController& controller);

    void handleEvent(const sf::Event& e, const app::AppState& state) override;
    void update(const app::AppState& state, sf::Time dt) override;
    void draw(sf::RenderTarget& target) const override;

    void setViewSize(sf::Vector2u size);

private:
    void layout();

    app::AppController& controller_;
    sf::Vector2u        viewSize_{960, 600};

    sf::FloatRect cardRect_{};
    Button rejectBtn_;
    Button acceptBtn_;
};

} // namespace ltr::ui
