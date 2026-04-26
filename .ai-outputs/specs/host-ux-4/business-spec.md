# Spec métier — Sprint UX-4 SharePanel collapsible

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur

---

## 1. Contexte

Le SharePanel occupe 240 px à droite en permanence (22% de la largeur
fenêtre). Pour un utilisateur qui n'utilise pas le partage web, c'est
de l'espace gaspillé. Quand il est utilisé, le QR (160 px) et le PIN
(28 px) sont trop petits pour être lus de loin (scan téléphone à 1 m).

Par ailleurs le bouton « Copier » actuel ne copie que l'URL, pas le PIN.

---

## 2. Objectif

Rendre le SharePanel **pliable** pour libérer l'espace, **grossir** le
QR et le PIN quand il est déplié, et **ajouter** un bouton pour copier
le PIN.

---

## 3. Décisions produit validées

### Q1 — État initial (A)
**Déplié par défaut** — cohérent avec l'usage actuel, découverte facile
du feature. L'utilisateur plie manuellement quand il veut récupérer la
place.

### Q2 — Persistance (A)
**Sauvegardé dans `infra::Config`**. À la réouverture de l'app, le
panneau est dans le même état (déplié ou plié). Champ booléen
`sharePanelCollapsed` ajouté au JSON de config.

### Q3 — Visuel collapsed (A)
**Rail vertical ~40 px** à droite contenant :
- Icône QR (vectorielle, 20×20, orientée verticalement via rotation ou
  dédié) en haut du rail
- Badge numérique si ≥ 1 visiteur web connecté : pill accent avec le
  count (`1`, `2`, …)
- Clic n'importe où sur le rail → déplie

### Q4 — Boutons Copier (B)
**Deux boutons séparés**, empilés verticalement sous l'URL :
- « Copier URL »
- « Copier PIN »
Chacun avec feedback transitoire « Copié ! » 2 s (comme aujourd'hui
pour URL).

### Q5 — Emplacement toggle (B)
**Intégré dans le SharePanel lui-même** :
- Mode déplié : petite croix/flèche dans le **coin haut-droit** de la
  overline `PARTAGE WEB` → clic = plier
- Mode collapsed : le rail entier est cliquable → clic = déplier

Pas de bouton dans le header (header reste épuré).

---

## 4. Livrables checklist

- [ ] Toggle dans le SharePanel (croix haut-droit déplié, rail entier
      cliquable collapsed)
- [ ] État collapsed = rail 40 px avec icône QR + badge visiteurs
- [ ] QR agrandi à **220 px** quand déplié
- [ ] PIN agrandi à **44 px** avec kerning légèrement étendu
- [ ] Bouton « Copier URL » + bouton « Copier PIN » séparés (32 px each)
- [ ] Overline `CODE D'ACCÈS` avec accent correct (remplace `CODE D'ACCES`)
- [ ] Largeur SharePanel dynamique côté MainScreen : `kSharePanelW`
      devient `sharePanelWidth()` qui renvoie 240 ou 40 selon l'état
- [ ] Champ `bool sharePanelCollapsed` dans `infra::Config` +
      load/save JSON + param ctor AppController
- [ ] API controller : `toggleSharePanel()` qui flip + persiste
- [ ] PROGRESS.md UX-4 ✅ + journal daté
- [ ] Build Release propre, 8/8 tests passent

---

## 5. Critères d'acceptation

- Lancement : panneau déplié (par défaut config), visible avec QR 220 +
  PIN 44 + 2 boutons Copier.
- Clic sur croix coin haut-droit → panneau se replie à 40 px. Zone
  centrale gagne 200 px. Config écrite → fermer + rouvrir l'app = reste
  replié.
- Clic sur rail collapsed → déplié, retrouve QR 220 + PIN 44.
- Si un visiteur se connecte quand collapsed → badge `1` apparaît sur
  le rail.
- Clic Copier PIN → le PIN est dans le presse-papier formaté
  `472931` (sans espaces) + feedback « Copié ! » 2 s.
- 8 tests existants passent, 0 régression TCP LTR1 / web / UX-1..3.

---

## 6. Contraintes

- C++17 strict
- Pas de nouvelle dépendance
- Tokens Theme (Colors, Spacing, Radius, FontSize)
- RoundedRect pour nouveaux éléments
- Backward compat : `SharePanel::setBounds` API inchangée, nouveaux
  setters additifs (`setCollapsed(bool)`, `setVisitorCount(int)`)
- Config JSON : nouveau champ optionnel `sharePanelCollapsed` avec
  default `false` (déplié) si absent — pas de breaking change de config
