# Spec métier — Sprint UX-1 Hygiène visuelle host

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur
**Scope :** app desktop SFML, aucune nouvelle fonctionnalité utilisateur

---

## 1. Contexte

La V1 de l'app desktop affiche des workarounds visuels posés rapidement
pendant les sprints précédents :
- Glyphe `v` pour la coche (la police système n'a pas toujours `✓`)
- Préfixe texte `[D]` pour les dossiers
- Texte `x` pour la croix de suppression
- Empty state sidebar avec glyphe `~` + texte fixe
- 2 boutons `Fichiers...` + `Dossier...` qui se confondent visuellement
- Texte obsolète "Cliquez sur « Parcourir »" (bouton renommé)
- Transferts sans icône directionnelle claire (juste `→` / `←` en texte)

Ce sprint remet l'app au niveau d'hygiène visuelle attendu avant
d'attaquer les features lourdes des sprints suivants (drag&drop, dark
mode, raccourcis clavier, notifications OS).

---

## 2. Objectif

8 livrables visuels, zéro nouveau flux utilisateur, zéro régression.

---

## 3. Décisions produit validées

### Q1 — Bouton « Ajouter... » (A)
Remplacer les 2 boutons par **un seul** bouton `Ajouter...` qui ouvre un
petit menu popup sous lui avec 2 options :
- « Fichiers... » → ouvre le file picker multi
- « Dossier... » → ouvre le folder picker

### Q2 — Pill « Local » (A)
Afficher un pill `Local` (accent-light + accent text, même style que
`Web`) sur les `DeviceListItem` dont `kind == PeerKind::Lan` (les peers
découverts via TCP). Symétrie visuelle avec le pill `Web` existant.

### Q3 — Empty state sidebar 2 états (A)
- **Pendant les 5 premières secondes** après lancement OU après un
  rescan manuel : picto radar pulsant + texte « Recherche... »
- **Après 5 s sans pair détecté** : picto statique gris (3 traits /
  globe barré) + texte « Aucun appareil détecté »

### Q4 — Icônes via PNG rasterisés + sf::Sprite (B)
Approche retenue : **rasteriser les SVG en PNG** (32×32 et 48×48 pour
l'empty state) une fois, commiter les PNG dans `assets/icons/`, les
embarquer via `ltr_embed_file` comme pour les SVG web, et les charger
à froid dans une `IconLibrary` singleton.

Raison : rendu visuel plus fin qu'avec des primitives SFML, zéro coût
runtime vs VertexArray (juste un `sf::Sprite::draw`).

Icônes à produire :
- `check.png` 24×24 (coche blanche fond transparent)
- `folder.png` 20×20 (dossier neutre)
- `close.png` 16×16 (croix fine)
- `arrow-up.png` 18×18 (flèche montante)
- `arrow-down.png` 18×18 (flèche descendante)
- `radar.png` 48×48 (pour empty state pulsant)
- `no-device.png` 48×48 (pour empty state stable)

### Q5 — Couleurs flèches transfert (A)
- Envoi `→` : `↑` teinté **accent** (indigo `#6366F1`)
- Réception `←` : `↓` teinté **success** (vert `#10B981`)

Cohérent avec la sémantique « j'envoie vers l'extérieur » (action
principale, indigo) vs « je reçois » (résultat positif, vert).

---

## 4. Critères d'acceptation

- [ ] Checkbox FileRow : coche dessinée depuis `check.png` (blanche sur
  fond accent quand cochée)
- [ ] Dossier FileRow : icône `folder.png` à la place du texte `[D]`
- [ ] Croix suppression FileRow : icône `close.png` à la place de `x`
- [ ] Card TRANSFERTS : icône flèche ↑ accent OU ↓ success en tête, avant
  le nom du peer
- [ ] Bouton zone centrale : un seul bouton `Ajouter ▾` qui dropdown 2
  choix (Fichiers / Dossier)
- [ ] Pill `Local` visible sur les DeviceListItem natifs TCP (à côté du
  pill `Web` existant dans les devices web)
- [ ] Empty state sidebar : 2 états distincts, radar pulsant pendant les
  5 s initiales, statique ensuite
- [ ] Texte `Cliquez sur « Parcourir »` remplacé par un texte cohérent
  avec le nouveau bouton (`Cliquez sur « Ajouter »`)
- [ ] Build Release propre, 8 tests passent
- [ ] `.ai-outputs/specs/host-ui-improvements/PROGRESS.md` mis à jour
  (UX-1 ✅, entrée journal datée)

---

## 5. Contraintes

- C++17 strict
- Aucune nouvelle dépendance (miniz + SFML suffisent pour PNG)
- Les PNG sont générés une fois (externalisation du script de génération
  documentée dans `docs-agents/UI_GUIDELINES.md`)
- Tokens Colors / Spacing / Radius respectés
- Backward compat : l'API publique des widgets existants (`FileRow::setChecked`,
  etc.) reste identique
- UI_REQUIRED: true (propositions mockups A/B attendues pour le menu
  Ajouter et la pill Local, même si l'utilisateur a déjà choisi l'approche
  générale)
