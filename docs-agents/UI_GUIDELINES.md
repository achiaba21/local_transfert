# UI_GUIDELINES.md — Design system LocalTransfer

## 🆕 Système d'affichage central (sprint UI Layout System, 2026-04-27)

7 modules pour résoudre les débordements de texte, le clipping par
zone, la non-uniformité des scrolls, l'absence de responsive.

### `Label` étendu (text constraints)

```cpp
Label{}
    .setText(longName)
    .setSize(FontSize::body)
    .setBounds({x, y, width, height})  // raccourci : maxWidth + position
    .setMaxWidth(width)
    .setEllipsis(true)
    .setAlignment(Label::Alignment::Center)
    .draw(target);
```

`setMaxWidth + setEllipsis(true)` tronque automatiquement avec `…` si
le texte naturel dépasse. **Toujours appliquer dans les zones
contraintes** (rows de listes, pills, zones flex).

`setBounds(rect)` est un raccourci qui :
- pose `maxWidth = rect.width`
- positionne le label DANS le rect selon l'alignement courant

### `ClipScope` (RAII)

Pour garantir qu'aucun dessin ne déborde d'une zone :

```cpp
{
    ClipScope clip(target, sidebarRect_);
    drawSidebar(target);   // tout dessin clippé à sidebarRect_
}
// view restaurée auto à la sortie du scope
```

À utiliser autour de chaque grande zone du screen (header, sidebar,
centre, share, bottom).

### `ScrollArea` (widget scroll unifié)

```cpp
class MyScreen {
    ScrollArea scroll_;

    void draw() {
        scroll_.setBounds(rect)
               .setDirection(ScrollArea::Direction::Vertical)
               .setContentSize(0.f, items.size() * rowH);

        scroll_.forEachVisible(
            items.size(),
            [rowH](size_t i) {
                return sf::FloatRect{0, i * rowH, w, rowH};
            },
            [&](size_t i, const sf::FloatRect& screenRect) {
                drawItem(items[i], screenRect);
            });

        scroll_.draw(target);  // scrollbar
    }

    void handleEvent(const sf::Event& e) {
        if (scroll_.handleEvent(e)) return;  // molette consommée
        // ... reste du handling, hit tests tiennent compte du scrollY
    }
};
```

### `Breakpoint` responsive

```cpp
breakpoint_ = detectBreakpoint(viewSize_.x);  // Compact / Regular / Large
const auto m = metricsFor(breakpoint_);

// m.sidebarW           = 300 (Regular) ou 360 (Large) ou 0 (Compact masqué)
// m.sharePanelExpandedW = 240 / 320 / 240
// m.forceSharePanelCollapsed = true en Compact
```

Compact mode = bouton ☰ dans le header pour toggle sidebar visible/cachée.

### Layout DSL `HBox` / `VBox`

Mini-DSL fluent builder :

```cpp
HBox{}
    .padding(Spacing::md)
    .spacing(Spacing::sm)
    .fixed(20.f, [](auto& t, const auto& r) { drawIcon(t, r); })
    .expanded(1, [](auto& t, const auto& r) {
        Label{}.setText(name).setBounds(r).setMaxWidth(r.width)
              .setEllipsis(true).draw(t);
    })
    .fixed(80.f, [](auto& t, const auto& r) { drawButton(t, r); })
    .layout(parentRect, target);
```

- `fixed(size, drawFn)` : enfant largeur (HBox) ou hauteur (VBox) fixe
- `expanded(weight, drawFn)` : enfant qui prend `weight/total` de
  l'espace restant
- `spacer(size)` : espace vide
- `padding(p)` ou `padding(h, v)` : marges internes
- `spacing(s)` : entre enfants

### Cache de mesure `LabelCache`

Géré automatiquement par `Label::measure`. Vidé sur changement DPI.
Ne pas appeler manuellement.

### `DpiScale` (pour V2)

Détecté au démarrage de `UiApp`. **Pas encore appliqué** aux tokens
Theme. Pour propager en V2 :

```cpp
namespace Spacing {
    inline float sm() { return DpiScale::scaled(8.f); }
}
```

### `Animation` (helper)

```cpp
Animation fade_;
fade_.start(0.f, 1.f, 0.2f, Animation::Easing::EaseOut);

// chaque frame :
fade_.update(dt.asSeconds());
const float alpha = fade_.value();
```

Pas encore câblé sur les cards transfer / inbox — disponible comme
infrastructure pour les sprints UX futurs.

---

## 🆕 Drag & drop OS (sprint UX-3, 2026-04-24)

### Module
`ltr::ui::DragDropHandler` (include/ltr/ui/drag_drop.hpp) — pImpl avec 3
backends compilés conditionnellement par CMake :

- **macOS** → `src/ui/drag_drop_mac.mm` (Objective-C++, AppKit). Ajoute
  dynamiquement `draggingEntered:`, `draggingExited:`,
  `performDragOperation:` à la classe de la contentView SFML via
  `class_addMethod`. Callbacks stockés en `objc_setAssociatedObject`.
- **Windows** → `src/ui/drag_drop_win.cpp` (Win32 + Shell32). Subclass
  du HWND via `SetWindowSubclass` pour capter `WM_DROPFILES`.
  **Limitation V1** : pas d'events drag-over (seul le drop final fire).
- **Linux/autres** → `src/ui/drag_drop_stub.cpp` (no-op, log warning).

### Usage
```cpp
dragHandler_.attach(window_.getSystemHandle(), {
    .onEnter = [this]{ main_->setDragOver(true);  },
    .onExit  = [this]{ main_->setDragOver(false); },
    .onDrop  = [this](std::vector<std::filesystem::path> p){
        main_->setDragOver(false);
        controller_.addFiles(p);
    },
});
```

### Thread-safety
Les callbacks arrivent sur le thread UI (NSApp runloop / Windows msg
pump) — pas de verrou requis.

### CMake
```cmake
project(local_transfer ... LANGUAGES C CXX OBJCXX)  # APPLE only
if(APPLE)
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_mac.mm)
elseif(WIN32)
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_win.cpp)
else()
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_stub.cpp)
endif()
# Linker AppKit (mac) ou shell32+comctl32 (win) conditionnellement.
```

---

## 🆕 Icônes (sprint UX-1, 2026-04-24)

### Source
7 icônes PNG monochromes + alpha dans `assets/icons/` :
`check.png`, `folder.png`, `close.png`, `arrow-up.png`, `arrow-down.png`,
`radar.png`, `no-device.png`.

Générées une fois via `scripts/generate_icons.py` (Python + Pillow) à
partir de définitions PIL algorithmiques. Commit des PNG dans le repo
pour reproductibilité build (pas de dépendance à Pillow en CI).

### Usage C++
```cpp
sf::Sprite s(IconLibrary::get(IconLibrary::Id::Check));
s.setColor(Colors::accent);   // tintage (les PNG sont blancs + alpha)
s.setPosition(x, y);
target.draw(s);
```

Les PNG sont embarqués via `ltr_embed_file` dans CMakeLists.txt et
chargés lazy dans `IconLibrary` (singleton thread-main). 0 I/O runtime
après le 1er `get()` par icône.

### Quand générer de nouvelles icônes
1. Ajouter la fonction de dessin dans `scripts/generate_icons.py`
2. Lancer `python3 scripts/generate_icons.py`
3. Ajouter `ltr_embed_file(...)` dans `CMakeLists.txt`
4. Ajouter `Id::X` + case dans `IconLibrary`
5. Commit les PNG + le script + le CMake

---

## Philosophie

- **Clair, aéré, moderne** — inspiré des apps desktop récentes (Linear,
  Notion)
- **Aucune dépendance UI** (pas d'ImGui, pas de Qt) — rendu 100 % SFML
- **Cohérence via tokens** — toutes les dimensions, couleurs, typographies
  passent par `include/ltr/ui/theme.hpp`
- **Pas d'animation** en V1 (hors progression naturelle)
- **Coins arrondis systématiques** via `RoundedRect` (pas de
  `sf::RectangleShape` dans l'UI finale)

## Palette

Toutes les couleurs sont dans `ltr::ui::Colors` (défini dans
`include/ltr/ui/theme.hpp`, initialisé dans `src/ui/theme.cpp`).

| Token | Hex | Usage |
|-------|-----|-------|
| `bg` | `#FAFAFB` | Fond général de la fenêtre |
| `surface` | `#FFFFFF` | Cards, boutons secondaires, barres |
| `sidebar` | `#F1F3F6` | Fond de la sidebar gauche |
| `accent` | `#6366F1` | Actions primaires, focus, liens |
| `accentHover` | `#4F46E5` | État hover des boutons primary |
| `accentLight` | `#EEF2FF` | Fond des éléments sélectionnés |
| `text` | `#0F172A` | Texte principal |
| `textSecondary` | `#64748B` | Texte secondaire / légendes |
| `separator` | `#E2E8F0` | Traits 1 px, borders |
| `success` | `#10B981` | États OK / terminé |
| `error` | `#EF4444` | États échec / refusé |
| `warning` | `#F59E0B` | Alerte |
| `overlay` | `rgba(0,0,0,.55)` | Backdrop modale |
| `shadow` | `rgba(0,0,0,.10)` | Ombre portée (cards, boutons primary) |

## Espacements

Grille 4 px, dans `namespace Spacing` :

```cpp
xs=4, sm=8, md=12, lg=16, xl=24, xxl=32, xxxl=48
```

## Rayons de coins

Dans `namespace Radius` :

```cpp
sm=6, md=10, lg=14, pill=999
```

- `sm` : petits composants (tags, file rows)
- `md` : boutons, cards normales (valeur par défaut de `RoundedRect`)
- `lg` : modales, panneaux principaux
- `pill` : badges arrondis complets

## Typographie

Police : **Inter** (bundled dans `assets/fonts/`) ou fallback système
(Helvetica, SF, Segoe UI, DejaVu Sans).

Dans `namespace FontSize` :

```cpp
h1=24, h2=16, body=14, small=12, overline=11, button=14, pin=56
```

Conventions :
- **h1** : titre d'écran ou de carte (en **Bold**)
- **h2** : sous-titre, label de section
- **body** : texte courant
- **small** : légendes, métadonnées
- **overline** : labels MAJUSCULES en tête de section (**Bold**, souvent
  avec `textSecondary`)
- **button** : libellé des boutons (toujours Bold)
- **pin** : chiffre du code d'appairage

## Widgets maison

### `RoundedRect`

Primitive de base. À utiliser partout où on aurait fait un `RectangleShape`.

```cpp
RoundedRect r(x, y, w, h, Radius::md);
r.setFillColor(Colors::surface)
 .setOutline(Colors::separator, 1.f)   // optionnel
 .setShadow(Colors::shadow, 4.f);      // optionnel
r.draw(target);
```

### `Button`

Trois variantes : `Primary` (indigo), `Secondary` (gris clair bordé),
`Danger` (rouge).

- Primary → ombre portée, pas de bordure
- Secondary → pas d'ombre, bordure `separator` 1 px
- État `disabled` → gris plein, pas d'ombre

Tailles conseillées : 40-44 px de haut, min 96 px de large.

### `ProgressBar`

Hauteur 4-8 px, rayons = hauteur / 2 (forme pill).

### `DeviceListItem`

Row de 64 px de haut, fond `transparent` → `separator` (hover) →
`accentLight` (selected). Checkbox ronde à droite.

### `FileRow`

Row de 48 px, fond `surface` avec bordure `separator`. Nom à gauche,
taille à droite.

### `Card`

Panneau rectangulaire générique avec option `setRadius()` + `setShadow()`.

## Règles visuelles

1. **Marges** : toujours ≥ `Spacing::xl` (24 px) autour du contenu
   principal. Grouper visuellement avec `Spacing::lg` (16 px).
2. **Séparation entre blocs** : soit fond différent (`bg` vs `surface` vs
   `sidebar`), soit trait 1 px `separator`. Jamais les deux simultanément.
3. **Pas de bordures partout** : préférer la couleur de fond et l'ombre
   pour isoler. Bordure 1 px uniquement sur les cards `surface` posées sur
   un fond `surface` ou `bg`.
4. **Ombres** : très légères (`shadow` = alpha 25), offset Y de 2-4 px
   maximum. Pas d'ombre colorée.
5. **Texte sur fond coloré** : toujours vérifier le contraste. Primary
   (`accent`) → texte blanc. Light (`accentLight`) → texte `accent`.
6. **Icônes** : on utilise des caractères Unicode (`✓`, `✕`, `✗`, `•`,
   `→`, `←`). Pas d'images. Si glyphe absent de la police, fallback ASCII.
7. **Hover** : toujours un indicateur visible sur les éléments cliquables
   (fond plus foncé, couleur du curseur, etc.).

## Gestion UTF-8 avec SFML (piège classique)

SFML 2.6 interprète `std::string` comme **Latin-1** par défaut. Pour de
l'UTF-8 correct (accents, caractères Unicode), **toujours** passer par
`ltr::ui::utf8()` :

```cpp
// ❌ Mauvais — "é" va s'afficher en carré
text.setString("Sélectionnez un appareil");

// ✅ Bon
text.setString(utf8("Sélectionnez un appareil"));
```

Helper défini dans `include/ltr/ui/utf8.hpp`.

Tous les widgets maison (`Label`, `Button`, etc.) appliquent `utf8()`
automatiquement. Si tu crées un `sf::Text` manuellement (rare), n'oublie
pas.

## Dimensions actuelles des écrans

**MainScreen** (`src/ui/screens/main_screen.cpp`) :

- Header : 64 px de haut
- Sidebar : 300 px de large
- Barre transferts : 104 px de haut
- DeviceListItem : 64 px
- FileRow : 48 px
- Fenêtre par défaut : 1040 × 680 px (redimensionnable, min 800 × 500)

**IncomingOfferScreen** (modale) :

- Card : 480 × 360 px, centrée
- Boutons : 180 × 44 px, aux coins inférieurs
- PIN : taille 56, centré, sur fond `accentLight`

## Ajouter un nouveau widget — checklist

- [ ] Header `include/ltr/ui/widgets/xxx.hpp` + impl `src/ui/widgets/xxx.cpp`
- [ ] Ajouté à `add_library(ltr_ui ...)` dans `CMakeLists.txt`
- [ ] Textes via `utf8()`
- [ ] Couleurs via `Colors::...`, tailles via `Spacing::...`, rayons via
      `Radius::...`, police via `FontSize::...`
- [ ] Formes arrondies via `RoundedRect` (pas de `sf::RectangleShape`)
- [ ] État hover / pressed / disabled si cliquable
- [ ] API chainable `setXxx() -> Xxx&`
- [ ] `handleEvent(const sf::Event&)` + `draw(sf::RenderTarget&) const`
