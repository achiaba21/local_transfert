#include "ltr/ui/ui_app.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/ui/theme.hpp"

#include <SFML/Graphics/View.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <tinyfiledialogs.h>

namespace ltr::ui {


UIApp::UIApp() {
    core::log_debug("UIApp: chargement de la police...");
    (void)theme_font();
    core::log_debug("UIApp: police OK");

    // V1.1.2 : inject du dialog natif pour "choisir dossier de destination"
    // (appelé depuis AppController::acceptWebUpload sur le thread UI).
    controller_.setFolderPicker([](const std::string& defaultDir) {
        const char* dir = tinyfd_selectFolderDialog(
            "Enregistrer le(s) fichier(s) dans…",
            defaultDir.c_str());
        return dir ? std::string(dir) : std::string{};
    });

    controller_.start();

    sf::ContextSettings settings;
    settings.antialiasingLevel = 4;

    core::log_debug("UIApp: création de la fenêtre 1100x680...");
    window_.create(sf::VideoMode(1100, 680),
                   "LocalTransfer - " + controller_.state().self.name,
                   sf::Style::Default, settings);
    core::log_debug("UIApp: fenêtre créée");
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);

    core::log_debug("UIApp: création des écrans...");
    main_  = std::make_unique<MainScreen>(controller_);
    offer_ = std::make_unique<IncomingOfferScreen>(controller_);
    core::log_debug("UIApp: écrans OK");

    onResize(window_.getSize().x, window_.getSize().y);
    core::log_debug("UIApp: layout initial OK");

    // V1.1.8-UX3 : drag & drop OS → AppController::addFiles. Les callbacks
    // arrivent sur le thread UI (NSApp runloop / Windows msg pump), donc
    // on peut appeler directement controller_ sans verrou.
    dragHandler_.attach(window_.getSystemHandle(), {
        /*.onEnter =*/ [this]{ if (main_) main_->setDragOver(true); },
        /*.onExit  =*/ [this]{ if (main_) main_->setDragOver(false); },
        /*.onDrop  =*/ [this](std::vector<std::filesystem::path> paths) {
            if (main_) main_->setDragOver(false);
            controller_.addFiles(paths);
        },
    });
}

void UIApp::onResize(unsigned w, unsigned h) {
    sf::View view(sf::FloatRect(0.f, 0.f,
                                static_cast<float>(w),
                                static_cast<float>(h)));
    window_.setView(view);
    main_->setViewSize({w, h});
    offer_->setViewSize({w, h});
}

int UIApp::run() {
    sf::Clock clock;
    while (window_.isOpen()) {
        // Draine la queue d'événements applicatifs (réseau → UI).
        controller_.tick();

        sf::Event e;
        while (window_.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                window_.close();
                break;
            }
            if (e.type == sf::Event::Resized) {
                onResize(e.size.width, e.size.height);
                continue;
            }

            // V1.1.2 : la modale capture aussi les events pour les offres web.
            const bool modalActive = controller_.state().incomingOffer ||
                                     controller_.state().pendingWebOffer ||
                                     (controller_.state().webInboxModalOpen &&
                                      !controller_.state().webInbox.empty());
            if (modalActive) {
                offer_->handleEvent(e, controller_.state());
            } else {
                main_->handleEvent(e, controller_.state());
            }
        }

        const sf::Time dt = clock.restart();
        main_->update(controller_.state(), dt);
        offer_->update(controller_.state(), dt);

        window_.clear(Colors::bg);
        main_->draw(window_);
        const bool modalVisible = controller_.state().incomingOffer ||
                                  controller_.state().pendingWebOffer ||
                                  (controller_.state().webInboxModalOpen &&
                                   !controller_.state().webInbox.empty());
        if (modalVisible) {
            offer_->draw(window_);
        }
        window_.display();
    }

    controller_.stop();
    return 0;
}

} // namespace ltr::ui
