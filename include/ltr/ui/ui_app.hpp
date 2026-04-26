#pragma once

#include <memory>

#include <SFML/Graphics/RenderWindow.hpp>

#include "ltr/app/app_controller.hpp"
#include "ltr/ui/drag_drop.hpp"
#include "ltr/ui/screens/incoming_offer_screen.hpp"
#include "ltr/ui/screens/main_screen.hpp"

namespace ltr::ui {

class UIApp {
public:
    UIApp();

    int run();

private:
    void onResize(unsigned w, unsigned h);

    app::AppController controller_;
    sf::RenderWindow   window_;

    std::unique_ptr<MainScreen>          main_;
    std::unique_ptr<IncomingOfferScreen> offer_;

    // V1.1.8-UX3 : drag & drop OS natif vers la fenêtre.
    DragDropHandler dragHandler_;
};

} // namespace ltr::ui
