# UI/UX Proposal — Sprint Web P2P V1.3

**Date :** 2026-05-02
**Statut :** ✅ Validée

---

## Surface 1 — Tabs Host / P2P dans la footer

```
┌─────────────────────────────────────┐
│ TRANSFERTS                          │
│ ┌──────────┬──────────┐             │
│ │  Host 1  │  P2P 4   │             │
│ └──────────┴──────────┘             │
│ [contenu de la tab active]          │
└─────────────────────────────────────┘
```

- Pills indigo (active) / gris (inactive)
- Compteurs entre parenthèses
- Hauteur tab 36 px
- Switch instantané via [hidden] swap

## Surface 2 — Lignes entry P2P

```
✓  photo1.jpg                  →
   2.4 Mo · 🦊 Pingouin Bleu · 13:42

↻  video.mp4                   →
   67 % · 8 Mo/s · 🦊 Pingouin Bleu
   ████████░░░░░░░░

✗  doc.pdf       [↻ Réessayer]  →
   Échec réseau · 🦊 Pingouin Bleu

⏱  rapport.docx               →
   En attente · 🦊 Pingouin Bleu

✓  slides.pdf                  ←
   4.1 Mo · 🐧 Lapin Vif · 13:38
```

### Tokens
- ✓ success / ↻ accent (pulse animée) / ✗ error / ⏱ textSecondary
- Nom fichier body 14 px, ellipsis
- → envoi / ← réception (textSecondary, droite)
- Sous-titre body small + peer emoji+nom + heure ou
  progression + vitesse + peer
- Barre 3 px accent visible uniquement en `↻ sending`
- Bouton Réessayer ligne `failed`-out, compact accent
- Hauteur ligne ~56 px (multi-ligne)

### Auto-collapse > 10 entrées
```
[10 récentes]
··· 12 entrées plus anciennes
[Voir tout]
```

## Surface 3 — Notification complétion

- Toast existant + son court + `navigator.vibrate(200)`
- Position centré bas au-dessus footer, durée 2,5 s
- Couleur success

## Tokens
- `--success`, `--accent`, `--error`, `--text-secondary`
- Tous existants dans `:root`

## Animations
- `↻` : rotation 1 s linear infinite
- Nouvelle entry : fade-in + slide-down 200 ms
- Pas d'animation lourde

## Mobile-first
- Cible 360 px : tout tient sans wrap horizontal
- Touch target ≥44 px pour boutons
- Scroll vertical only
