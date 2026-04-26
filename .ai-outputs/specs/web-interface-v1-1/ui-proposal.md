# 🎨 Design UI Validé — web-interface V1.1

> **Feature** : web-interface-v1-1
> **Statut** : ✅ Validé par l'utilisateur (choix : "tes recommandations")
> **Date** : 2026-04-22

---

## Synthèse des choix validés

| Surface | Option retenue |
|---|---|
| 1 — Page `/login` | **B — Plein écran minimal iOS-like** |
| 2 — Header `index.html` | **B hybride — icône mobile + libellé desktop** |
| 3 — Checkbox `FileRow` desktop | **A — Checkbox carrée accent + coche ✓** |
| 4 — Statuts « Proposé » / « Expiré » | **A — Libellés texte + couleur barre** |

---

## 🌐 Surface 1 — Page `/login` (Option B)

### Principe
Layout plein écran minimaliste, sans card. Éléments centrés verticalement et horizontalement. Style iOS-like, maximum de respiration.

### Structure HTML/CSS

```
┌────────────────────────────────────┐
│                                     │
│           ● LocalTransfer          │  ← brand dot 10px + nom
│                                     │
│                                     │
│        Code d'accès                │  ← h1 24px bold, text color
│        Affiché sur Mac de Serge    │  ← body 14px, text-secondary
│                                     │
│     ┌──┬──┬──┬──┬──┬──┐            │  ← 6 cases 46×56 px
│     │ 4│ 7│ 2│ 9│ 3│ 1│            │    font-size 24px bold
│     └──┴──┴──┴──┴──┴──┘            │    color accent
│                                     │
│         [ Se connecter ]           │  ← bouton primary max 260 px
│                                     │
└────────────────────────────────────┘
```

### Tokens CSS
- Fond : `var(--bg)` sur toute la page (pas de card)
- Conteneur centré via flexbox `align-items/justify-content: center`
- Cases PIN :
  - `width: 46px; height: 56px`
  - `font-size: 24px` (pas 44px qui cassait iOS)
  - `color: var(--accent)` + `font-weight: 700`
  - `border: 2px solid var(--separator)` → focus `var(--accent)` + halo `accent-light`
  - `text-align: center`
  - `autocomplete="off"` + `autocorrect="off"` + `autocapitalize="off"` par input
- Bouton : plein width de la zone (max 260 px desktop / 100% mobile)
- Messages d'erreur : `color: var(--error)` sous les cases PIN

### Règles de comportement
- **Aucun auto-submit** : la soumission ne se déclenche QUE sur :
  - Clic bouton « Se connecter »
  - Touche Entrée dans un input du form
- Saisie chiffre → auto-focus case suivante (ergonomie mobile) mais PAS de submit
- Paste d'un PIN complet dans la 1re case : répartit sur les 6 cases, focus la dernière — toujours pas de submit
- Le chiffre saisi s'affiche en clair (pas masqué)
- Si 401 → message d'erreur sous les cases + vider les cases + focus 1re case

---

## 📌 Surface 2 — Header `index.html` (Option B hybride)

### Principe
Header avec :
- **À gauche** : brand dot + nom "LocalTransfer"
- **Au milieu/droite** : nom du host (ex : "Mac de Serge") — visible uniquement ≥ 480 px
- **Tout à droite** : bouton « Se déconnecter »
  - **Mobile (< 480 px)** : icône `⎋` seule, tap-target 44×44
  - **Desktop (≥ 480 px)** : icône `⎋` + libellé « Se déconnecter » sur une ligne

### Rendu

```
Desktop (≥ 480 px) :
┌────────────────────────────────────────────────────────┐
│ ● LocalTransfer        Mac de Serge   ⎋ Se déconnecter │
└────────────────────────────────────────────────────────┘

Mobile (< 480 px) :
┌────────────────────────────────┐
│ ● LocalTransfer            ⎋   │
└────────────────────────────────┘
```

### Tokens CSS
- Hauteur header : 56 px (inchangé V1)
- Fond : `var(--surface)` + bordure basse `var(--separator)` 1 px
- Brand : inchangé
- Pill « Mac de Serge » : `accentLight` bg + `accent` color bold, radius `pill`
- Bouton logout :
  - Sur desktop : variant Secondary compact (32 px hauteur), `⎋` + texte
  - Sur mobile : icône seule dans un bouton 44×44 transparent (hover = `sidebar`), tooltip `title="Se déconnecter"` pour accessibilité
- Media query : `@media (max-width: 479px) { .logout-label { display: none } }`

### Comportement
- Clic → `POST /api/logout` → navigation `window.location = '/login'`
- Si échec réseau → afficher un toast discret mais rediriger quand même (session morte de toute façon)

---

## ☑️ Surface 3 — Checkbox dans `FileRow` desktop (Option A)

### Principe
Checkbox carrée à gauche de la row, style Linear/Notion. Cochée par défaut. Clic → toggle.

### Rendu

```
Cochée :
┌─────────────────────────────────────────────────────┐
│  ┌─┐                                                 │
│  │✓│   rapport.pdf                234 Ko         ✕  │
│  └─┘                                                 │
└─────────────────────────────────────────────────────┘

Décochée :
┌─────────────────────────────────────────────────────┐
│  ┌─┐                                                 │
│  │ │   photo.jpg                  1.2 Mo         ✕  │
│  └─┘                                                 │
└─────────────────────────────────────────────────────┘
```

### Spec visuelle
- Taille : 20×20 px
- Position : `left: Spacing::md` du bord gauche de la FileRow, vertical centré
- **Cochée** :
  - `RoundedRect` 20×20, fill `Colors::accent`, `Radius::sm` (6 px)
  - Glyph `✓` au centre, color blanc, `FontSize::small` (12 px) bold
- **Décochée** :
  - `RoundedRect` 20×20, fill `Colors::surface`, outline `Colors::separator` 1 px, radius `sm`
  - Vide à l'intérieur
- **Hover** (bonus si simple) : outline/fill un ton plus foncé
- **Clic hit-area** : toute la zone `20×20 + 4 px padding` pour meilleur tap sur pavé tactile

### Impact layout FileRow
- Le texte du nom de fichier commence désormais à `left + checkbox_width (20) + Spacing::md (12) + Spacing::md (12) = 44 px` au lieu de `Spacing::md` (12 px)
- Le reste (taille, bouton ✕) inchangé

### API widget
```cpp
class FileRow {
    // ...
    FileRow& setChecked(bool b);
    FileRow& onToggle(std::function<void(bool)> cb);
    // ...
};
```

---

## 🏷️ Surface 4 — Statuts « Proposé » / « Expiré » (Option A)

### Principe
Libellés texte intégrés dans la ligne 2 de la card de transfert, avec couleur distincte selon statut. Bouton « Annuler » inline pour les statuts « Proposé ».

### Rendu par statut

```
PROPOSÉ (en attente du visiteur, desktop→web) :
┌───────────────────────────────────────────────┐
│ → iPhone (Safari)                             │
│ En attente du visiteur…       [ Annuler ]    │  ← warning color
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░ (warning bar)   │
└───────────────────────────────────────────────┘

EN COURS (après clic visiteur) :
┌───────────────────────────────────────────────┐
│ → iPhone (Safari)                             │
│ 67 %  ·  15 Mo/s  ·  12s                      │  ← text-secondary
│ ████████████░░░░░░░░░░░░░░░░ (accent)         │
└───────────────────────────────────────────────┘

TERMINÉ (inchangé V1) :
┌───────────────────────────────────────────────┐
│ → iPhone (Safari)                             │
│ ✓ Terminé                                     │  ← success color
│ ████████████████████████████████ (success)    │
└───────────────────────────────────────────────┘

EXPIRÉ (15 min sans clic) :
┌───────────────────────────────────────────────┐
│ → iPhone (Safari)                             │
│ Expiré                                        │  ← error color
│ ▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬ (separator bar)  │
└───────────────────────────────────────────────┘

ANNULÉ (bouton Annuler cliqué) :
┌───────────────────────────────────────────────┐
│ → iPhone (Safari)                             │
│ Annulé                                        │  ← text-secondary
│ ▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬ (separator bar)  │
└───────────────────────────────────────────────┘
```

### Spec visuelle par statut

| Statut | Texte ligne 2 | Couleur texte | Couleur barre |
|---|---|---|---|
| `Proposed` | « En attente du visiteur… » + bouton `[ Annuler ]` | `Colors::warning` | `Colors::warning` (fond clair) |
| `InProgress` | « 67 %  ·  15 Mo/s  ·  12s » | `Colors::textSecondary` | `Colors::accent` |
| `Done` | « ✓ Terminé » | `Colors::success` | `Colors::success` |
| `Failed` | « ✗ Échec : <reason> » | `Colors::error` | `Colors::error` |
| `Rejected` | « ✗ Refusé » | `Colors::error` | `Colors::error` |
| `Cancelled` | « Annulé » | `Colors::textSecondary` | `Colors::separator` |
| `Expired` | « Expiré » | `Colors::error` | `Colors::separator` |
| `WaitingAcceptance` | « En attente… » (inchangé V1) | `Colors::textSecondary` | `Colors::accent` (indéterminée) |

### Bouton « Annuler » pour Proposed
- Style : `Button::Variant::Secondary` compact (28 px hauteur, 70 px largeur)
- Position : à droite sur la même ligne que le libellé statut
- Clic → `AppController::cancelPending(sessionId)` → passe le statut à `Cancelled`

### Couleurs à ajouter au Theme ? NON
- `Colors::warning` existe déjà dans Theme V1
- Les autres (`success`, `error`, `separator`, `accent`, `textSecondary`, `accentLight`) existent

### Impact sur le layout `drawTransferBar`
- Hauteur de la card inchangée (104 px)
- Bouton Annuler ajouté conditionnellement dans la zone texte ligne 2 pour les statuts Proposed
- Texte wrapping : pour les statuts avec bouton, le libellé peut être raccourci en "En attente…" (au lieu de "En attente du visiteur…")

---

## 📝 Composants à créer / modifier

### Côté SFML C++
- **Modifier** `FileRow` : ajout `setChecked(bool)` + `onToggle(cb)` + dessin checkbox + ajustement marge gauche
- **Modifier** `MainScreen::drawTransferBar` : support des nouveaux statuts + bouton Annuler pour Proposed
- **Aucun nouveau widget**

### Côté Web
- **Ajouter** `assets/web/login.html` (page plein écran)
- **Ajouter** `assets/web/login.js` (logique PIN sans auto-submit)
- **Modifier** `assets/web/index.html` (supprimer état auth, ajouter header avec logout)
- **Modifier** `assets/web/app.js` (suppression logique auth, ajout logout, fetch+blob download, bouton pick files)
- **Modifier** `assets/web/style.css` (styles login, header, checkbox web-side si besoin, tailles PIN réduites)

---

## 📐 Contraintes visuelles (rappel)

- Tokens `Colors::*`, `Spacing::*`, `Radius::*`, `FontSize::*` de V1 (aucun ajout)
- CSS vars côté web alignées sur Theme SFML (déjà en place V1)
- Mobile-first : viewport ≥ 320 px testable sur iPhone SE
- Icônes Unicode uniquement (`⎋`, `✓`, `✕`, `→`, `←`, `•`)
- Pas d'animations lourdes (hover + transition 100 ms max)
- Pas de JS framework — vanilla uniquement
- Accessibilité : contraste AA, focus visibles, aria-labels, tap-targets ≥ 44 px sur mobile

---

## ✅ Prochaine étape

→ Retour orchestrateur → Étape 4 (Plan d'action) → Étape 5 (Dev agent).
