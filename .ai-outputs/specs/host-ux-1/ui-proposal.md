# UI/UX — Sprint UX-1 mockups

**Date :** 2026-04-24

---

## 1. Menu Ajouter (zone centrale, bas)

### AVANT
```
┌────────────────────────────────────────────────────┐
│ [ Fichiers... ] [ Dossier... ] [Vider]   [ENVOYER]│
└────────────────────────────────────────────────────┘
 Secondary      Secondary       Sec.      Primary
 115 px         115 px          75 px     180 px
```
→ 3 boutons secondaires visuellement identiques = confusion.

### APRÈS — bouton ouvert
```
┌─────────────────────────────────────────┐
│ [ Ajouter ▾ ]  [Vider]        [ENVOYER]│
│  ┌────────────┐                         │
│  │ 📄 Fichiers...│ ←─ hover accentLight │
│  │ 📁 Dossier... │                      │
│  └────────────┘                         │
└─────────────────────────────────────────┘
 Primary         Sec.           Primary
 140 px          75 px          180 px
```

Menu : popup sous le bouton, fond surface, outline separator 1 px, shadow
subtile. 2 items × 32 px. Icônes 16×16 à gauche. Clic = action + close.
Clic extérieur = close sans action.

---

## 2. Pill Local (DeviceListItem)

### AVANT
```
┌──────────────────────────────────┐
│ ● MacBook Pro                    │  ← natif
│   macOS · 192.168.1.3      ○     │
├──────────────────────────────────┤
│ ● iPhone (Safari)        [Web] ○ │  ← web
│   iOS · 192.168.1.17             │
└──────────────────────────────────┘
```

### APRÈS (symétrie)
```
┌──────────────────────────────────┐
│ ● MacBook Pro        [Local] ○   │  ← natif
│   macOS · 192.168.1.3            │
├──────────────────────────────────┤
│ ● iPhone (Safari)     [Web]  ○   │  ← web
│   iOS · 192.168.1.17             │
└──────────────────────────────────┘
```

- Pill `Local` : même style que `Web` (accentLight bg, accent text,
  pill 46×18, radius pill).
- Position : juste à gauche de la checkbox ronde, centré vertical.
- Symétrie visuelle → l'utilisateur voit instantanément le canal.

---

## 3. Empty state sidebar — 2 états

### État A (t < 5 s, aucun pair) — radar pulsant
```
┌────────────────┐
│                │
│                │
│     ((•))      │  ← cercle accent pulsant (1.0 → 1.2 → 1.0)
│    ( ( O ) )   │     + point central accent fixe
│     ((•))      │
│                │
│  Recherche...  │  ← textSecondary, body
│                │
└────────────────┘
```
Animation : scale modulé par `sin(elapsed × 5)` sur le rayon externe.
Pas de rotation (lourd visuellement sur SFML).

### État B (t ≥ 5 s, aucun pair) — statique
```
┌────────────────┐
│                │
│                │
│     ╱─────╲    │  ← globe barré textSecondary
│    │   ╱   │   │     (cercle + diagonale)
│     ╲─────╱    │
│                │
│  Aucun appareil│
│     détecté    │
│                │
└────────────────┘
```
Icône grise neutre. Texte deux lignes centré.

### Transition
Fade A → B en 300 ms via alpha interpolé (simple, optionnel V2).
V1 : swap sec au passage du seuil.

---

## 4. Icônes vectorielles — previews 1:1

Rendues par `scripts/generate_icons.py` (PIL) à la taille cible,
monochrome + alpha, teintables via `sf::Sprite::setColor`.

```
check (24×24)    folder (20×20)    close (16×16)
   ▒▒             ░░░░░▒▒           ░    ░
  ▒▒             ░░░░░░░▒           ░░  ░░
 ▒▒  ▒▒         ▒▒▒▒▒▒▒▒▒           ░░░░
▒▒    ▒         ▒▒▒▒▒▒▒▒▒            ░░
▒▒▒▒▒▒          ▒▒▒▒▒▒▒▒▒           ░░░░
                                   ░░  ░░
                                   ░    ░

arrow-up (18×18)   arrow-down (18×18)
    ▲                   │
   ▲▲▲                  │
  ▲ ▲ ▲                 │
    │                 ▼ │ ▼
    │                  ▼▼▼
    │                   ▼
```
Teintes appliquées au dessin :
- check → blanc (sur fond accent)
- folder → accent
- close → textSecondary
- arrow-up → accent (envoi)
- arrow-down → success (réception)

---

## 5. FileRow (avant / après)

### AVANT
```
┌─────────────────────────────────────────────────────┐
│ [□] v   [D] Mon Dossier         125 Mo       x      │
│         12 fichiers                                 │
└─────────────────────────────────────────────────────┘
```
- `v` ASCII au lieu de ✓
- `[D]` préfixe texte au lieu d'icône
- `x` ASCII au lieu d'icône croix

### APRÈS
```
┌─────────────────────────────────────────────────────┐
│ [✓]     📁 Mon Dossier          125 Mo       ✕      │
│            12 fichiers                              │
└─────────────────────────────────────────────────────┘
```
- coche vectorielle blanche sur fond accent
- icône dossier 20×20 accent à gauche du nom
- croix 16×16 textSecondary à droite (hover → fond separator)

Fichier simple (pas un dossier) : pas d'icône avant le nom (le row
entier suffit, icônes uniquement pour les dossiers — cohérent avec
la liste Finder).

---

## 6. Card Transfert avec flèche directionnelle

### AVANT
```
┌──────────────────────────────────┐
│ →  MacBook Pro                   │  ← "→" en texte dans le label
│ 67 %  ·  15 Mo/s  ·  12s         │
│ ════════════════════════░░░░░░   │
└──────────────────────────────────┘
```

### APRÈS
```
┌──────────────────────────────────┐
│ ↑  MacBook Pro                   │  ← icône 18×18 accent
│    67 %  ·  15 Mo/s  ·  12s      │
│ ══════════════════════════░░░░   │
└──────────────────────────────────┘
```

ou réception :

```
┌──────────────────────────────────┐
│ ↓  iPhone (Safari)               │  ← icône 18×18 success (vert)
│    45 %  ·  8 Mo/s  ·  22s       │
│ ══════════════░░░░░░░░░░░░░░░░   │
└──────────────────────────────────┘
```

Icône 18×18, marge `Spacing::md` à gauche, label1 commence à
`tx + Spacing::md + 18 + Spacing::sm`.

---

## Validation design

Tous les mockups respectent :
- Tokens Theme (accent #6366F1, success #10B981, textSecondary #64748B,
  accentLight #EEF2FF, surface #FFFFFF, separator #E2E8F0)
- Spacing 4 px grid
- Radius::sm (6 px) pour les pills, icônes non arrondies
- Aucun glyphe Unicode dans les rendus finaux

**L'utilisateur a déjà validé les choix haut-niveau (Q1=A menu, Q2=A
Local, Q3=A 2 états, Q4=B PNG, Q5=A accent/success).** Les mockups
ci-dessus sont la concrétisation visuelle ; pas de nouvelle validation
requise — on passe au Dev.
