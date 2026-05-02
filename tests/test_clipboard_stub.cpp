// V1.4 — Sprint Clipboard Paste
// Test minimal de l'API ClipboardPaste. Sur le stub Linux ou si le
// presse-papier est vide / inaccessible, readClipboard doit retourner
// Kind::None proprement (pas de crash, pas d'exception).
//
// Note : on ne peut pas vraiment tester les implémentations natives
// macOS/Windows en CI sans manipuler le presse-papier, donc on se
// limite à l'invariant : appeler readClipboard est sûr et retourne
// une struct cohérente.

#include "ltr/ui/clipboard_paste.hpp"

#include <cassert>
#include <iostream>

int main() {
    using ltr::ui::ClipboardPaste;
    using ltr::ui::readClipboard;

    // Appel doit toujours retourner sans throw.
    ClipboardPaste paste = readClipboard();

    // Invariants par kind :
    if (paste.kind == ClipboardPaste::Kind::None) {
        assert(paste.text.empty());
        assert(paste.imageBytes.empty());
        assert(paste.imageExt.empty());
        assert(paste.files.empty());
    } else if (paste.kind == ClipboardPaste::Kind::Text) {
        assert(!paste.text.empty());
        assert(paste.imageBytes.empty());
        assert(paste.files.empty());
    } else if (paste.kind == ClipboardPaste::Kind::Image) {
        assert(!paste.imageBytes.empty());
        assert(paste.imageExt == "png");
        assert(paste.files.empty());
    } else if (paste.kind == ClipboardPaste::Kind::Files) {
        assert(!paste.files.empty());
        assert(paste.imageBytes.empty());
    }

    std::cout << "test_clipboard_stub OK (kind="
              << static_cast<int>(paste.kind) << ")\n";
    return 0;
}
