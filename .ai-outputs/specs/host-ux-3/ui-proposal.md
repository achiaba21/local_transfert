# UI/UX — Sprint UX-3 Drag & drop OS

**Date :** 2026-04-24

Les choix haut-niveau sont validés (Q2=C, combinaison highlight + texte
inline). Mockup unique de référence.

---

## État normal (aucun drag en cours)

```
┌─────────────────────────────────────────────────────────────────┐
│  LocalTransfer                                 Mac de Serge     │
├─────────┬───────────────────────────────────────┬───────────────┤
│ RECHERCHER                                       PARTAGE WEB    │
│ [Rescan]│       Sélectionnez un appareil        │               │
│ [192.   │                                        │  ┌──────────┐│
│  168. +]│  FICHIERS À ENVOYER                    │  │ QR code  ││
│         │  ┌──────────────────────────────────┐ │  │  240×240 ││
│ APPAREIL│  │                                   │ │  └──────────┘│
│ · Mac   │  │   Aucun fichier sélectionné       │ │              │
│ · iPhone│  │   Cliquez sur « Ajouter » …       │ │  URL : ...   │
│         │  │                                   │ │  PIN  : ...  │
│         │  └──────────────────────────────────┘ │              │
│         │  [ Ajouter ▾ ] [Vider]     [ENVOYER] │              │
├─────────┴───────────────────────────────────────┴───────────────┤
│ TRANSFERTS · 0           Aucun transfert en cours.              │
└─────────────────────────────────────────────────────────────────┘
```

## État drag en cours (macOS uniquement — voir archi §4 Windows limitation)

```
┌─────────────────────────────────────────────────────────────────┐
│  LocalTransfer                                 Mac de Serge     │
├─────────┬───────────────────────────────────────┬───────────────┤
│ RECHERCHER                                       PARTAGE WEB    │
│ [Rescan]│       Sélectionnez un appareil        │               │
│ [192.   │  ╔═══════════════════════════════════╗ │  ┌──────────┐│
│  168. +]│  ║ (overlay accentLight α=40, border ║ │  │ QR code  ││
│         │  ║  2 px accent, radius md)          ║ │  │          ││
│ APPAREIL│  ║                                   ║ │  └──────────┘│
│ · Mac   │  ║                                   ║ │              │
│ · iPhone│  ║     Déposer pour ajouter          ║ │  URL : ...   │
│         │  ║     (FontSize::h1, accent)        ║ │  PIN  : ...  │
│         │  ║                                   ║ │              │
│         │  ╚═══════════════════════════════════╝ │              │
│         │  [ Ajouter ▾ ] [Vider]     [ENVOYER] │              │
├─────────┴───────────────────────────────────────┴───────────────┤
│ TRANSFERTS · 0           Aucun transfert en cours.              │
└─────────────────────────────────────────────────────────────────┘
```

### Détails rendu
- **Overlay** : `Colors::accentLight` avec `alpha = 40/255` (très léger,
  ne masque pas le contenu sous-jacent). Bordure 2 px `Colors::accent`
  bien visible. Radius::md.
- **Texte** : « Déposer pour ajouter », FontSize::h1, bold, accent.
  Centré verticalement dans la zone centrale.
- **Position** : l'overlay recouvre `centerRect_` - 8 px de chaque côté
  pour laisser une respiration.
- **Autres zones** (sidebar / share / header / transfers) : inchangées
  visuellement — elles acceptent quand même le drop (Q1=A) mais ne
  highlightent pas (choix esthétique : le centre sert de feedback
  unique).

### Transition
- `draggingEntered:` → `setDragOver(true)` → overlay s'affiche au
  prochain frame
- `draggingExited:` / `performDragOperation:` → `setDragOver(false)` →
  overlay disparaît

Pas d'animation de fade V1 (instantané — la latence des events OS
rend le toggle perceptible comme « immédiat »).

---

## Windows : pas de feedback visuel V1

Sur Windows, seul le drop final émet un event (`WM_DROPFILES`). Pas de
`draggingEntered` → pas de highlight pendant le drag. L'utilisateur voit
les fichiers apparaître d'un coup dans la liste au moment du drop.

Documenté comme limitation V1 dans `docs-agents/UI_GUIDELINES.md`.
Upgrade possible V2 via COM `IDropTarget`.

---

Les choix sont alignés avec l'existant (tokens Theme). **Pas de
nouvelle validation requise — on passe au Dev.**
