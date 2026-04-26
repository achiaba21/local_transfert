# 🎨 Design UI Validé — Interface Web LocalTransfer

> **Feature** : web-interface
> **Statut** : ✅ Validé par l'utilisateur (choix : "tes recommandations")
> **Date** : 2026-04-22

---

## Synthèse des choix validés

| Surface | Option retenue |
|---|---|
| 1 — Page web | **C — Split adaptive** (2 colonnes desktop / stack mobile) |
| 2 — SharePanel desktop | **A — Card permanente à droite** (3ᵉ colonne dans MainScreen) |
| 3 — Icône Native vs Web | **C — Pill "Web"** à droite (style aligné sur pill header) |

---

## 🌐 Surface 1 — Page web (Option C : Split adaptive)

### Principe
Sur écrans **≥ 720 px** : deux colonnes côte à côte (Envoyer / Reçus du host).
Sur écrans **< 720 px** : stack vertical (Envoyer puis Reçus).
Barre des transferts en bas fixe (analogue au MainScreen SFML).

### Zones & composants

#### Header (hauteur 56 px)
- Dot indigo 10×10 + `"LocalTransfer"` bold 24
- Nom du host dans pill `accentLight` à droite (comme le MainScreen SFML)

#### Bandeau install app native (cond. `sameOs == true`)
- Card `surface` pleine largeur, radius `md`, fermable `×`
- Texte : "Installer l'app pour macOS — Transferts plus rapides"
- Bouton `[ Installer ]` variant Primary → GET `/download/self`
- Si `sameOs == false` → bandeau devient "Voir sur GitHub" (lien externe)
- Stockage "fermé" dans `sessionStorage` (réapparaît au prochain onglet, pas pendant la session en cours)

#### Colonne gauche — "ENVOYER"
- Overline `"ENVOYER"` + petit sous-texte "Glisser-déposer ou cliquer"
- **Drop zone** : radius `lg`, bordure dashed `accent` 2 px, fond `accentLight` quand hover ou drag-over
- Icône centrale `⬆` taille 48, couleur `accent`
- Texte "Déposez vos fichiers ici" `body` bold + "ou cliquez pour choisir" `small` `textSecondary`
- Input `<input type="file" multiple>` masqué, déclenché par clic sur la zone
- Liste des fichiers en cours d'upload (sous la drop zone) avec mini progress bar `accent`

#### Colonne droite — "REÇUS DU HOST"
- Overline `"REÇUS DU HOST · N"`
- Liste de FileRow-web :
  - Icône `📄/🎵/🎞/📦` selon extension, fallback `📄`
  - Nom + taille à gauche
  - Bouton `[ Télécharger ]` variant Primary à droite
- Empty state : "Aucun fichier reçu pour le moment"

#### Barre transferts (bas fixe)
- Overline `"TRANSFERTS · N"`
- Cards mini 280 px larges en ligne horizontale scrollable
- Contenu par card : direction `↑/↓` + nom, %, vitesse, progress bar 4 px
- Couleur de la barre : `accent` en cours, `success` terminé, `error` échec (aligné MainScreen)

### État "auth" (premier contact)
- Fond `bg`, une seule card centrée `surface` 520×360 radius `lg`
- Titre "Entrez le code d'accès" `h1` bold
- **6 cases PIN** (inputs `type="tel" maxlength="1"`) de 48×56 px, radius `md`, bordure `separator`, focus → bordure `accent`
- Texte sous les cases : "Affiché sur « Mac de Serge »" (nom du host) en `small` `textSecondary`
- Bouton `[ Se connecter ]` Primary pleine largeur
- Auto-submit quand 6 cases remplies
- Erreur PIN → cases passent en bordure `error`, message "Code incorrect" sous les cases

### Breakpoint responsive

```css
@media (min-width: 720px) {
  /* layout split — 2 colonnes 1fr 1fr */
}
/* mobile par défaut : stack vertical, 1 colonne */
```

### Tokens CSS alignés sur le theme SFML

```css
:root {
  --bg: #FAFAFB;
  --surface: #FFFFFF;
  --accent: #6366F1;
  --accent-hover: #4F46E5;
  --accent-light: #EEF2FF;
  --text: #0F172A;
  --text-secondary: #64748B;
  --separator: #E2E8F0;
  --success: #10B981;
  --error: #EF4444;
  --radius-sm: 6px;
  --radius-md: 10px;
  --radius-lg: 14px;
  --radius-pill: 999px;
  --spacing-xs: 4px;
  --spacing-sm: 8px;
  --spacing-md: 12px;
  --spacing-lg: 16px;
  --spacing-xl: 24px;
  --spacing-xxl: 32px;
  --font-h1: 24px;
  --font-body: 14px;
  --font-small: 12px;
  --font-overline: 11px;
  --font-pin: 56px;
  --shadow: 0 2px 8px rgba(0, 0, 0, 0.10);
}
```

### Accessibilité
- Contraste AA : text `#0F172A` sur `#FAFAFB` = 15:1 ✅, textSecondary sur bg = 7:1 ✅
- Focus visible sur tous les éléments interactifs (outline 2 px `accent`)
- Attribut `aria-label` sur drop zone + input file
- Boutons avec texte + icône (pas d'icône seule)

---

## 🖥️ Surface 2 — SharePanel desktop (Option A)

### Principe
Nouvelle colonne à droite du MainScreen SFML, toujours visible, affichant QR code + URL + PIN web.

### Impact layout MainScreen

Nouvelle fenêtre **1100 × 680 px** (vs 1040 × 680 actuel) :

```
┌────────────────────────────────────────────────────────────────┐
│  Header 64 px                                                  │
├────────────────────────────────────────────────────────────────┤
│ Sidebar 300  │  Centre 560                │ SharePanel 240    │
│              │                            │                   │
│              │                            │                   │
│              │                            │                   │
├──────────────┴────────────────────────────┴───────────────────┤
│  Bottom 104 px (transferts)                                   │
└────────────────────────────────────────────────────────────────┘
```

### Composant `ui::widgets::SharePanel`

Card `surface` pleine hauteur, radius 0 (colle au bord), bordure gauche 1 px `separator`.

#### Structure verticale

```
┌─────────────────────────────┐  ← top +spacing::xl
│ PARTAGE WEB                 │  ← Label overline textSecondary bold
│                             │
│  ┌─────────────────────┐    │  ← QR 160×160, fond blanc pur
│  │                     │    │     RoundedRect radius::md
│  │  █▄▀█▄▀█▄▀█▄▀█      │    │     bordure 1px separator
│  │  ▀▄█▀▄█▀▄█▀▄█▀      │    │
│  │  █▄▀█▄▀█▄▀█▄▀█      │    │
│  └─────────────────────┘    │
│                             │
│ URL                         │  ← Label overline textSecondary
│ 192.168.1.42:45456          │  ← Label body text, bold, mono-ish
│    [ Copier ]               │  ← Button Secondary, compact
│                             │
│ CODE D'ACCÈS                │  ← overline textSecondary
│                             │
│   4  7  2  9  3  1          │  ← FontSize::pin (56), accent
│                             │     espacés de spacing::md
│                             │
│ ─────────────────────────── │  ← separator 1px
│                             │
│ 📱  Scannez ce QR code ou   │  ← Label small textSecondary
│     tapez l'URL dans votre  │     texte wrapped sur 2-3 lignes
│     navigateur              │
└─────────────────────────────┘
```

#### API (header `include/ltr/ui/widgets/share_panel.hpp`)

```cpp
class SharePanel {
public:
    SharePanel& setBounds(const sf::FloatRect& r);
    SharePanel& setUrl(const std::string& url);
    SharePanel& setPin(const std::string& pin6);  // "472931"
    SharePanel& setQrImage(const sf::Image& img); // généré par QrCode

    const sf::FloatRect& bounds() const noexcept;
    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    std::string url_;
    std::string pin_;
    sf::Texture qrTexture_;
    Button copyBtn_;
    // ...
};
```

#### Interactions
- **Clic QR** → copie l'URL dans le presse-papier (`sf::Clipboard`)
- **Bouton "Copier"** → copie l'URL, feedback visuel "Copié ✓" pendant 2 s
- PIN non interactif (affichage seul)

#### Source des données
- `AppController::webShareInfo()` → retourne `{ url, pin, qrImage }`
- Mis à jour à chaque frame (PIN/URL stables, QR régénéré seulement si URL change)

### Modifications `MainScreen`
- Ajout membre `std::unique_ptr<SharePanel> share_`
- Dans `rebuildLayout()` : calcul `sharePanelRect_` = `{w - 240, kHeaderH, 240, h - kHeaderH - kBottomH}`
- Ajustement `centerRect_` : largeur réduite de 240 px
- Dans `draw()` : appel `share_->draw(target)` après `drawSidebar()`
- Dans `handleEvent()` : propagation à `share_`

### Constantes ajoutées (dans `main_screen.cpp` local)
```cpp
constexpr float kSharePanelW = 240.f;
constexpr float kQrSize      = 160.f;
```

---

## 🏷️ Surface 3 — Icône Native vs Web (Option C : Pill "Web")

### Principe
Ajouter une pill "Web" à droite du `DeviceListItem` quand `device.kind == PeerKind::Web`. Les pairs natifs restent inchangés (pas de pill "Native" — silence = natif).

### Rendu visuel

```
Natif (inchangé) :
┌──────────────────────────────────────┐
│  🟪  Mac de Lena                     │
│      macOS                     (○)   │
└──────────────────────────────────────┘

Web (NEW) :
┌──────────────────────────────────────┐
│  🟪  iPhone de Marc       [ Web ]    │
│      Safari · iOS             (○)    │
└──────────────────────────────────────┘
```

### Spec de la pill
- RoundedRect 40×18 px, radius `Radius::pill` (999 → ovale complet)
- Fill : `Colors::accentLight`
- Label : `"Web"`, `FontSize::overline` (11), bold, color `Colors::accent`
- Positionnement : x = right - spacing::md - pillW - spacing::md - checkboxW - spacing::md
- Vertical : centré sur la row 64 px

### Modifications `DeviceListItem`

**Header `include/ltr/ui/widgets/device_list_item.hpp`** — aucune modification d'API requise (`device_` contient déjà `kind`).

**Impl `src/ui/widgets/device_list_item.cpp`** :
- Dans `draw()`, juste avant la checkbox : si `device_.kind == domain::PeerKind::Web`, dessiner la pill
- Ajuster la largeur max du texte du nom pour ne pas chevaucher la pill (clip ou ellipsis)

### Différenciation nom/platform pour les sessions web
- `device.name` = `"iPhone (Safari)"` ou `"Windows (Chrome)"` (parsé du User-Agent)
- `device.platform` = `"iOS"`, `"Android"`, `"macOS"`, `"Windows"`, `"Linux"`, ou `"Other"`
- La 2ᵉ ligne du `DeviceListItem` affiche `device.platform` — reste cohérent

---

## 📐 Contraintes visuelles globales (rappel)

- **Couleurs** : via `Colors::*` uniquement (web via `:root` CSS aligné)
- **Espacements** : grille 4 px, `Spacing::*` côté SFML / vars CSS côté web
- **Radius** : `sm=6 / md=10 / lg=14 / pill=999`
- **Typographie** : Inter + fallback système
- **Icônes** : Unicode uniquement (`⬆`, `⬇`, `📄`, `🌐`, `📱`)
- **Pas d'animations V1** — hover et transition 100 ms max sur boutons
- **Ombres** : `shadow` discret (alpha 25, offset Y 2-4 px) — uniquement sur boutons Primary et cards principales

---

## 📝 Composants à créer

### Côté SFML C++
- `include/ltr/ui/widgets/qr_code_view.hpp` + `src/ui/widgets/qr_code_view.cpp`
- `include/ltr/ui/widgets/share_panel.hpp` + `src/ui/widgets/share_panel.cpp`

### Côté Web
- `assets/web/index.html` (structure + état auth + état principal dans le même fichier, switch par classe sur `<body>`)
- `assets/web/style.css` (tokens + layout split + responsive media query)
- `assets/web/app.js` (fetch, SSE, file upload, drag-drop, PIN input, download)
- `assets/web/icons/upload.svg`, `download.svg`, `file.svg`

### Composants réutilisés côté SFML
- `Card`, `RoundedRect`, `Button`, `Label`, `ProgressBar`, `DeviceListItem` (modifié pour la pill)

---

## ✅ Prochaine étape

→ Retour orchestrateur → Étape 4 (Plan d'action) → Étape 5 (Développement par `/agent-dev`).
