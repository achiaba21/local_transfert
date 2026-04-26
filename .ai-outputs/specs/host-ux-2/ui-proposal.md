# UI/UX — Sprint UX-2 mockups

**Date :** 2026-04-24

---

## 1. Zone TRANSFERTS complète (avec débordement)

### Layout global (avant / après)

```
AVANT (104 px fixe, cards 320×~58, tronque silencieusement)
┌────────────────────────────────────────────────────────────────────────────┐
│ TRANSFERTS · 6                                                             │
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ [?]                         │
│ │ → Mac       │ │ ← iPhone    │ │ → Win       │  ← 3 cards visibles, les   │
│ │ 67% · 15M/s │ │ 45% · 8M/s  │ │ 89% · 20M/s │     3 autres disparaissent │
│ │ ════░░░░░░  │ │ ══░░░░░░░░  │ │ ═════░░     │                             │
│ └─────────────┘ └─────────────┘ └─────────────┘                             │
└────────────────────────────────────────────────────────────────────────────┘

APRÈS (104 px, cards 340×64, scroll horizontal + flèches L/R)
┌────────────────────────────────────────────────────────────────────────────┐
│ TRANSFERTS · 6                                               [◀] [▶]  2/6 │
│ ┌───────────────┐ ┌───────────────┐ ┌───────────────┐ ┊                    │
│ │ ↑ MacBook     │ │ ↓ iPhone   [O]│ │ ↑ Win PC  [X] │ ┊ ← fade gradient   │
│ │ 85% 15 Mo/s…  │ │ ✓ Terminé     │ │ 45% 8 Mo/s… │ ┊   bord droit       │
│ │ ██████████░░  │ │ ██████████    │ │ █████░░░░░  │ ┊                    │
│ └───────────────┘ └───────────────┘ └───────────────┘ ┊                    │
└────────────────────────────────────────────────────────────────────────────┘
```

- **Flèches L/R** : 22×22, icône vectorielle (réutiliser arrow-up/down
  tournée OU nouveau arrow-left/right à ajouter). Actives quand scroll
  possible dans la direction. Clic = scroll de 1 card (340 + gap).
- **Compteur position** : `"2/6"` discret à droite des flèches
  (textSecondary, FontSize::small). Optionnel V1.
- **Fade gradient** : sur les 12 px de droite quand cards cachées
  existent. Optionnel, simple bande dégradée `surface → transparent`.
- **Molette horizontale** + **shift+molette verticale** dans la zone
  bottomRect_ ajuste `transfersScrollX_`.

---

## 2. Card InProgress (340 × 64)

```
┌────────────────────────────────────────────────────────┐
│ ↑  MacBook Pro                          [  Annuler  ]  │  ← ligne titre (30 px)
│    85 %   15 Mo/s · 12s                                │  ← ligne status (20 px)
│ ██████████████████████████████████████░░░░             │  ← progress 6 px
└────────────────────────────────────────────────────────┘
  ↑ icône 18×18 accent (envoi) ou success (réception)
```

Détails :
- Flèche 18×18 à `Spacing::md` du bord gauche
- Nom peer à côté, FontSize::body bold
- **Bouton Annuler** 90×22 à top-right : fond `accentLight`, texte
  `accent`, label "Annuler" FontSize::overline bold. Espacement `Spacing::md`
  du bord droit. Hover = fond `separator`.
- **Ligne 2 hiérarchisée** : `"85 %"` en `FontSize::h2` bold `text`, puis
  `"15 Mo/s · 12s"` en `FontSize::small` `textSecondary` côte à côte.
- **Barre de progression 6 px** (au lieu de 4) collée en bas de la card.

---

## 3. Card Done + Incoming (côté récepteur)

```
┌────────────────────────────────────────────────────────┐
│ ↓  iPhone (Safari)              [ Ouvrir le dossier ]  │
│    ✓ Terminé                                           │
│ ████████████████████████████████████████████████       │
└────────────────────────────────────────────────────────┘
```

- **Bouton « Ouvrir le dossier »** 140×22 à top-right : variant `Primary`
  (fond `accent`, texte blanc). Hover légèrement plus foncé.
- **"✓ Terminé"** en `FontSize::body` color `success`, pas de speed/eta.
- Progress 100% pleine, color `success`.
- Card visible pendant 10 s puis auto-retirée.

---

## 4. Card Done + Outgoing (côté envoyeur)

```
┌────────────────────────────────────────────────────────┐
│ ↑  MacBook Pro                                         │
│    ✓ Terminé                                           │
│ ████████████████████████████████████████████████       │
└────────────────────────────────────────────────────────┘
```

- Pas de bouton (l'envoyeur n'a rien à « ouvrir » — le fichier part).
- Card auto-retirée après 10 s.

---

## 5. Card Failed / Cancelled / Expired

```
┌────────────────────────────────────────────────────────┐
│ ↑  MacBook Pro                                         │
│    ✗ Annulé                                            │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░             │
└────────────────────────────────────────────────────────┘
```

- Ligne 2 en color `error` pour Failed/Cancelled, `warning` pour Expired.
- Progress vide ou reste figée à la valeur du cancel.
- Failed/Cancelled auto-retirés après 30 s. Expired reste (à l'utilisateur
  de comprendre que son envoi web n'a pas été récupéré).

---

## 6. Card Proposed (existante — rappel)

```
┌────────────────────────────────────────────────────────┐
│ ↑  iPhone (Safari)                  [   Annuler   ]    │
│    En attente du visiteur…                             │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░             │
└────────────────────────────────────────────────────────┘
```

- Inchangé par rapport à V1.1. Le bouton Annuler route vers
  `cancelPending` (existant), pas vers `cancelSession`.
- Distinction : `cancelPending` pour Proposed, `cancelSession` pour
  Accepted/InProgress. Deux codes distincts, correct.

---

## 7. Interactions résumées

| Card status | Bouton top-right | Action |
|-------------|------------------|--------|
| Proposed | « Annuler » | `controller_.cancelPending(sid)` |
| Accepted / InProgress | « Annuler » | `controller_.cancelSession(sid)` |
| Done + Incoming | « Ouvrir le dossier » | `controller_.openDownloadDir()` |
| Done + Outgoing | — | — |
| Failed / Cancelled / Expired | — | — |

---

## 8. Icônes additionnelles requises

Deux icônes supplémentaires (à ajouter dans `scripts/generate_icons.py`
et embed CMake) :

- `arrow-left.png` 16×16 — pour flèche scroll L
- `arrow-right.png` 16×16 — pour flèche scroll R

(Facultatif : on peut réutiliser `arrow-up` / `arrow-down` avec
`sf::Sprite::setRotation(90)` / `setRotation(-90)`, **préférable** pour
éviter de grossir la banque d'icônes.)

**Décision :** rotation via `sf::Sprite::setRotation` pour garder la
banque minimale.

---

## Validation design

Les choix haut-niveau sont déjà validés (scroll H, annuler InProgress,
ouvrir dossier côté récepteur, auto-clean 10/30s). Les mockups
ci-dessus concrétisent sans ambiguïté. **Pas de nouvelle validation
requise — on passe au Dev.**
