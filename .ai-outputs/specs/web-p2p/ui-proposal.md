# UI/UX Proposal — Sprint Web P2P (V1.2)

**Date :** 2026-05-01
**Statut :** ✅ Validée (mobile-first)

---

## Cible primaire

Navigateurs mobiles (iPhone/Android, 360-430 px). Desktop = scale up via
media queries.

---

## Surface 1 — Section peers : pucks horizontaux

**Choix : Option A** — pucks horizontaux scroll-snap, sous le header.

### Layout
- Card peer 80×96 px
  - Cercle emoji 56×56 px (Radius::lg, fill accentLight)
  - Nom 12 px bold tronqué (ellipsis sur 1 ligne)
- Scroll-snap horizontal natif CSS
- Tap → file picker → confirme « Envoyer X fichiers à <peer> ? »
- Long-press / hover → tooltip « Pingouin Bleu · iPhone · Safari »
- Le **host** apparait comme premier peer (🖥) — unifie web↔host et
  web↔web sous le même geste

### Markup HTML attendu
```html
<section class="peers-section">
  <h2 class="overline">À envoyer à…</h2>
  <ul class="peers-list" id="peers-list" role="listbox">
    <!-- généré dynamiquement -->
  </ul>
</section>
```

### CSS variables
- `--peer-card-w: 80px; --peer-card-h: 96px;`
- `--peer-emoji-size: 56px;`
- `scroll-snap-type: x mandatory; scroll-snap-align: start;`

---

## Surface 2 — Modale réception : bottom-sheet adaptive

**Choix : Option A** — bottom sheet (mobile) ↔ centered modal (desktop).

### Comportement
- **< 720 px** : sheet ancrée en bas, slide-up animation 200 ms easeOut,
  drag-handle visible (━━━), swipe-down = refuser (avec confirmation
  legère)
- **≥ 720 px** : modale centrée, overlay sombre, taille 480×360 max
- File d'attente : si 2 offres simultanées, on traite la 1ère, la sheet
  se rouvre pour la 2e après décision

### Contenu
- Emoji 56 px (Radius::full)
- Titre « <Nom> veut t'envoyer »
- Sous-titre « 3 fichiers · 12,4 Mo »
- Liste des 1-2 premiers fichiers + « + N autres »
- 2 boutons 44 px : `[Refuser]` (Secondary) et `[Accepter]` (Primary)
- TTL 60 s : barre de timeout discrète qui shrink au-dessus des boutons

### Markup HTML attendu
```html
<div class="p2p-incoming-modal" id="p2p-incoming-modal" hidden>
  <div class="p2p-sheet">
    <div class="sheet-handle"></div>
    <div class="p2p-emoji">🦊</div>
    <h2 class="p2p-title">Pingouin Bleu veut t'envoyer</h2>
    <p class="p2p-summary">3 fichiers · 12,4 Mo</p>
    <ul class="p2p-files">…</ul>
    <div class="p2p-ttl-bar"><span class="p2p-ttl-fill"></span></div>
    <div class="p2p-actions">
      <button class="btn btn-secondary" id="p2p-refuse">Refuser</button>
      <button class="btn btn-primary"   id="p2p-accept">Accepter</button>
    </div>
  </div>
</div>
```

---

## Surface 3 — Progress P2P en cours

**Choix : Option A** — card peer transforme + mini-bar sticky bas.

### Comportement
- **Card peer pendant transfert** : remplace le sous-titre platforme par
  « Envoi · 67 % · 8 Mo/s · 3 s », barre de progression intégrée au bas
  de la card
- **Mini-bar sticky bas** : visible dès qu'au moins 1 P2P actif. Tap →
  scroll smooth vers la card concernée. Disparait quand 0 transfert
- **N transferts simultanés** : la mini-bar affiche « ↑ N transferts P2P
  · X % » (moyenne pondérée)

### Markup HTML attendu
```html
<!-- mini-bar globale -->
<div class="p2p-sticky-bar" id="p2p-sticky-bar" hidden>
  <span class="p2p-sticky-icon">↑</span>
  <span class="p2p-sticky-text">1 transfert P2P · 67 %</span>
  <span class="p2p-sticky-chevron">›</span>
</div>

<!-- état progress dans une card peer -->
<li class="peer-card peer-card--sending" data-device-id="…">
  <span class="peer-emoji">🦊</span>
  <span class="peer-name">Pingouin Bleu</span>
  <span class="peer-progress-text">67 % · 8 Mo/s</span>
  <div class="peer-progress-bar"><span style="width:67%"></span></div>
</li>
```

---

## Détails complémentaires

- Header : badge « N appareils » à côté du host pill quand N>0
- Logout / SSE perdu : sheet se referme avec message « Connexion perdue »
- Refus côté émetteur : toast 3 s « <Peer> a refusé » (Colors::warning)
- ICE failed : card peer affiche temporairement « Connexion P2P échouée »
  (Colors::error) puis retour au state normal après 3 s
- Empty state : si 0 peer, masquer toute la section peers (pas de
  placeholder gênant) ; afficher seulement l'avatar host

## Tokens à respecter

- Couleurs : Colors::accent, accentLight, surface, sidebar, text,
  textSecondary, error, warning, separator
- Radius : sm/md/lg/full
- Spacing : sm/md/lg/xl
- FontSize : overline/small/body/h1
- Animation : ease-out, durée 200 ms (cohérent avec animation.cpp)
