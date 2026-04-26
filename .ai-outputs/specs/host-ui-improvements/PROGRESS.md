# Host UI/UX — Roadmap & Suivi

> Document de suivi des améliorations UX/UI de l'**app host (desktop SFML)**.
> À lire en début de session pour reprendre le travail au bon sprint.

**Créé :** 2026-04-24
**Méthode :** chaque sprint passe par `/feature full` → BA → Archi → UI/UX → Dev → Audit → Doc
**Contexte projet :** C++17 / SFML 2.6 / CMake / window 1100×680

---

## 📊 État global

| Sprint | Titre | Statut | Spec folder |
|--------|-------|--------|-------------|
| UX-1 | Hygiène visuelle (icônes, hiérarchie boutons, empty states) | ✅ livré 2026-04-24 | `.ai-outputs/specs/host-ux-1/` |
| UX-2 | Zone Transferts robuste (scroll, actions, annulation) | ✅ livré 2026-04-24 | `.ai-outputs/specs/host-ux-2/` |
| UX-3 | Drag & drop OS (macOS + Windows) | ✅ livré 2026-04-24 | `.ai-outputs/specs/host-ux-3/` |
| UX-4 | SharePanel collapsible + PIN/QR agrandis | ✅ livré 2026-04-24 | `.ai-outputs/specs/host-ux-4/` |
| UX-5 | Confort (dark mode, raccourcis clavier, dirty-flag redraw) | ⏳ à venir | `.ai-outputs/specs/host-ux-5-*/` |

Légende : ⏳ à venir · 🟡 en cours · ✅ livré · ⛔ bloqué

---

## 🔍 Analyse initiale (2026-04-24)

Source : lecture de `src/ui/screens/main_screen.cpp`, `src/ui/widgets/*.cpp`,
`include/ltr/ui/theme.hpp`, app lancée pour smoke visuel.

### Architecture actuelle (rappel)
- Fenêtre 1100×680, 5 zones fixes
- Header 64 px (logo LocalTransfer + pill self name)
- Sidebar gauche 300 px (zone Rechercher + liste APPAREILS)
- Centre flex (titre + FICHIERS À ENVOYER + 4 boutons)
- SharePanel droite 240 px (QR + URL + Copier + PIN)
- Zone bas 104 px (TRANSFERTS avec cards inline)

### 🔴 Problèmes critiques
1. **Pas de drag & drop** fichiers/dossiers depuis Finder/Explorer
2. **Zone TRANSFERTS se tronque silencieusement** au-delà de 3-4 transferts (`main_screen.cpp:576`)
3. **Empty state sidebar trompeur** : "Recherche en cours..." même quand le scan est stabilisé
4. **SharePanel toujours affiché** (240 px = 22% écran) même sans visiteur web

### 🟠 Problèmes majeurs
- **Sidebar** : pas de scroll si > 7 pairs, pastille verte toujours allumée, pas de filtre/recherche, zone "RECHERCHER" lourde (132 px)
- **Centre** : `Fichiers...` et `Dossier...` se confondent (tous Secondary), aucune icône sur boutons, préfixe `[D]` moche, glyphe `v` au lieu de `✓`, retirer un fichier peu découvrable (✕ au hover)
- **SharePanel** : PIN 28 px (petit à 1 m), QR 160 px (petit pour scan smartphone), pas de "Copier PIN", `CODE D'ACCES` sans accent
- **Transferts** : direction codée en texte (`→ Mac`), barre 4 px sans `%` intégré, séparateur `·` dense, emojis `✓✗` dépendants police, pas d'annulation InProgress, aucune action post-transfert

### 🟡 Problèmes mineurs / polish
- Header : self name brut sans icône machine, aucun statut réseau, aucun accès settings
- Empty state centre : texte "Parcourir" obsolète (`main_screen.cpp:459`)
- Redraw 60 FPS constant (pas de dirty flag)
- Pas de dark mode, pas de raccourcis clavier documentés
- Pas de notification OS native (NSUserNotificationCenter / WinRT toast)
- 100% français en dur (pas d'i18n)
- Aucun support clavier/screen reader

---

## 📋 Sprints détaillés

### Sprint UX-1 — Hygiène visuelle ✅
**Objectif :** petits fixes visuels à fort impact, pas de nouveau flux.

Livrables :
- [x] Checkbox `✓` → sprite PNG blanc via IconLibrary (remplace glyphe "v")
- [x] Icône dossier 20×20 → sprite PNG accent (remplace préfixe "[D]")
- [x] Icône croix 16×16 → sprite PNG textSecondary (remplace texte "x")
- [x] Icônes flèche transfert (↑ accent = envoi, ↓ success = réception) en tête de card
- [x] Hiérarchie boutons zone centrale : un seul bouton `Ajouter ▾` avec DropdownMenu à 2 items
- [x] Pill `Local` sur DeviceListItem (symétrie avec `Web`)
- [x] Empty state sidebar 2 états : radar pulsant 5 s → globe barré statique
- [x] Texte obsolète « Parcourir » → « Ajouter »

**Approche PNG choisie :** scripts/generate_icons.py (Python + PIL)
génère offline 7 PNG dans `assets/icons/`, embarqués via `ltr_embed_file`,
chargés lazy dans `IconLibrary` singleton (sf::Texture cache), rendus
comme `sf::Sprite` teinté via `setColor`.

### Sprint UX-2 — Zone Transferts robuste ✅
**Objectif :** scalabilité de la zone basse + actions utiles.

Livrables :
- [x] Scroll horizontal + flèches L/R + molette dans la zone
- [x] Direction par icône vectorielle (hérité UX-1)
- [x] Barre de progression 6 px (au lieu de 4)
- [x] Bouton Annuler pour les transferts `InProgress` + `Accepted` (natif TCP + web streaming via cancelFlag atomique)
- [x] Action « Ouvrir le dossier » pour Incoming+Done (via `openInFileManager` cross-platform)
- [x] Hiérarchie status : `%` en gros + speed/eta en petit secondaire
- [x] Auto-clean Done (10 s) / Failed-Cancelled (30 s) / Expired garde

**Reporté à UX-5 :** notifications OS natives (out of scope décidé en BA Q4=B).

**Cancel flow côté serveur :** `WebService::acquireCancelFlag(sid)` est
appelé au début de chaque GET ; le provider capture le `shared_ptr<atomic<bool>>`
et vérifie `.load()` à chaque chunk. `AppController::cancelSession`
route vers client TCP + server TCP + WebService selon disponibilité.

### Sprint UX-3 — Drag & drop OS ✅
**Objectif :** amener les fichiers dans l'app sans dialog.

Livrables :
- [x] Glue macOS via `class_addMethod` sur la contentView + `NSPasteboardTypeFileURL`
- [x] Glue Windows via `WM_DROPFILES` + `SetWindowSubclass`
- [x] Bridge callback → `AppController::addFiles(paths)` (thread-safe, appel direct UI thread)
- [x] Highlight zone centrale : overlay accentLight alpha 40 + bordure 2 px + texte « Déposer pour ajouter »
- [x] Stub Linux no-op (warning log)
- [x] Pattern documenté dans `docs-agents/UI_GUIDELINES.md`

**Limitation V1 :** Windows n'a pas d'event drag-over (seul le drop final fire via WM_DROPFILES) → pas de highlight pendant le drag côté Windows. Upgrade V2 via `IDropTarget` COM si besoin.

### Sprint UX-4 — SharePanel collapsible + PIN/QR agrandis ✅
**Objectif :** récupérer 240 px d'écran quand non utilisé, agrandir ce qui est lu à distance.

Livrables :
- [x] Toggle intégré au SharePanel : croix coin haut-droit (expanded) / clic sur rail entier (collapsed)
- [x] État collapsed = rail 40 px avec icône QR + badge numérique si N visiteur(s) web
- [x] QR agrandi à **220 px** quand déplié
- [x] PIN agrandi à **44 px** bold accent avec kerning (espaces entre chiffres)
- [x] 2 boutons séparés « Copier URL » + « Copier PIN » (feedback transitoire « Copié ! » 2 s)
- [x] « CODE D'ACCÈS » avec accent correct (`\xC3\x88`)
- [x] État persisté dans `infra::Config::sharePanelCollapsed` (JSON champ optionnel, default false)
- [x] MainScreen `sharePanelWidth()` dynamique (40 ou 240) avec re-layout au toggle

### Sprint UX-5 — Confort
**Objectif :** ergonomie de long usage.

Livrables :
- [ ] Dark mode : `Theme::dark()` avec palette inverse + toggle settings (persistance config)
- [ ] Raccourcis clavier :
  - `Space` ou `Enter` = envoyer
  - `Cmd/Ctrl+V` dans la sidebar = paste IP dans l'input
  - `Delete/Backspace` avec un fichier focus = retirer
  - `Cmd/Ctrl+,` = settings
- [ ] Dirty-flag redraw : UiApp redessine uniquement si AppState change (pas 60 FPS constant)

---

## 📌 Hors scope (V2 ou plus tard)

- i18n (refacto des strings éparpillés)
- Screen reader / accessibilité SFML
- Scroll sidebar + filtre de recherche par nom
- Preview miniatures images
- Thème "High Contrast"
- Settings UI (dialog dédié)
- Onboarding / welcome tour premier lancement

---

## 📝 Journal

### 2026-04-24
- Analyse initiale livrée (6 dimensions, 5 sprints identifiés)
- Ce document créé pour suivi inter-sessions
- **Sprint UX-1 livré** (score audit 93/100, 8/8 tests, 0 régression)
  - Nouveau module `ltr::ui::IconLibrary` + `DropdownMenu`
  - 7 icônes PNG générées via `scripts/generate_icons.py`
  - `UiTransfer::direction` migré `std::string` → `enum TransferDirection`
  - Refs : `.ai-outputs/specs/host-ux-1/` (business-spec + architecture + ui-proposal + audit-report)
- **Sprint UX-4 livré** (score audit 93/100, 8/8 tests, 0 régression)
  - `infra::Config::sharePanelCollapsed` (JSON persistant optionnel)
  - `AppController::toggleSharePanel()` + `isSharePanelCollapsed()`
  - `SharePanel` double rendu : expanded (QR 220 / PIN 44 / 2 boutons Copier) + collapsed (rail 40 px avec icône QR + badge visiteurs)
  - Nouvelle icône `qr.png` dans IconLibrary
  - `MainScreen::sharePanelWidth()` dynamique (40 ou 240)
  - « CODE D'ACCÈS » avec accent correct
  - Refs : `.ai-outputs/specs/host-ux-4/`
- **Sprint UX-3 livré** (score audit 94/100, 8/8 tests, 0 régression)
  - `ltr::ui::DragDropHandler` pImpl + 3 backends (mac / win / stub)
  - macOS : `class_addMethod` runtime pour NSDraggingDestination
  - Windows : `SetWindowSubclass` + `WM_DROPFILES` (sans events drag-over V1)
  - Linux : stub no-op
  - Highlight zone centrale via `MainScreen::setDragOver(bool)`
  - Refs : `.ai-outputs/specs/host-ux-3/`
- **Sprint UX-2 livré** (score audit 87/100, 8/8 tests, 0 régression)
  - Scroll horizontal avec flèches L/R + molette dans zone Transferts
  - `WebService::acquireCancelFlag` + vérif chunk-par-chunk côté providers
  - `AppController::tick` avec auto-clean Done 10 s / Failed-Cancelled 30 s
  - `AppController::openDownloadDir` via nouveau helper `core::openInFileManager`
  - Cards 340×64 avec boutons contextuels (Annuler / Ouvrir le dossier)
  - Hiérarchie status `%` gros + speed/eta petit
  - Refs : `.ai-outputs/specs/host-ux-2/` (business-spec + architecture + ui-proposal + audit-report)
