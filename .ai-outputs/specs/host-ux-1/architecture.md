# Architecture — Sprint UX-1 Hygiène visuelle host

**Date :** 2026-04-24
**NEW_PROJECT :** false
**UI_REQUIRED :** true (mockups ASCII pour menu Ajouter + pill Local + empty states)

---

## 1. Vue d'ensemble

Sprint visuel sans nouveau flux. Structure :
1. Nouveau module `ltr::ui::IconLibrary` — singleton lazy qui charge les
   PNG embedded dans `sf::Texture` partagées.
2. Nouveau script `scripts/generate_icons.py` (one-shot, Python + PIL)
   qui produit les PNG dans `assets/icons/` depuis des définitions
   algorithmiques. Commité pour reproductibilité ; les PNG aussi.
3. Nouveau widget `DropdownMenu` minimaliste (rendu par-dessus la zone
   centrale, clic extérieur = ferme).
4. Modifications ciblées : `FileRow`, `DeviceListItem`, `MainScreen`.

### Files à toucher

| Fichier | Action |
|---------|--------|
| `scripts/generate_icons.py` | NOUVEAU — génère les PNG via PIL |
| `assets/icons/*.png` (7 PNG) | NOUVEAUX — générés par le script |
| `include/ltr/ui/icon_library.hpp` | NOUVEAU |
| `src/ui/icon_library.cpp` | NOUVEAU |
| `include/ltr/ui/widgets/dropdown_menu.hpp` | NOUVEAU |
| `src/ui/widgets/dropdown_menu.cpp` | NOUVEAU |
| `include/ltr/ui/widgets/file_row.hpp` | — (API inchangée) |
| `src/ui/widgets/file_row.cpp` | MODIFIÉ (check/folder/close via IconLibrary) |
| `src/ui/widgets/device_list_item.cpp` | MODIFIÉ (pill Local) |
| `include/ltr/ui/screens/main_screen.hpp` | MODIFIÉ (radar elapsed, dropdown menu) |
| `src/ui/screens/main_screen.cpp` | MODIFIÉ (menu Ajouter, empty state 2 états, flèches) |
| `CMakeLists.txt` | MODIFIÉ (embed PNG icons) |

---

## 2. IconLibrary (nouveau)

```cpp
// include/ltr/ui/icon_library.hpp
#pragma once

#include <string_view>
#include <SFML/Graphics/Texture.hpp>

namespace ltr::ui {

// Catalogue d'icônes PNG embedded, lazy-loaded au 1er appel.
// Singleton (thread-main uniquement, cohérent avec l'UI SFML).
class IconLibrary {
public:
    enum class Id {
        Check,       // 24×24 blanc
        Folder,      // 20×20 accent
        Close,       // 16×16 textSecondary
        ArrowUp,     // 18×18 accent
        ArrowDown,   // 18×18 success
        Radar,       // 48×48 accent (empty state animé)
        NoDevice,    // 48×48 textSecondary (empty state statique)
    };

    static const sf::Texture& get(Id id);

private:
    IconLibrary() = default;
};

} // namespace ltr::ui
```

Impl :
- Map `Id → sf::Texture` lazy + const strings `Id → string_view` des
  données embedded depuis `<ltr/web/assets/icon_*.hpp>` (réutilise le
  même mécanisme `ltr_embed_file`).
- Charge via `texture.loadFromMemory(data, len)` puis cache.
- Retourne `const sf::Texture&` — les widgets construisent `sf::Sprite`
  à chaque draw (coût négligeable).

### Script generate_icons.py

```python
# scripts/generate_icons.py
# Usage : python3 scripts/generate_icons.py
# Idempotent : regénère assets/icons/*.png
# Icônes monochromes, alpha channel, exportées comme PNG RGBA.
from PIL import Image, ImageDraw
import os

OUT = "assets/icons"
os.makedirs(OUT, exist_ok=True)

def draw_check(size=24):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Coche en 2 segments blancs épais
    w = 3
    d.line([(5, 13), (10, 18)], fill=(255,255,255,255), width=w)
    d.line([(10, 18), (19, 7)], fill=(255,255,255,255), width=w)
    return img

def draw_folder(size=20): ...
def draw_close(size=16): ...
def draw_arrow(direction, size=18): ...   # up / down
def draw_radar(size=48): ...
def draw_no_device(size=48): ...

# ... (détail dans le fichier) ...

for name, img in icons.items():
    img.save(f"{OUT}/{name}.png", "PNG")
```

---

## 3. DropdownMenu (nouveau)

```cpp
// include/ltr/ui/widgets/dropdown_menu.hpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

// Menu popup simple : liste d'options sous une position d'ancrage.
// Affiché seulement si open_ == true. Se ferme au clic extérieur ou
// sur sélection. À dessiner PAR-DESSUS le reste de l'écran (dernier).
class DropdownMenu {
public:
    struct Item {
        std::string label;
        std::function<void()> action;
    };

    DropdownMenu& setItems(std::vector<Item> items);
    DropdownMenu& setAnchor(const sf::FloatRect& anchor); // rect du bouton

    void openMenu();
    void close();
    bool isOpen() const noexcept { return open_; }

    // Gère le clic : si open && clic sur un item → action + close.
    // Si open && clic extérieur → close.
    // Return : true si l'event a été consommé (pour empêcher propagation).
    bool handleEvent(const sf::Event& e);

    void draw(sf::RenderTarget& target) const;

private:
    std::vector<Item> items_;
    sf::FloatRect anchor_{};
    bool open_{false};
    int hoverIdx_{-1};
};

} // namespace ltr::ui
```

Dessin : fond `Colors::surface` + outline `separator`, items 32 px de
haut, hover → `accentLight`, texte `body`. Shadow subtile via 2ème
RoundedRect translucide dessous.

---

## 4. Modifications MainScreen

### 4.1 Bouton unique + menu

```cpp
// MainScreen state
Button addBtn_;            // remplace browseFilesBtn_ + browseFolderBtn_
DropdownMenu addMenu_;

// Layout : un seul bouton à gauche au lieu de 2 → + de place pour ENVOYER
const float addW = 140.f;
addBtn_.setBounds({baseX, btnY, addW, btnH});
clearFilesBtn_.setBounds({baseX + addW + gap, btnY, clearW, btnH});

// onClick
addBtn_.onClick([this]{
    addMenu_.setAnchor(addBtn_.bounds()).openMenu();
});

// Menu items
addMenu_.setItems({
    {"Fichiers...", [this]{ openFilesPicker(); }},
    {"Dossier...",  [this]{ openFolderPicker(); }},
});

// handleEvent : donner priorité au menu s'il est ouvert
if (addMenu_.isOpen()) {
    if (addMenu_.handleEvent(e)) return;  // menu a consommé
}

// draw : dessiner le menu EN DERNIER (par-dessus tout)
void MainScreen::draw(...) const {
    drawBackground(...); drawHeader(...); drawSidebar(...);
    drawCenter(...); sharePanel_.draw(...); drawTransferBar(...);
    addMenu_.draw(target);  // ← dernier
}
```

### 4.2 Radar animé (elapsed interne)

```cpp
// MainScreen state
float emptyElapsed_{0.f};   // secs since scan started
bool  hasEverSeenPeer_{false};

void MainScreen::update(const AppState& state, sf::Time dt) {
    emptyElapsed_ += dt.asSeconds();
    if (!state.peers.empty()) hasEverSeenPeer_ = true;
    // ...
}

// drawSidebar empty state
const bool searching = emptyElapsed_ < 5.f && !hasEverSeenPeer_;
if (searching) {
    // radar pulsant : scale 1.0 → 1.2 → 1.0 sur 1.2 s, modulation via sin
    const float phase = std::sin(emptyElapsed_ * 5.f) * 0.5f + 0.5f;
    drawIconSprite(Icon::Radar, cx, cy, 48 + phase*8);
    drawText("Recherche...", cx, cy+40);
} else {
    drawIconSprite(Icon::NoDevice, cx, cy, 48);
    drawText("Aucun appareil détecté", cx, cy+40);
}
```

Rescan manuel : reset `emptyElapsed_ = 0; hasEverSeenPeer_ = false` dans
le callback `rescanBtn_.onClick`.

### 4.3 Flèches transfert

```cpp
// drawTransferBar : remplacer t.direction (texte "→" / "←") par :
const auto icon = t.isOutgoing ? IconId::ArrowUp : IconId::ArrowDown;
// Dessin 18×18 à (tx + Spacing::md, ty + 10)
// Puis label1 "peerName" décalé à droite de l'icône.
```

L'existence de `t.isOutgoing` nécessite de regarder l'UiTransfer struct.
Actuellement `t.direction` est un `std::string` ("→" ou "←"). On
introduit un `enum class Direction { Out, In }` OU on parse `t.direction`.
**Choix :** étendre `UiTransfer` avec `enum Direction` pour éviter du
string-matching.

### 4.4 Texte "Parcourir" → "Ajouter"

`main_screen.cpp:459` — changer le texte de l'empty state centrale.

---

## 5. Modifications FileRow

```cpp
// src/ui/widgets/file_row.cpp : draw()

// Checkbox cochée : sprite Check (blanc) centré sur l'accent carré
if (checked_) {
    RoundedRect(chk).setFillColor(Colors::accent).draw(target);
    sf::Sprite s(IconLibrary::get(IconLibrary::Id::Check));
    s.setPosition(cb.left + (cb.width - 24)/2, cb.top + (cb.height - 24)/2);
    target.draw(s);
}

// Icône dossier : avant le nom, si kind == Folder
if (kind_ == Kind::Folder) {
    sf::Sprite fs(IconLibrary::get(IconLibrary::Id::Folder));
    fs.setPosition(textLeft, bounds_.top + (bounds_.height - 20)/2);
    target.draw(fs);
    textLeft += 20 + Spacing::sm;  // le nom se décale
}

// Croix de suppression : sprite Close (textSecondary) au lieu du texte "x"
if (hoverX_) {
    // ... fond hover ...
}
sf::Sprite cs(IconLibrary::get(IconLibrary::Id::Close));
cs.setColor(Colors::textSecondary); // tintage via sf::Sprite::setColor
cs.setPosition(xBtn.left + (xBtn.width - 16)/2, xBtn.top + (xBtn.height - 16)/2);
target.draw(cs);
```

Suppression du préfixe `"[D] "` dans `displayName` — on dessine l'icône
à la place.

---

## 6. Modifications DeviceListItem

Pill `Local` + pill `Web` → extraction en helper local pour DRY :

```cpp
// Dans device_list_item.cpp anonymous namespace
void drawKindPill(sf::RenderTarget& t, float right, float centerY,
                  domain::PeerKind kind) {
    const std::string text =
        (kind == domain::PeerKind::Web) ? "Web" : "Local";
    constexpr float pillW = 46.f;
    // ... (style identique pour les 2, accentLight + accent text) ...
}

// Dans draw() :
drawKindPill(target, chkX - chkR - Spacing::md, chkY, device_.kind);
```

Les 2 pills visuellement identiques — seul le texte change. Pas de
pill pour absence de `kind` (mais `kind` a un default Native, donc tous
les DeviceListItem auront un pill — la symétrie est complète).

---

## 7. CMake — embed des PNG

```cmake
# Dans CMakeLists.txt :
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/check.png
               IconCheck "image/png" icon_check)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/folder.png
               IconFolder "image/png" icon_folder)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/close.png
               IconClose "image/png" icon_close_png)  # "close" existe déjà → icon_close_png
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/arrow-up.png
               IconArrowUp "image/png" icon_arrow_up)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/arrow-down.png
               IconArrowDown "image/png" icon_arrow_down)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/radar.png
               IconRadar "image/png" icon_radar)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/no-device.png
               IconNoDevice "image/png" icon_no_device)

set(LTR_WEB_GEN_HEADERS
    ${LTR_WEB_GEN_HEADERS}
    ${icon_check_GEN} ${icon_folder_GEN} ${icon_close_png_GEN}
    ${icon_arrow_up_GEN} ${icon_arrow_down_GEN}
    ${icon_radar_GEN} ${icon_no_device_GEN})

# Ajouter au ltr_core
add_library(ltr_core STATIC
    ...
    src/ui/icon_library.cpp
    src/ui/widgets/dropdown_menu.cpp)
```

**Note :** les headers sont générés dans `build/generated/ltr/web/assets/`
(préfixe web/ historique). Pas bloquant — juste un chemin.

---

## 8. CONTRAT D'IMPLÉMENTATION

### Pages / Routes
- Aucune (pas de changement backend)

### Fichiers à créer
- [ ] `scripts/generate_icons.py`
- [ ] `assets/icons/check.png` (24×24)
- [ ] `assets/icons/folder.png` (20×20)
- [ ] `assets/icons/close.png` (16×16)
- [ ] `assets/icons/arrow-up.png` (18×18)
- [ ] `assets/icons/arrow-down.png` (18×18)
- [ ] `assets/icons/radar.png` (48×48)
- [ ] `assets/icons/no-device.png` (48×48)
- [ ] `include/ltr/ui/icon_library.hpp`
- [ ] `src/ui/icon_library.cpp`
- [ ] `include/ltr/ui/widgets/dropdown_menu.hpp`
- [ ] `src/ui/widgets/dropdown_menu.cpp`

### Fichiers à modifier
- [ ] `src/ui/widgets/file_row.cpp` — use IconLibrary pour Check/Folder/Close
- [ ] `src/ui/widgets/device_list_item.cpp` — pill Local + helper drawKindPill
- [ ] `include/ltr/ui/screens/main_screen.hpp` — `addBtn_`, `addMenu_`, `emptyElapsed_`, `hasEverSeenPeer_`
- [ ] `src/ui/screens/main_screen.cpp` — menu, empty state 2 états, flèches transfert, texte obsolète
- [ ] `include/ltr/app/app_state.hpp` — `UiTransfer::direction` devient enum `Direction { Out, In }`
- [ ] `src/app/app_controller.cpp` — adapte `t.direction = Direction::Out/In`
- [ ] `CMakeLists.txt` — embed des 7 PNG + ajout icon_library.cpp + dropdown_menu.cpp
- [ ] `docs-agents/UI_GUIDELINES.md` — section « Icônes & génération PNG »
- [ ] `.ai-outputs/specs/host-ui-improvements/PROGRESS.md` — UX-1 ✅, journal

### Tests
- Pas de nouveau test unitaire (purement visuel). Smoke test manuel :
  lancer l'app, vérifier toutes les icônes, ouvrir/fermer le menu
  Ajouter, attendre 5 s en empty state pour voir la transition radar →
  stable.

---

UI_REQUIRED: true (l'agent UI/UX doit proposer 2 mockups ASCII pour :
menu Ajouter ouvert, pill Local, empty state 2 états)
