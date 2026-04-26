# Architecture — Sprint UX-3 Drag & drop OS

**Date :** 2026-04-24
**NEW_PROJECT :** false
**UI_REQUIRED :** true (mockups highlight zone + texte inline)

---

## 1. Vue d'ensemble

Ajout d'un module `ltr::ui::DragDropHandler` avec interface commune C++
et trois implémentations plateforme-specific linkées conditionnellement
par CMake :
- **macOS** → `drag_drop_mac.mm` (Objective-C++, AppKit)
- **Windows** → `drag_drop_win.cpp` (Win32 + Shell32)
- **Linux** → `drag_drop_stub.cpp` (no-op, log warning)

Le handler est instancié dans `UIApp` après création de la fenêtre,
attaché au `sf::WindowHandle`, et connecté à `AppController::addFiles`
via callback.

Le `MainScreen` gagne un flag `dragOverActive_` exposé par `UIApp` pour
dessiner le highlight de la zone centrale pendant un drag en cours.

### Fichiers à toucher

| Fichier | Action |
|---------|--------|
| `include/ltr/ui/drag_drop.hpp` | NOUVEAU — interface commune |
| `src/ui/drag_drop_mac.mm` | NOUVEAU (APPLE only) |
| `src/ui/drag_drop_win.cpp` | NOUVEAU (WIN32 only) |
| `src/ui/drag_drop_stub.cpp` | NOUVEAU (Linux/autres) |
| `include/ltr/ui/ui_app.hpp` | Ajout `dragHandler_` + setters drag state |
| `src/ui/ui_app.cpp` | Instancier + attacher callbacks |
| `include/ltr/ui/screens/main_screen.hpp` | Ajout `setDragOver(bool)` |
| `src/ui/screens/main_screen.cpp` | Dessin highlight + texte inline pendant drag |
| `CMakeLists.txt` | Compile conditionnelle + enable_language(OBJCXX) sur APPLE |
| `docs-agents/UI_GUIDELINES.md` | Section « Drag & drop » |

---

## 2. Interface commune

```cpp
// include/ltr/ui/drag_drop.hpp
#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include <SFML/Window/WindowHandle.hpp>

namespace ltr::ui {

// Handler natif drag & drop. Attaché à une fenêtre SFML via son handle
// système (NSWindow* sur mac, HWND sur win).
// Les callbacks sont appelés sur le thread UI (main NSApp / Windows msg
// pump) — aucun lock nécessaire côté caller.
class DragDropHandler {
public:
    struct Callbacks {
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void(std::vector<std::filesystem::path>)> onDrop;
    };

    DragDropHandler();
    ~DragDropHandler();

    DragDropHandler(const DragDropHandler&) = delete;
    DragDropHandler& operator=(const DragDropHandler&) = delete;

    // Attache à la fenêtre. La fenêtre doit être déjà créée.
    // Return false si la plateforme ne supporte pas (stub Linux).
    bool attach(sf::WindowHandle handle, Callbacks cb);
    void detach();

private:
    // Impl opaque pour masquer les détails plateforme dans le header.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ltr::ui
```

---

## 3. Impl macOS (drag_drop_mac.mm)

Approche : **ajout dynamique de méthodes à la classe de la contentView
SFML** via l'Objective-C runtime. Moins invasif qu'un swizzling complet,
et la contentView SFML (SFOpenGLView ou son ancêtre) reçoit directement
les events drag.

```objc++
// Pseudocode des points clés
@interface LTRDragDelegate : NSObject
@property ltr::ui::DragDropHandler::Callbacks cb;
@end

// Dans attach() :
NSWindow* win = (__bridge NSWindow*)handle;
NSView* view = [win contentView];

// Autoriser les drops de fichiers (types modernes + legacy).
[view registerForDraggedTypes:@[NSPasteboardTypeFileURL,
                                 NSFilenamesPboardType]];

// Ajouter les méthodes NSDraggingDestination si absentes.
Class c = [view class];
if (!class_getInstanceMethod(c, @selector(draggingEntered:))) {
    class_addMethod(c, @selector(draggingEntered:),
                   (IMP)ltr_draggingEntered, "L@:@");
}
// idem draggingExited: + performDragOperation:

// Stocker les callbacks via associated object (retainé, libéré au
// -dealloc du view → pas de fuite).
LTRDragDelegate* delegate = [LTRDragDelegate new];
delegate.cb = cb;
objc_setAssociatedObject(view, kCallbacksKey, delegate,
                          OBJC_ASSOCIATION_RETAIN);
```

Extraction des paths dans `performDragOperation:` :

```objc
NSPasteboard* pb = [sender draggingPasteboard];
NSArray<NSString*>* files = [pb propertyListForType:NSFilenamesPboardType];
if (files.count == 0) {
    NSArray<NSURL*>* urls = [pb readObjectsForClasses:@[NSURL.class]
                                                options:nil];
    // ... fallback vers les URLs file://
}
std::vector<std::filesystem::path> paths;
for (NSString* f in files) {
    paths.emplace_back([f UTF8String]);
}
```

Frameworks à linker sur APPLE (déjà liés par SFML) :
- `-framework Cocoa` (AppKit, Foundation) : déjà transitif via SFML

---

## 4. Impl Windows (drag_drop_win.cpp)

Approche : **subclass du HWND via `SetWindowSubclass`** pour intercepter
`WM_DROPFILES`. Les events enter/exit ne sont pas disponibles via le mode
simple `DragAcceptFiles` — seul le drop final fire (IDropTarget avec COM
le permettrait mais est lourd pour V1).

**Conséquence UX :** sur Windows, le feedback visuel enter/exit N'EST
PAS dispo en V1. Seul le drop émet l'event. L'utilisateur voit juste le
fichier ajouté.

```cpp
LRESULT CALLBACK dropProc(HWND h, UINT msg, WPARAM wp, LPARAM lp,
                           UINT_PTR id, DWORD_PTR ref) {
    if (msg == WM_DROPFILES) {
        HDROP hdrop = (HDROP)wp;
        UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::filesystem::path> paths;
        for (UINT i = 0; i < count; ++i) {
            UINT len = DragQueryFileW(hdrop, i, nullptr, 0);
            std::wstring buf(len + 1, 0);
            DragQueryFileW(hdrop, i, buf.data(), len + 1);
            buf.resize(len);
            paths.emplace_back(buf);
        }
        DragFinish(hdrop);
        auto* cbs = reinterpret_cast<DragDropHandler::Callbacks*>(ref);
        if (cbs->onDrop) cbs->onDrop(std::move(paths));
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp, id, ref);
}

bool DragDropHandler::attach(sf::WindowHandle h, Callbacks cb) {
    HWND hwnd = reinterpret_cast<HWND>(h);
    DragAcceptFiles(hwnd, TRUE);
    auto* stored = new Callbacks(cb);
    SetWindowSubclass(hwnd, dropProc, 1, reinterpret_cast<DWORD_PTR>(stored));
    impl_ = /* ... stocke hwnd + stored pour détacher */;
    return true;
}
```

Libs à linker : `shell32.lib` (pour DragAcceptFiles, DragQueryFileW,
DragFinish), `comctl32.lib` (pour SetWindowSubclass).

---

## 5. Stub Linux (drag_drop_stub.cpp)

```cpp
#include "ltr/ui/drag_drop.hpp"
#include "ltr/core/logger.hpp"

namespace ltr::ui {
struct DragDropHandler::Impl {};
DragDropHandler::DragDropHandler() : impl_(std::make_unique<Impl>()) {}
DragDropHandler::~DragDropHandler() = default;

bool DragDropHandler::attach(sf::WindowHandle, Callbacks) {
    core::log_warn("[drag-drop] not supported on this platform");
    return false;
}
void DragDropHandler::detach() {}
} // namespace
```

---

## 6. UIApp — orchestration

```cpp
// ui_app.hpp (ajouts)
private:
    DragDropHandler dragHandler_;

// ui_app.cpp (dans ctor, après window_.create)
dragHandler_.attach(window_.getSystemHandle(), {
    /*.onEnter =*/ [this]{
        if (main_) main_->setDragOver(true);
    },
    /*.onExit  =*/ [this]{
        if (main_) main_->setDragOver(false);
    },
    /*.onDrop  =*/ [this](std::vector<std::filesystem::path> paths){
        if (main_) main_->setDragOver(false);
        controller_.addFiles(paths);
        // S'assurer qu'on dessine la mise à jour au prochain frame
        // (déjà automatique via le event loop SFML).
    },
});
```

Thread-safety : les callbacks arrivent sur le thread main (NSApp
runloop ou Windows msg pump). `controller_.addFiles` s'exécute déjà
depuis le thread main (boutons UI), donc réutilisation directe.

---

## 7. MainScreen — feedback visuel

### Nouveau champ

```cpp
class MainScreen {
public:
    void setDragOver(bool on) { dragOver_ = on; }
private:
    bool dragOver_{false};
};
```

### Dessin dans drawCenter

```cpp
void MainScreen::drawCenter(sf::RenderTarget& target) const {
    // ... dessin existant ...

    if (dragOver_) {
        // 1) Bordure accent 2 px sur le rectangle central
        RoundedRect border(centerRect_.left + 4.f, centerRect_.top + 4.f,
                           centerRect_.width - 8.f,
                           centerRect_.height - 8.f, Radius::md);
        border.setFillColor(sf::Color(
            Colors::accentLight.r, Colors::accentLight.g,
            Colors::accentLight.b, 40));
        border.setOutline(Colors::accent, 2.f);
        border.draw(target);

        // 2) Texte inline « Déposer pour ajouter » centré vertically
        Label drop;
        drop.setText("Déposer pour ajouter")
            .setBold(true).setSize(FontSize::h1)
            .setColor(Colors::accent);
        const auto m = drop.measure();
        drop.setPosition(
            centerRect_.left + (centerRect_.width - m.x) / 2.f,
            centerRect_.top + (centerRect_.height - m.y) / 2.f - 20.f);
        drop.draw(target);
    }
}
```

---

## 8. CMakeLists — compile conditionnelle

```cmake
# Ajouter dans le bloc ltr_ui
if(APPLE)
    enable_language(OBJCXX)
    list(APPEND LTR_DRAG_DROP_SRC src/ui/drag_drop_mac.mm)
    set_source_files_properties(src/ui/drag_drop_mac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc")
elseif(WIN32)
    list(APPEND LTR_DRAG_DROP_SRC src/ui/drag_drop_win.cpp)
else()
    list(APPEND LTR_DRAG_DROP_SRC src/ui/drag_drop_stub.cpp)
endif()

add_library(ltr_ui STATIC
    # ... sources existantes ...
    ${LTR_DRAG_DROP_SRC}
)

# Frameworks macOS (Cocoa déjà lié par SFML)
if(APPLE)
    target_link_libraries(ltr_ui PRIVATE "-framework AppKit")
elseif(WIN32)
    target_link_libraries(ltr_ui PRIVATE shell32 comctl32)
endif()
```

---

## 9. CONTRAT D'IMPLÉMENTATION

### Fichiers à créer
- [ ] `include/ltr/ui/drag_drop.hpp`
- [ ] `src/ui/drag_drop_mac.mm` (APPLE)
- [ ] `src/ui/drag_drop_win.cpp` (WIN32)
- [ ] `src/ui/drag_drop_stub.cpp` (autres)

### Fichiers à modifier
- [ ] `include/ltr/ui/ui_app.hpp` — membre `dragHandler_`
- [ ] `src/ui/ui_app.cpp` — attach dans ctor après création window
- [ ] `include/ltr/ui/screens/main_screen.hpp` — `setDragOver(bool)` + flag
- [ ] `src/ui/screens/main_screen.cpp` — dessin highlight dans `drawCenter`
- [ ] `CMakeLists.txt` — compile conditionnelle + enable_language(OBJCXX)
      sur APPLE + frameworks
- [ ] `docs-agents/UI_GUIDELINES.md` — section « Drag & drop »

### Limitations connues (documentées)
- Windows : pas de feedback enter/exit en V1 (seul le drop fire). Une
  v2 pourrait passer par IDropTarget COM.
- Linux : stub complet, pas de support XdndAware V1.

### Tests
- Pas de tests unitaires possibles (API native requiert une fenêtre NSApp
  / HWND réelles). Smoke test manuel :
  - Drag 1 fichier depuis Finder → dépôt → apparition dans la liste
  - Drag 1 dossier → dépôt → apparition avec [icône dossier]
  - Drag multiple → tous ajoutés
  - 8 tests existants doivent passer

---

UI_REQUIRED: true (mockups highlight + feedback à valider — mais
décisions haut-niveau déjà prises, donc mockup léger)
