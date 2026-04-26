#pragma once

#include <functional>
#include <string>
#include <vector>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

// Menu popup minimaliste : liste d'items sous un rectangle d'ancrage.
// Visible seulement si open_ == true. Se ferme automatiquement au clic
// extérieur ou sur sélection. À dessiner EN DERNIER (par-dessus tout).
class DropdownMenu {
public:
    struct Item {
        std::string label;
        std::function<void()> action;
    };

    DropdownMenu& setItems(std::vector<Item> items);
    DropdownMenu& setAnchor(const sf::FloatRect& anchor);

    void openMenu();
    void close();
    bool isOpen() const noexcept { return open_; }

    // Return true si l'event a été consommé (ne pas propager).
    bool handleEvent(const sf::Event& e);

    void draw(sf::RenderTarget& target) const;

private:
    std::vector<Item> items_;
    sf::FloatRect anchor_{};
    bool open_{false};
    int  hoverIdx_{-1};
};

} // namespace ltr::ui
