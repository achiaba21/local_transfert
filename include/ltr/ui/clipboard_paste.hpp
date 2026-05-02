#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ltr::ui {

// V1.4 — Sprint Clipboard Paste
// Lecture du presse-papier système. Compile-time conditionnel par OS,
// même pattern que drag_drop_*.
//
// Priorité de détection : Files > Image > Text > None.
struct ClipboardPaste {
    enum class Kind { None, Text, Image, Files };

    Kind kind = Kind::None;

    // Si kind == Text, contenu UTF-8 du presse-papier.
    std::string text;

    // Si kind == Image, octets bruts de l'image (déjà au format imageExt).
    std::vector<std::uint8_t> imageBytes;
    std::string               imageExt;  // "png" (toujours en V1)

    // Si kind == Files, chemins absolus.
    std::vector<std::filesystem::path> files;
};

// Lit le presse-papier système et retourne un ClipboardPaste.
// Implémentation par plateforme :
//   - APPLE : NSPasteboard (clipboard_paste_mac.mm)
//   - WIN32 : OpenClipboard (clipboard_paste_win.cpp)
//   - autres : sf::Clipboard pour le texte uniquement (clipboard_paste_stub.cpp)
ClipboardPaste readClipboard();

} // namespace ltr::ui
