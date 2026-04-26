#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Event.hpp>

#include "ltr/app/app_state.hpp"

namespace ltr::ui {

class Screen {
public:
    virtual ~Screen();

    virtual void handleEvent(const sf::Event& e, const app::AppState& state) = 0;
    virtual void update(const app::AppState& state, sf::Time dt) = 0;
    virtual void draw(sf::RenderTarget& target) const = 0;
};

} // namespace ltr::ui
