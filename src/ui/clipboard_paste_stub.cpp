// V1.4 — Sprint Clipboard Paste (Linux / autres : stub minimal).
//
// SFML ne supporte que le texte. On tente sf::Clipboard::getString() ;
// si non vide → Kind::Text, sinon Kind::None. Pas d'image, pas de
// fichiers (les protocoles X11 / Wayland varient et ce n'est pas le
// focus V1).

#include "ltr/ui/clipboard_paste.hpp"

#include <SFML/Window/Clipboard.hpp>

namespace ltr::ui {

ClipboardPaste readClipboard() {
    ClipboardPaste out;
    const auto sfmlStr = sf::Clipboard::getString();
    if (!sfmlStr.isEmpty()) {
        const auto u8 = sfmlStr.toUtf8();
        out.kind = ClipboardPaste::Kind::Text;
        out.text.assign(u8.begin(), u8.end());
    }
    return out;
}

} // namespace ltr::ui
