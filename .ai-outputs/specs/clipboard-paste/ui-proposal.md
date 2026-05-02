# UI/UX Proposal — Sprint Clipboard Paste (V1.4)

**Date :** 2026-05-02
**Statut :** ✅ Validée

## Surface 1 — 3e entrée menu « Ajouter ▾ »

```
┌─────────────────────┐
│  Fichiers...        │
│  Dossier...         │
├─────────────────────┤
│  📋 Coller   ⌘V    │
└─────────────────────┘
```

- Icône 📋, texte « Coller »
- Hint raccourci à droite (⌘V Mac / Ctrl+V Win)
- Séparateur visuel entre Fichier/Dossier et Coller

## Surface 2 — Toast feedback (réuse existant)

- ✓ « 3 fichiers ajoutés depuis le presse-papier »
- ✓ « Texte ajouté · 1.2 Ko »
- ✓ « Image PNG ajoutée · 240 Ko »
- ⚠ « Presse-papier vide »
- ✗ « Format non supporté »

## Surface 3 — Bouton web

```
[Choisir des fichiers] [Choisir dossier] [📋 Coller]
```

- Style btn-secondary
- Affiché uniquement si `navigator.clipboard.read` dispo
- Touch target 44 px, pas de wrap < 360 px

## Tokens
- accent / success / warning / error
- Spacing sm/md, FontSize body / small
