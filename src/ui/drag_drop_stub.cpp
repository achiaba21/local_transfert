// Sprint UX-3 : stub no-op pour les plateformes sans impl native V1
// (Linux X11 notamment). Log un warning au 1er attach mais ne casse
// pas le build ni l'app.

#include "ltr/ui/drag_drop.hpp"

#include "ltr/core/logger.hpp"

namespace ltr::ui {

struct DragDropHandler::Impl {};

DragDropHandler::DragDropHandler()  : impl_(std::make_unique<Impl>()) {}
DragDropHandler::~DragDropHandler() = default;

bool DragDropHandler::attach(sf::WindowHandle, Callbacks) {
    core::log_warn("[drag-drop] pas encore support\xC3\xA9 sur cette "
                    "plateforme — utiliser Ajouter \xE2\x96\xBE "
                    "pour choisir des fichiers");
    return false;
}

void DragDropHandler::detach() {}

} // namespace ltr::ui
