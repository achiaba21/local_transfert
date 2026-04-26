# 🎨 Design UI Validé — `local-file-transfer`

**Option choisie** : **B — Desktop 2 colonnes**
**Validé le** : 2026-04-18

---

## 1. Vision générale

Interface desktop horizontale, inspirée des clients mail / FTP modernes.
Deux colonnes principales :
- **Gauche** (sidebar) : liste des appareils découverts
- **Droite** (panneau central) : sélection de fichiers + action d'envoi
- **Bas** (bande fixe) : transferts actifs visibles en permanence

---

## 2. Dimensions & Fenêtre

| Propriété | Valeur |
|-----------|--------|
| Taille par défaut | **960 × 600 px** |
| Taille minimale | 800 × 500 px |
| Redimensionnable | Oui |
| Titre fenêtre | `LocalTransfer — [nom appareil]` |
| Icône | `assets/icons/app.png` (fournir au dev) |

---

## 3. Palette de couleurs

| Usage | Hex | SFML `sf::Color` |
|-------|-----|------------------|
| Fond général | `#FFFFFF` | `sf::Color(255, 255, 255)` |
| Sidebar | `#F3F4F6` | `sf::Color(243, 244, 246)` |
| Accent principal | `#4F46E5` | `sf::Color(79, 70, 229)` |
| Accent hover | `#4338CA` | `sf::Color(67, 56, 202)` |
| Texte principal | `#1A1D23` | `sf::Color(26, 29, 35)` |
| Texte secondaire | `#6B7280` | `sf::Color(107, 114, 128)` |
| Séparateurs | `#E5E7EB` | `sf::Color(229, 231, 235)` |
| Succès | `#22C55E` | `sf::Color(34, 197, 94)` |
| Erreur | `#EF4444` | `sf::Color(239, 68, 68)` |
| Warning | `#F59E0B` | `sf::Color(245, 158, 11)` |

---

## 4. Typographie

- **Font principale** : **Inter** (ou alternative libre : Roboto, Open Sans)
- **Fichier** : `assets/fonts/Inter-Regular.ttf` + `Inter-Bold.ttf`

| Rôle | Taille | Poids |
|------|--------|-------|
| H1 (titres écrans) | 20 px | Bold |
| H2 (sections) | 14 px | Bold |
| Body | 13 px | Regular |
| Small / caption | 11 px | Regular |
| Button | 14 px | Bold |

---

## 5. Espacements (base : 4 px)

| Token | Valeur |
|-------|--------|
| `xs`  | 4 px  |
| `sm`  | 8 px  |
| `md`  | 12 px |
| `lg`  | 16 px |
| `xl`  | 24 px |
| `2xl` | 32 px |

---

## 6. Layout principal

```
┌────────────────────────────────────────────────────────────┐
│  Header (48 px)                                            │
├─────────────────────┬──────────────────────────────────────┤
│  Sidebar gauche     │  Panneau central                     │
│  (280 px)           │  (flex)                              │
│                     │                                      │
│  [liste pairs]      │  [zone fichiers + bouton envoi]     │
│                     │                                      │
│                     │                                      │
├─────────────────────┴──────────────────────────────────────┤
│  Barre transferts actifs (72 px)                           │
└────────────────────────────────────────────────────────────┘
```

### 6.1 Header (48 px)
- Logo + titre "LocalTransfer" à gauche
- Nom de l'appareil courant au centre-droit
- Bouton ⚙️ Settings tout à droite (V1 : placeholder seulement)

### 6.2 Sidebar (280 px — fixe)
- Titre de section : "APPAREILS (N)"
- Liste verticale des `DeviceListItem`
- Scrollable si > 6 appareils
- Séparateur fin entre chaque élément

### 6.3 Panneau central (flex)
- En-tête : "Envoyer à : [nom]" ou "Sélectionnez un appareil"
- Liste des fichiers sélectionnés (scrollable)
- Bouton `[📂 Parcourir...]`
- Ligne totale : "Total : X Go"
- Bouton principal `[ENVOYER]` (disabled tant que pas de pair + fichier)

### 6.4 Barre transferts (72 px — fixe)
- Titre : "TRANSFERTS ACTIFS"
- Liste horizontale (scroll H si plusieurs) :
  `→ [pair]  [fichier]  [%]  [progress]  [vitesse]`

---

## 7. Composants à créer

### 7.1 Widgets réutilisables

| Widget | Rôle | État |
|--------|------|------|
| `Button` | Bouton avec texte + hover/pressed/disabled | primary / secondary / danger |
| `Label` | Texte simple avec taille + couleur | — |
| `ProgressBar` | Barre de progression | — |
| `DeviceListItem` | Ligne appareil (icône + nom + IP + état + checkbox) | selected / hover / default |
| `FileRow` | Ligne fichier sélectionné (icône + nom + taille + ✕) | — |
| `Card` | Conteneur avec fond, bordure, shadow | — |
| `TextInput` | Champ texte simple (utile pour renommage) | focus / default |

### 7.2 Écrans

| Écran | Fichier | Rôle |
|-------|---------|------|
| `MainScreen` | `screens/main_screen.{hpp,cpp}` | Layout principal (header + sidebar + panneau + barre) |
| `IncomingOfferOverlay` | `screens/incoming_offer_screen.{hpp,cpp}` | Modale semi-transparente avec PIN + accept/reject |
| `TransferStatusBar` | Composant inclus dans MainScreen | Liste horizontale des transferts actifs |

> **Note** : L'architecture initiale prévoyait un `TransferScreen` dédié. On
> remplace par une **barre de transferts intégrée** dans `MainScreen` pour
> respecter le layout Option B (multi-transferts toujours visibles).

### 7.3 Modale `IncomingOfferOverlay`

- Overlay semi-transparent : fond `sf::Color(0, 0, 0, 128)` sur toute la fenêtre
- Carte centrée : 400 × 320 px, fond blanc, coins arrondis (simulés via
  rectangles + cercles)
- Code PIN en très gros (48 px, espacement lettres)
- Boutons `Refuser` (secondary) et `Accepter` (primary)

---

## 8. Interactions & états

### 8.1 Sélection d'appareils
- **Clic** sur un `DeviceListItem` → toggle sélection (checkbox à droite)
- Plusieurs appareils peuvent être sélectionnés (broadcast)
- Le panneau central affiche "Envoyer à : 2 appareils" si > 1

### 8.2 Sélection fichiers
- Clic sur `[📂 Parcourir...]` → ouvre dialogue natif (tinyfiledialogs)
- Sélection multi-fichiers OU dossier
- Liste affichée avec option `✕` pour retirer un fichier

### 8.3 Envoi
- Bouton `ENVOYER` disabled si :
  - aucun appareil sélectionné, OU
  - aucun fichier sélectionné
- Clic → envoi démarre → ligne apparaît dans la barre de transferts

### 8.4 Réception d'une offre
- Overlay apparaît par-dessus tout
- Écran principal non cliquable tant que l'overlay est ouvert
- Code PIN affiché pendant 30 s (timeout auto-refus si pas de réponse)

### 8.5 Transfert actif
- Ligne dans la barre du bas
- Clic sur une ligne → ouvre mini-popover avec détails + bouton Annuler
- À la fin : ligne reste 5 s en vert (✓ Terminé) puis disparaît

---

## 9. Règles visuelles

- **Pas de bordures 1 px grises** inutiles entre chaque élément — utiliser
  l'espacement et les fonds pour séparer
- **Coins arrondis** : simuler via `sf::RectangleShape` + 4 cercles, OU via
  texture générée une fois
- **Hover states** : changement de fond uniquement (pas d'animation complexe)
- **Pas d'animations** en V1 (hors progression naturelle de la barre)
- **Curseur** : changer vers `sf::Cursor::Hand` sur les éléments cliquables

---

## 10. Éléments à fournir par l'utilisateur (optionnels)

- Icône application (`assets/icons/app.png`, 256×256) — **optionnel**, une
  icône générique peut être générée
- Font Inter (ou autre libre) — si non fournie, le dev la télécharge

---

## 11. Contraintes dev

- **Pas d'ImGui, pas de Qt** : rendu 100 % SFML
- **Layout manuel** : pas de moteur de layout, positions calculées à la main
  ou via helper (pas besoin de flexbox)
- **Un widget = une classe, une paire `.hpp`/`.cpp`**
- **Thème centralisé** : tous les styles passent par `ui/theme.hpp`
- **Navigation simple** : un `UIApp` contient le `MainScreen` actif ; l'overlay
  d'offre entrante est dessiné par-dessus si `AppState.incomingOffer` est
  présent (pas de stack d'écrans)
