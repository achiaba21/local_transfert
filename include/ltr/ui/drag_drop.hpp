#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include <SFML/Window/WindowHandle.hpp>

namespace ltr::ui {

// Handler natif drag & drop fichiers/dossiers OS → fenêtre SFML.
//
// Implémenté en .mm (Objective-C++ / AppKit) sur macOS, en Win32
// (Shell32 + WM_DROPFILES) sur Windows, stub no-op ailleurs.
// Tous les callbacks sont invoqués sur le thread UI (main NSApp
// runloop sur mac, Windows message pump sur win) — aucun verrou
// nécessaire côté caller.
//
// Limitations V1 :
//  - macOS : onEnter/onExit + onDrop complets
//  - Windows : onDrop seul (pas d'events drag-over via WM_DROPFILES)
//  - Linux : stub, log warning à l'attach
class DragDropHandler {
public:
    struct Callbacks {
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void(std::vector<std::filesystem::path>)> onDrop;
    };

    DragDropHandler();
    ~DragDropHandler();

    DragDropHandler(const DragDropHandler&)            = delete;
    DragDropHandler& operator=(const DragDropHandler&) = delete;

    // Attache le handler à une fenêtre déjà créée.
    // Return false si la plateforme ne supporte pas (stub).
    bool attach(sf::WindowHandle handle, Callbacks cb);
    void detach();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ltr::ui
