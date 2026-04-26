# Spec métier — Sprint UI Layout System

**Date :** 2026-04-26
**Statut :** ✅ validé utilisateur
**Branche git :** prochain commit branche `main` (snapshot précédent : `ea097d5`)

---

## 1. Contexte

L'app desktop SFML utilise un rendu immediate-mode avec :
- 91 appels à `Label` dans `main_screen.cpp` avec `setPosition(x, y)` calculés à la main
- 14+ constantes par écran (kHeaderH, kSidebarW, etc.)
- 3 systèmes de scroll ad-hoc (centre files, transfers horizontal, modale inbox)
- Aucun clipping → texte qui déborde des conteneurs
- Aucun breakpoint responsive → fenêtre <800 px = inutilisable

C'est un refactor MASSIF qui touche ~12 fichiers et ~3000 lignes.

---

## 2. Objectif

Refondre l'UI en système central et cohérent qui résout en une fois :
1. Texte qui déborde
2. Pas de clipping par zone
3. 3 scrolls ad-hoc → 1 ScrollArea unifié
4. Layout hardcoded → mini-DSL HBox/VBox
5. Pas de responsive → 3 breakpoints (Compact / Regular / Large)
6. Animations basiques (fade-in/out cards, slide drawer)
7. Support DPI HiDPI/Retina

---

## 3. Décisions produit validées

### Q1 = B — Compact mode <800 px : bouton hamburger
- Bouton ☰ ajouté dans le header
- Clic → bascule entre 2 layouts :
  - Sidebar visible + centre étroit
  - Centre seul (sidebar masquée)
- **Pas d'overlay sombre, pas d'animation slide complexe** — simple toggle
  layout flexbox
- SharePanel forcé collapsed en Compact mode (rail 40 px à droite)

### Q2 = A — Big bang
- Foundation + tous les écrans refactorés en **une seule livraison**
- Risqué mais cohérent : pas d'état hybride mi-old / mi-new
- Tous les `setPosition` manuels remplacés par HBox/VBox/Label avec maxWidth
- Tous les écrans/widgets passent en mode layout déclaratif d'un coup

### Q3 = B — Theming dynamique repoussé
- Dark mode reste planifié dans **sprint UX-5 dédié**
- Ce sprint reste focus sur la couche layout
- L'API du Theme reste static const pour V1, refactor en `ThemeProvider`
  fait plus tard

### Q4 = A — Animations incluses
- Fade-in 200 ms sur nouvelles cards (transferts, demandes inbox, file rows)
- Fade-out 200 ms avant suppression (auto-clean post-transfert)
- Slide-in pour drawer sidebar Compact (animation horizontale rapide)
- Helper d'animation centralisé dans le système layout

### Q5 = A — DPI / HiDPI inclus
- Détection auto Retina/HiDPI au démarrage (`window.getSettings()` ou
  via `sf::VideoMode::getDesktopMode()`)
- Scaling factor appliqué à toutes les tailles (FontSize, Spacing,
  Radius)
- Multi-écrans pris en compte au resize

---

## 4. Livrables (5 lots)

### Lot 1 — Text constraints (`Label` étendu)
- `setMaxWidth(w)` + `setEllipsis(bool)` : tronquage auto avec `…`
- `setAlignment(Left/Center/Right)` : positionnement auto
- Cache de mesure `(text, size, bold, maxW)` → `Vector2f`
- Application : tous les Label de l'app passent par `setMaxWidth`

### Lot 2 — Clipping (`ClipScope`)
- RAII helper `ClipScope(target, rect)` qui push/pop `sf::View`
- Application : autour de chaque zone (header, sidebar, centre, share,
  bottom transferts, modale inbox)
- Garantit qu'aucun dessin ne sort de sa zone

### Lot 3 — `ScrollArea` unifié
- Widget paramétrable (vertical / horizontal / les deux)
- État interne : contentH, contentW, scrollY, scrollX, clamping auto
- Molette + drag scrollbar
- Helper `forEachVisible(items, fn)` pour itérer sans dessiner les
  hors-zone
- Refactor des 3 scrolls existants (centre files, transferts, modale
  inbox) sur ce widget
- **Ajout** : sidebar APPAREILS scrollable (>7 pairs OK), modale inbox
  scrollable (>5 demandes OK)

### Lot 4 — Responsive breakpoints
- enum `Breakpoint { Compact, Regular, Large }` selon `viewSize.x`
  - Compact <800 : bouton ☰ pour toggle sidebar, sharePanel collapsed forcé
  - Regular 800-1300 : layout actuel
  - Large >1300 : sidebar +360 px, sharePanel +320 px
- Détection automatique au resize, propagation aux écrans via setter
- DPI scaling appliqué (Retina = ×2 typique)

### Lot 5 — Layout déclaratif HBox/VBox
- Mini-DSL :
  ```cpp
  HBox{}.spacing(8).padding(16)
      .child(Icon{...}.fixedWidth(20))
      .child(Label{...}.expanded())
      .child(Button{...}.fixedWidth(80))
      .layout(parentRect);
  ```
- Application progressive (mais dans la même livraison Big bang) :
  widgets simples d'abord (file_row, device_list_item, transfer_card,
  share_panel), puis screens (main_screen, incoming_offer_screen)

### Bonus — Animations
- `Animation` helper avec `easing` (linear, ease-out)
- Hook : nouvelle entrée dans une liste → fade-in 200 ms
- Hook : suppression d'une entrée → fade-out 200 ms avant remove effectif
- Slide-in horizontal pour drawer sidebar (Compact)

### Bonus — DPI scaling
- `dpiScale` détecté au lancement
- Multiplie toutes les `Spacing::*`, `FontSize::*`, `Radius::*` par
  `dpiScale`
- Au resize de fenêtre, recalcul si nécessaire (déplacement entre
  écrans)

---

## 5. Critères d'acceptation

- Fenêtre 600 × 800 (Compact) : bouton ☰ visible dans le header,
  sidebar masquée, clic ☰ → sidebar apparaît, centre se rétrécit
- Fenêtre 1100 × 680 (Regular) : layout actuel inchangé visuellement
- Fenêtre 1600 × 1000 (Large) : sidebar 360 px, sharePanel 320 px
- Nom de fichier de 100 caractères : `…tronqué…` avec `…` dans une row
- 20 pairs dans la sidebar : scroll vertical natif, dernière row OK
- 10 demandes dans la modale inbox : scroll vertical, toutes les rows
  accessibles via scroll
- Resize de la fenêtre en live : layout se met à jour fluide, sans
  flicker
- Nouvelle card transfer apparaît : fade-in 200 ms
- Card auto-clean après 10 s : fade-out 200 ms puis disparition
- Mac Retina 13" : tailles de texte/spacing visibles correctement (pas
  trop petites)
- Build Release propre, 9 tests passent, 0 régression sur les flows
  existants

---

## 6. Contraintes techniques

- C++17 strict
- Aucune nouvelle dépendance externe
- Tokens Theme respectés
- RoundedRect pour tout nouvel élément
- EventBus thread-safe pour comm thread → UI
- AUCUNE régression : couche web V1.1.8, sprints UX-1..4, transfer
  resume MVP, web-batch-ux
- 9 tests existants doivent continuer à passer
- Tests unitaires nouveaux : Label ellipsis, ScrollArea clamping,
  HBox/VBox layout
- Doc UI_GUIDELINES.md mise à jour avec tutoriel HBox/VBox/ScrollArea/
  Breakpoint/animations

---

## 7. Hors scope V1

- Theming dynamique dark/light (UX-5 dédié)
- Notifications OS natives (UX-5)
- Accessibilité screen reader / clavier (UX-5)
- Internationalisation (V2)
- Migration vers ECS / scene graph plus poussé (overkill pour la taille
  actuelle)
- Rendu vectoriel (SVG) à la volée — on garde les PNG embedded

---

## 8. Estimation

Sprint **massif** : ~10-15 j de dev humain, livré en 4 vagues IA :
1. **Wave 1 — Foundation** : Label étendu, ClipScope, Layout HBox/VBox,
   tests unitaires
2. **Wave 2 — Core widgets** : refactor file_row, device_list_item,
   transfer_card, share_panel, dropdown_menu sur HBox/VBox
3. **Wave 3 — Screens** : refactor main_screen + incoming_offer_screen,
   intégration ScrollArea, breakpoints, drawer Compact
4. **Wave 4 — Polish** : animations, DPI, tests audit final, doc
