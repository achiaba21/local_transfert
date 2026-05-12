# Propositions UI/UX — Phase 2 : Portail Client Externe

> 2 surfaces visuelles : (A) page web déposant, (B) écran desktop SFML.
> Format : 3 options par surface, recommandations argumentées, parcours
> mobile-first.

## ✅ DÉCISION FINALE (validée le 2026-05-12)

- **Surface A — Page web déposant : A3** (Single-page progressif).
  Zone fichiers verrouillée tant que nom + consent non valides.
- **Surface B — Écran desktop SFML : B1** (Liste verticale pleine
  largeur, cards `DepositLinkCard` cohérentes avec `device_list_item`).

Les options A1/A2/B2/B3 restent en annexe ci-dessous pour traçabilité.

---

## A) Page Web Déposant

### Contraintes communes aux 3 options

- **Étanchéité totale du dashboard host.** Aucun header `LocalTransfer`,
  aucun bouton « Se connecter », aucune mention `PIN`, `peers`, `P2P`.
- **Réutilisation discrète du design system CSS** : on garde
  `--bg`, `--surface`, `--accent`, `--separator`, `--r-md`, espacements
  `--sp-*` pour cohérence VISUELLE, mais on REDÉFINIT une classe `body.page-deposit`
  qui n'inclut JAMAIS la classe `page-main`.
- **Footer ultra-discret** : « Sécurisé. Pas de cloud, pas de compte. »
  En `--text-secondary`, `--fs-small`, centré, sans logo.
- **Mobile-first** : tout le layout est en colonne unique, max-width
  520 px sur desktop, full-width avec padding `--sp-lg` sur mobile.
- **Pas de meta description revealing the product name** : `<title>` =
  juste le libellé du dépôt + nom du host.
- **3 états techniques** (lien actif / expiré / confirmation) =
  3 fichiers HTML distincts servis par `deposit_routes`, chacun avec
  son propre flux d'inclusion JS (pas de SPA).

---

### Option A1 — Single-page linéaire (scrollable)

**Description** : un seul écran qui scroll, tout est visible d'emblée :
header, infos déposant, fichiers, bouton envoyer. Comme WeTransfer
« send file ».

**Wireframe (mobile-first) :**

```
┌────────────────────────────────────────┐
│ Envoi pour                             │  ← bandeau --accent-light
│ Pièces Dupont 2026                     │  ← --fs-h1 bold
│ Destinataire : Cabinet Martin          │  ← --fs-small --text-secondary
├────────────────────────────────────────┤
│                                        │
│  Vos infos                             │  ← overline
│  ┌──────────────────────────────────┐  │
│  │ Nom *                            │  │  ← text-input
│  │ ┌────────────────────────────┐   │  │
│  │ │ Marie Dupont               │   │  │
│  │ └────────────────────────────┘   │  │
│  │                                  │  │
│  │ ☐ J'accepte que mes fichiers     │  │  ← checkbox + label
│  │   soient transmis à Cabinet      │  │
│  │   Martin.                        │  │
│  └──────────────────────────────────┘  │
│                                        │
│  Vos fichiers                          │  ← overline
│  ┌──────────────────────────────────┐  │
│  │ ↓                                │  │  ← drop-zone
│  │ Glissez vos fichiers ici         │  │  ← icône upload
│  │ ou cliquez pour parcourir        │  │
│  │                                  │  │
│  │ Limite : 50 fichiers, 2 Go max   │  │  ← --fs-small
│  └──────────────────────────────────┘  │
│                                        │
│  ▸ devis.pdf      120 Ko          ×    │  ← file-row (réutilise patron)
│  ▸ photo.jpg      2.4 Mo          ×    │
│                                        │
│  ────────────────────────────────────  │
│  Total : 2.5 Mo sur 2 Go               │  ← --text-secondary
│                                        │
│  ┌──────────────────────────────────┐  │
│  │         Envoyer                  │  │  ← btn-primary, --r-md
│  └──────────────────────────────────┘  │  ← disabled si form invalide
│                                        │
├────────────────────────────────────────┤
│   Sécurisé. Pas de cloud, pas de       │  ← footer --fs-small
│   compte.                              │
└────────────────────────────────────────┘
```

**Avantages :**
- Tout visible : pas de surprise pour le déposant.
- Pas de gestion d'étapes côté JS → simple.
- Drop-zone immédiatement visible → familier (WeTransfer-like).

**Inconvénients :**
- Sur mobile, beaucoup de scroll si plusieurs fichiers + consent long.
- Le déposant peut ajouter des fichiers AVANT de remplir nom/consent →
  désynchronisation visuelle, faut bien désactiver le bouton envoyer.

---

### Option A2 — Wizard 2 étapes

**Description** : on découpe en 2 écrans successifs.
1. Étape 1 = identité + consentement.
2. Étape 2 = fichiers + envoi.
Une barre de progression discrète en haut indique l'étape.

**Wireframe étape 1 :**

```
┌────────────────────────────────────────┐
│ Envoi pour                             │
│ Pièces Dupont 2026                     │
│ Destinataire : Cabinet Martin          │
├────────────────────────────────────────┤
│  ●━━━━━━━━━━━ ○                        │  ← stepper 1/2
│  Étape 1 — Vos infos                   │
│                                        │
│  Nom *                                 │
│  ┌─────────────────────────────────┐   │
│  │                                 │   │
│  └─────────────────────────────────┘   │
│                                        │
│  ☐ J'accepte que mes fichiers          │
│    soient transmis à Cabinet Martin.   │
│                                        │
│  ┌─────────────────────────────────┐   │
│  │         Continuer    →          │   │  ← disabled si vide
│  └─────────────────────────────────┘   │
└────────────────────────────────────────┘
```

**Wireframe étape 2 :**

```
┌────────────────────────────────────────┐
│ Envoi pour Pièces Dupont 2026          │
│ Bonjour Marie Dupont                   │  ← --text-secondary
├────────────────────────────────────────┤
│  ●━━━━━━━━━━━ ●                        │  ← stepper 2/2
│  Étape 2 — Vos fichiers                │
│                                        │
│  ┌──────────────────────────────────┐  │
│  │   ↓ Glissez ou cliquez           │  │  ← drop-zone
│  └──────────────────────────────────┘  │
│                                        │
│  ▸ devis.pdf      120 Ko          ×    │
│  Total : 120 Ko sur 2 Go               │
│                                        │
│  [ ← Retour ]   [    Envoyer    ]      │
└────────────────────────────────────────┘
```

**Avantages :**
- Force l'ordre : on ne montre la drop-zone que si nom + consent sont OK.
- Réduit la charge cognitive sur mobile (1 chose à la fois).
- Identité déjà capturée → le déposant peut être « accueilli » par son
  prénom à l'étape 2 (« Bonjour Marie »).

**Inconvénients :**
- Plus de friction perçue (2 clics au lieu d'1).
- Si le déposant veut re-vérifier son nom → doit revenir en arrière.

---

### Option A3 — Single-page progressif (recommandé)

**Description** : single-page comme A1, MAIS la zone fichiers est
visuellement « verrouillée » (greyée + lock icon) tant que nom +
consent ne sont pas valides. Quand les 2 conditions sont remplies,
la zone fichiers s'anime (fade + scale léger) et devient interactive.
Hybride entre A1 (tout visible) et A2 (parcours guidé).

**Wireframe :**

```
┌────────────────────────────────────────┐
│ Envoi pour                             │
│ Pièces Dupont 2026                     │
│ Destinataire : Cabinet Martin          │
├────────────────────────────────────────┤
│                                        │
│  Vos infos                             │
│  ┌──────────────────────────────────┐  │
│  │ Nom *                            │  │
│  │ ┌─────────────────────────────┐  │  │
│  │ │ Marie Dupont           ✓    │  │  │  ← checkmark live validation
│  │ └─────────────────────────────┘  │  │
│  │ ☑ J'accepte que mes fichiers     │  │
│  │   soient transmis à Cabinet      │  │
│  │   Martin.                        │  │
│  └──────────────────────────────────┘  │
│                                        │
│  Vos fichiers                          │
│  ┌──────────────────────────────────┐  │
│  │  ↓ Glissez vos fichiers ici      │  │  ← actif (car form valide)
│  │  ou cliquez pour parcourir       │  │
│  │  50 fichiers, 2 Go max           │  │
│  └──────────────────────────────────┘  │
│                                        │
│  ▸ devis.pdf  120 Ko             ×    │
│  Total : 120 Ko sur 2 Go               │
│                                        │
│  ┌──────────────────────────────────┐  │
│  │         Envoyer                  │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
```

État verrouillé (nom vide / consent non coché) :

```
│  Vos fichiers                          │
│  ┌──────────────────────────────────┐  │
│  │  🔒                              │  │  ← icône lock
│  │  Remplissez vos infos d'abord    │  │  ← --text-secondary
│  └──────────────────────────────────┘  │  ← opacity .5, pointer-events:none
```

**Avantages :**
- Tout est visible (transparence + confiance).
- Parcours guidé sans étapes formelles.
- Animation subtile = renforce la sensation de progression.
- 1 seul écran HTML → JS minimal, pas de gestion de history.

**Inconvénients :**
- Animation à coder proprement (CSS transition + JS).
- Sur très petit mobile, la zone verrouillée peut frustrer.

---

### États « lien expiré/révoqué » (commun aux 3 options)

**Wireframe :**

```
┌────────────────────────────────────────┐
│                                        │
│           ⏱                            │  ← icône grosse
│                                        │
│   Ce lien n'est plus actif.            │  ← --fs-h1 bold centré
│                                        │
│   Contactez votre interlocuteur        │  ← --text-secondary centré
│   pour obtenir un nouveau lien.        │
│                                        │
│                                        │
│   Sécurisé. Pas de cloud, pas de       │
│   compte.                              │
└────────────────────────────────────────┘
```

Centré verticalement, sobre. Rien de plus.

---

### État « confirmation post-envoi » (commun aux 3 options)

**Wireframe :**

```
┌────────────────────────────────────────┐
│                                        │
│            ✓                           │  ← gros check vert --success
│                                        │
│   Dépôt enregistré                     │  ← --fs-h1 bold
│                                        │
│   Identifiant : a3f9c2b1               │  ← monospace --fs-body
│   ┌─────────────────────────────────┐  │
│   │ devis.pdf            120 Ko     │  │
│   │ photo.jpg            2.4 Mo     │  │
│   │ rapport.docx         48 Ko      │  │
│   └─────────────────────────────────┘  │
│   3 fichiers, 2.5 Mo                   │
│                                        │
│   ┌─────────────────────────────────┐  │
│   │   ⤓  Télécharger le reçu        │  │  ← btn-secondary
│   └─────────────────────────────────┘  │
│                                        │
│   Merci. Cabinet Martin a été          │  ← --text-secondary
│   prévenu de votre envoi.              │
│                                        │
└────────────────────────────────────────┘
```

Le bouton « Télécharger le reçu » déclenche le téléchargement du JSON
signé via `GET /api/deposit/receipt/:id`.

---

### Parcours d'erreur (commun aux 3 options)

- **Réseau coupé en cours d'upload** : toast en haut « Connexion
  perdue. Réessayez. » + bouton « Réessayer » sur la zone fichiers.
  Pas de reprise auto Phase 2.
- **Dépôt refusé (quota host / taille / nb fichiers)** : la barre
  d'upload se transforme en bande rouge + message non technique
  contextuel :
  - « Ce dépôt dépasse la taille autorisée (2.1 Go sur 2 Go). »
  - « Trop de fichiers (51 sur 50). »
  - « Ce dépôt ne peut pas être accepté pour le moment. » (= quota host)

---

### Recommandation surface A

**👉 Option A3 (Single-page progressif)** — recommandée.

Justification :
- Combine la simplicité d'A1 (1 page, JS minimal) avec la guidance
  d'A2 (parcours ordonné).
- Le verrouillage visuel évite le pitfall de A1 où le déposant ajoute
  des fichiers avant de réaliser qu'il faut remplir le formulaire.
- Pas de stepper formel → moins d'« officialisé », plus chaleureux.
- 1 seul fichier HTML, 1 seul template, code JS rectiligne.

---

## B) Écran Desktop SFML — DepositLinksScreen

### Contraintes communes aux 3 options

- Utilise **uniquement** les tokens du thème (`Colors`, `Spacing`,
  `Radius`, `FontSize`).
- Cards = `RoundedRect` avec ombre légère (cf. `Card` widget existant).
- Layout cohérent avec `main_screen` (header fixe, main scroll).
- Toutes les chaînes via `ltr::ui::utf8()`.
- Réutilise `qr_code_view` pour les modals QR.
- Réutilise `text_input`, `button`, `dropdown_menu` existants.
- État vide + état Personal Free traités identiquement dans les 3
  options (cf. en bas).

---

### Option B1 — Liste verticale pleine largeur (1 colonne)

**Description** : chaque lien occupe une card large, pleine largeur du
container central. Cohérent avec le pattern `device_list_item` /
`file_row` existant. Scroll vertical naturel.

**Wireframe :**

```
┌──────────────────────────────────────────────────────────────┐
│ ← Retour   Liens de dépôt                  [+ Nouveau lien]  │  ← header
├──────────────────────────────────────────────────────────────┤
│ [ Tous ] [ Actifs ] [ Expirés ]                              │  ← filter pills
│                                                              │
│ ┌──────────────────────────────────────────────────────────┐ │
│ │ Pièces Dupont 2026          🟢 Actif · 6 j restants      │ │  ← card
│ │ 3 dépôts reçus · 84 Mo total                             │ │
│ │ https://192.168.1.42:45457/deposit/AbCd...               │ │
│ │ [ Copier l'URL ]   [ QR ]   [ ⋮ Révoquer ]               │ │
│ └──────────────────────────────────────────────────────────┘ │
│                                                              │
│ ┌──────────────────────────────────────────────────────────┐ │
│ │ Devis client Martin         🟠 Expire dans 2 h           │ │
│ │ 0 dépôts reçus                                           │ │
│ │ https://192.168.1.42:45457/deposit/XyZw...               │ │
│ │ [ Copier l'URL ]   [ QR ]   [ ⋮ Révoquer ]               │ │
│ └──────────────────────────────────────────────────────────┘ │
│                                                              │
│ ┌──────────────────────────────────────────────────────────┐ │
│ │ Audit interne 2025           ⚫ Révoqué                  │ │  ← dim
│ │ 12 dépôts reçus · 1.2 Go total                           │ │
│ │ — révoqué le 10 mai 2026 —                               │ │
│ └──────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

**Avantages :**
- Pattern déjà utilisé dans le projet (cohérence visuelle immédiate).
- URL visible directement → cliquer-copier rapide.
- Beaucoup d'informations sur une seule card.

**Inconvénients :**
- Sur petite fenêtre desktop, peut sembler verbeux.
- Si beaucoup de liens (>10), scroll long.

---

### Option B2 — Grille 2 colonnes de cards compactes

**Description** : cards plus compactes (libellé + badge + boutons
icônes seuls), 2 colonnes sur largeur standard, 3 sur grand écran.

**Wireframe :**

```
┌──────────────────────────────────────────────────────────────┐
│ ← Retour   Liens de dépôt                  [+ Nouveau lien]  │
├──────────────────────────────────────────────────────────────┤
│ [ Tous ] [ Actifs ] [ Expirés ]                              │
│                                                              │
│ ┌────────────────────────┐  ┌────────────────────────┐       │
│ │ Pièces Dupont 2026     │  │ Devis client Martin    │       │
│ │ 🟢 6 j  · 3 dépôts     │  │ 🟠 2 h  · 0 dépôt      │       │
│ │ [📋]  [QR]  [⋮]        │  │ [📋]  [QR]  [⋮]        │       │
│ └────────────────────────┘  └────────────────────────┘       │
│                                                              │
│ ┌────────────────────────┐  ┌────────────────────────┐       │
│ │ Audit interne 2025     │  │ + Nouveau lien         │       │
│ │ ⚫ révoqué · 12 dépôts │  │                        │       │
│ │                        │  │ (gros bouton call-to-  │       │
│ │                        │  │  action en dernière    │       │
│ │                        │  │  card)                 │       │
│ └────────────────────────┘  └────────────────────────┘       │
└──────────────────────────────────────────────────────────────┘
```

**Avantages :**
- Très dense, on voit beaucoup de liens d'un coup d'œil.
- Card « + Nouveau lien » en grille = call-to-action naturel.
- Moderne (style « tableau de bord »).

**Inconvénients :**
- URL non visible → faut ouvrir un détail/modal pour copier.
- Plus de complexité de layout (responsive grid SFML manuel).
- Moins cohérent avec le reste du projet (pas d'autre grille).

---

### Option B3 — Master-Detail (liste à gauche + détail à droite)

**Description** : split-pane. Liste compacte à gauche (33 % largeur),
panneau de détail à droite (67 %). En sélectionnant un lien on voit
ses stats, son QR, ses dépôts récents, sans modal.

**Wireframe :**

```
┌──────────────────────────────────────────────────────────────┐
│ ← Retour   Liens de dépôt                  [+ Nouveau lien]  │
├──────────────────────────────────────────────────────────────┤
│  Pièces Dupont 2026 🟢│  Pièces Dupont 2026                  │
│  3 dépôts · 6 j       │  Actif · expire le 18 mai 2026       │
│ ──────────────────────│                                      │
│  Devis client Martin🟠│  ┌──────────────────────────────┐    │
│  0 dépôt · 2 h        │  │ ████████ QR ████████          │    │
│ ──────────────────────│  │ ████              ████        │    │
│  Audit 2025      ⚫   │  │ ████   QR CODE    ████        │    │
│  12 dépôts · révoqué  │  └──────────────────────────────┘    │
│                       │  https://192.168.1.42:45457/...      │
│                       │  [ Copier l'URL ]                    │
│                       │                                      │
│                       │  Limites : 50 fichiers, 2 Go         │
│                       │  Dépôts reçus : 3 (84 Mo)            │
│                       │                                      │
│                       │  Derniers dépôts                     │
│                       │  ▸ Marie Dupont · 11 mai · 2 fichiers│
│                       │  ▸ Marie Dupont · 9 mai · 1 fichier  │
│                       │  ▸ Marie Dupont · 3 mai · 5 fichiers │
│                       │                                      │
│                       │  [ Révoquer ce lien ]                │
└──────────────────────────────────────────────────────────────┘
```

**Avantages :**
- Très riche : QR + détails + activité récente sans clic supplémentaire.
- Adapté à un usage « gestion régulière » (le host vérifie qui a
  déposé quoi).
- Moins de modals → moins de friction.

**Inconvénients :**
- Plus complexe à implémenter (sélection persistante, layout split).
- Si peu de liens (1-2), beaucoup d'espace gaspillé.
- Plus loin du pattern existant (`main_screen` n'est pas split).

---

### États communs B1/B2/B3

**État vide :**

```
┌──────────────────────────────────────────────────────────────┐
│ ← Retour   Liens de dépôt                                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                                                              │
│                       📥                                     │  ← icône
│                                                              │
│              Aucun lien de dépôt                             │  ← --fs-h1
│         pour le moment                                       │
│                                                              │
│       Créez un lien pour recevoir des fichiers               │  ← --text-secondary
│       d'un client externe.                                   │
│                                                              │
│       ┌─────────────────────────────────┐                    │
│       │   + Créer le premier lien       │                    │  ← btn-primary
│       └─────────────────────────────────┘                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**État Personal Free :**

```
┌──────────────────────────────────────────────────────────────┐
│ ← Retour   Liens de dépôt                                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                       🔒                                     │
│                                                              │
│      Disponible avec un plan Business                        │  ← --fs-h1
│                                                              │
│      Permettez à vos clients de déposer des fichiers         │
│      via un simple lien, sans installer LocalTransfer.       │
│                                                              │
│      ┌─────────────────────────────────┐                     │
│      │   En savoir plus                │                     │  ← btn-secondary, --accent
│      └─────────────────────────────────┘                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

### Modals (communs B1/B2/B3, sauf B3 où ils sont remplacés par le pane droit)

**Modal de création :**

```
┌─────────────────────────────────────────────┐
│ Nouveau lien de dépôt                  ×    │
├─────────────────────────────────────────────┤
│ Libellé                                     │
│ ┌─────────────────────────────────────────┐ │
│ │ Pièces Dupont 2026                      │ │  ← text_input
│ └─────────────────────────────────────────┘ │
│                                             │
│ Expiration                                  │
│ ┌─────────────────────────────────────────┐ │
│ │ 7 jours                              ▾  │ │  ← dropdown_menu
│ └─────────────────────────────────────────┘ │
│   (options : 1 h / 24 h / 7 j / 30 j / sans │
│    expiration)                              │
│                                             │
│ Taille max par dépôt                        │
│ ┌─────────────────────────────────────────┐ │
│ │ 2 Go                                 ▾  │ │
│ └─────────────────────────────────────────┘ │
│                                             │
│ Nombre maximum de fichiers                  │
│ ┌─────────────────────────────────────────┐ │
│ │ 50                                      │ │
│ └─────────────────────────────────────────┘ │
│                                             │
│ Texte de consentement                       │
│ ┌─────────────────────────────────────────┐ │
│ │ J'accepte que mes fichiers soient       │ │  ← textarea (multiline input)
│ │ transmis à <Cabinet Martin>.            │ │
│ └─────────────────────────────────────────┘ │
│                                             │
│              [ Annuler ]   [   Créer   ]    │  ← boutons
└─────────────────────────────────────────────┘
```

**Modal QR :**

```
┌─────────────────────────────────────────────┐
│ Lien : Pièces Dupont 2026             ×     │
├─────────────────────────────────────────────┤
│                                             │
│           ┌─────────────────┐               │
│           │ ███ QR CODE ███ │               │  ← qr_code_view
│           │ ███         ███ │               │
│           │ ████████████████│               │
│           └─────────────────┘               │
│                                             │
│   https://192.168.1.42:45457/deposit/...    │  ← --fs-body monospace
│                                             │
│            [ Copier l'URL ]                 │
│            [    Fermer    ]                 │
└─────────────────────────────────────────────┘
```

**Modal révocation :**

```
┌─────────────────────────────────────────────┐
│ Révoquer ce lien ?                          │
├─────────────────────────────────────────────┤
│ « Pièces Dupont 2026 »                      │
│                                             │
│ Les dépôts déjà reçus restent disponibles.  │
│ Aucun nouveau dépôt ne sera accepté sur ce  │
│ lien.                                       │
│                                             │
│         [ Annuler ]   [ Révoquer ]          │  ← bouton --error
└─────────────────────────────────────────────┘
```

---

### Filtres recommandés

Pills horizontales en haut de la liste : `Tous` (défaut) · `Actifs` ·
`Expirés` · `Révoqués`. Pas plus — sinon ça devient un sapin de
filtres et on est dans le pro/CRM. KISS.

---

### Recommandation surface B

**👉 Option B1 (Liste verticale pleine largeur)** — recommandée.

Justification :
- **Cohérence maximale** avec `device_list_item`, `file_row`, et l'esprit
  général du projet (listes denses, pas de grilles).
- **Implémentation la plus simple** : 1 widget `DepositLinkCard` +
  `ScrollArea`, pas de grid layout SFML maison.
- **URL visible** = workflow rapide pour le host : il scanne, il
  copie, il colle.
- B2 est joli mais sort du pattern projet et complique le layout.
- B3 est puissant mais surdimensionné pour un usage qui restera
  occasionnel (créer 1 lien tous les 15 jours, pas un Trello à plein
  temps).

---

## C) Parcours mobile-first du déposant (transverse aux 3 options A)

| Évènement | Comportement |
|---|---|
| Ouverture du lien sur 4G/Wi-Fi mobile | Page deposit.html chargée seule, < 30 Ko HTML/CSS/JS (sans icônes lourdes). |
| Rotation portrait → paysage | Layout reste en colonne, max-width 520 px, padding adaptatif. |
| Pinch zoom | Désactivé par meta viewport (`user-scalable=no`) pour éviter zoom involontaire sur drop-zone. |
| Sélection fichier sur iOS | Input `<input type="file" accept="*/*" multiple>` → bouton sheet natif. |
| Réseau coupé en cours d'upload | Toast haut « Connexion perdue. Réessayez. » + bouton « Réessayer » qui re-tente cette session (mêmes fichiers, pas d'upload partiel). |
| Onglet/app mis en arrière-plan pendant upload | L'upload continue tant que l'OS ne suspend pas l'onglet. Si suspension, l'utilisateur revient sur une page d'erreur courte au retour. |
| App PWA install prompt | **DÉSACTIVÉ** sur la page deposit (étanchéité = pas de promotion de l'app). |
| Mode sombre OS | Force `color-scheme: light` (cohérent avec `<meta name="color-scheme" content="light">` du dashboard). |

---

## D) Décisions visuelles transverses

| Élément | Choix |
|---|---|
| Police | Système (-apple-system / Segoe UI / Inter), pas de webfont à charger côté déposant |
| Couleurs accent | `--accent` indigo (cohérence visuelle), mais **sans** mentionner « LocalTransfer » |
| Icônes | Emoji natifs (📥, ✓, 🔒, ⏱, 📋, ⋮, QR) — zéro lib externe, zéro SVG à servir |
| Bordures | `--separator` (`#E2E8F0`), 1 px |
| Ombres | `--shadow` (`0 2px 8px rgba(0,0,0,0.10)`) sur cards uniquement |
| Boutons primaires | `--accent` indigo, hover `--accent-hover`, `--r-md`, `--fs-button` |
| Boutons secondaires | `--surface` + border `--separator` |
| Disabled state | `opacity: .5` + `pointer-events: none` |
| Toast erreur | Bandeau haut full-width, `--error` + texte blanc, auto-dismiss 5 s |

---

## ✋ VALIDATION REQUISE

Merci de choisir une option par surface. Réponse attendue sous la
forme `A?` + `B?` (ex. « A3 et B1 »).

**Surface A — Page web déposant :**
- A1 : Single-page linéaire (tout visible, scroll naturel)
- A2 : Wizard 2 étapes (identité puis fichiers)
- **A3 : Single-page progressif (recommandé)** — zone fichiers verrouillée tant que le form n'est pas valide

**Surface B — Écran desktop SFML :**
- **B1 : Liste verticale pleine largeur (recommandé)** — pattern du projet, simple
- B2 : Grille 2 colonnes compactes — dense, moderne, hors-pattern
- B3 : Master-detail avec pane de détails — riche mais lourd
