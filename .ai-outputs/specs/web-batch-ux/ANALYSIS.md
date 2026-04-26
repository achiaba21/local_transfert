# Web batch UX — Télécharger tout + Inbox host

> Deux features couplées : côté visiteur web un bouton « Télécharger
> tout » pour éviter les clics en série ; côté host une inbox
> non-invasive pour agréger les demandes entrantes web au lieu d'empiler
> des modales.

**Créé :** 2026-04-24
**État :** décisions validées, prêt pour `/feature full`.

---

## 📊 Phases

| Phase | Titre | Effort |
|-------|-------|--------|
| 1 | Télécharger tout web (bundle ZIP serveur, fallback séquentiel) | 3-4 j |
| 2 | Inbox host compact + modale liste | 3-4 j |
| 3 | Tests + doc | 1-2 j |

**Sprint total ~7-10 j.**

---

## 1. Axe A — « Télécharger tout » côté visiteur web

### État actuel
Quand le host envoie N fichiers, SSE `files-offer` liste les N tickets,
JS affiche N rows chacune avec bouton Télécharger. L'utilisateur doit
cliquer N fois.

### Décision Q1=C (hybride)

- **Si 1 fichier** → bouton unique « Télécharger » (comportement actuel)
- **Si ≥ 2 fichiers ET total < 4 Go** → bouton « Télécharger tout » qui
  utilise un bundle ZIP streamé côté serveur (réutilise
  `StreamingZipSource`)
- **Si ≥ 4 Go** → bouton « Télécharger tout » en mode séquentiel JS
  (fallback, chaque fichier DL un par un)

### Nouveau endpoint

`GET /api/download/bundle/<sessionId>` :
- Lookup tous les tickets de cette session dans `DownloadTicketStore`
- Vérifie que le `sessionToken` correspond au cookie
- Construit une liste `ZipEntry[]` :
  - Pour chaque ticket kind=File → `ZipEntry{abs=path, relInZip=displayName, size}`
  - Pour chaque ticket kind=StreamingZip → inclut tout son `zipEntries[]`
    (fusion des dossiers déjà zippés dans le super-zip)
- Instancie `StreamingZipSource` avec cette liste
- Stream la réponse avec Content-Length précalculé
- Nom proposé : `LocalTransfer-<date>.zip` (timestamp pour éviter
  collisions)

### Côté JS (`download.js`)

Au rendu de `files-offer`, on ajoute au-dessus de la liste :
```html
<div class="dl-bundle">
  <button id="dl-all-btn">Télécharger tout (3 fichiers · 45 Mo)</button>
</div>
```

Callback :
- Si total < 4 Go → fetch `/api/download/bundle/<sid>` → blob → download
- Si total ≥ 4 Go → boucle `downloadWithProgress` séquentiellement

### Impact code

| Fichier | Action |
|---------|--------|
| `src/web/routes/download_routes.cpp` | +route `/api/download/bundle/:sid` |
| `include/ltr/web/download_ticket_store.hpp` | +méthode `listBySession(sid)` |
| `src/web/download_ticket_store.cpp` | impl |
| `src/web/streaming_zip_source.cpp` | rien à changer (déjà générique) |
| `assets/web/js/download.js` | bouton Télécharger tout + logique hybride |
| `assets/web/css/style.css` | style bouton `.dl-bundle` |

---

## 2. Axe B — Inbox host (demandes entrantes web)

### Problème actuel
Chaque `POST /api/upload-announce` émet un `WebIncomingOfferEvent` →
modale Accept bloquante. Si le visiteur envoie 3 fichiers en 3 actions
successives (avant que l'host n'ait cliqué), 3 modales s'empilent.

### Décision Q2 = section compact non-invasive + modale liste complète (C)

Pattern symétrique avec le web : tous les fichiers reçus apparaissent
dans une liste, avec possibilité d'agir individuellement OU en global.

### 2 composantes

**Composante 1 — Badge compact (non-invasif)**

Pill cliquable dans le header, à droite de la pill self name :
```
LocalTransfer  ●  Mac de Serge   [📥 3 demandes]
```
- Masqué si 0 demande
- Visible si ≥ 1 demande, affiche le count
- Fond accent + texte blanc pour attirer l'œil sans casser le flow
- Clic → ouvre la modale liste complète
- Pas de blocage UI, l'user peut continuer à ajouter/envoyer des fichiers

**Composante 2 — Modale liste complète** (sous forme c)

```
┌─────────────────────────────────────────────────────────┐
│ DEMANDES ENTRANTES · 3   [Refuser tout] [Accepter tout] │
├─────────────────────────────────────────────────────────┤
│ ↓ iPhone (Safari) · photo.jpg · 2.4 Mo                  │
│                              [Refuser] [Accepter]       │
├─────────────────────────────────────────────────────────┤
│ ↓ iPhone (Safari) · notes.txt · 12 Ko                   │
│                              [Refuser] [Accepter]       │
├─────────────────────────────────────────────────────────┤
│ ↓ Android (Chrome) · MonDossier/ · 5 fichiers · 42 Mo   │
│                              [Refuser] [Accepter]       │
├─────────────────────────────────────────────────────────┤
│                                       [Fermer sans agir]│
└─────────────────────────────────────────────────────────┘
```

**Actions :**
- `[Accepter]` sur une ligne → folder picker → upload démarre
- `[Refuser]` sur une ligne → entrée retirée, visiteur voit « refusé »
- `[Accepter tout]` → UN folder picker → tous dans ce dossier
- `[Refuser tout]` → toutes retirées d'un coup
- `[Fermer sans agir]` → ferme la modale, les demandes restent en
  attente (badge visible dans le header)

### Changement de flux

**Avant :** `WebIncomingOfferEvent` → modale immédiate bloquante
**Après :** `WebIncomingOfferEvent` → ajout à `AppState::webInbox` →
badge header update. L'user clique badge ou clé Cmd+I → modale liste.

**Timeout announce** : le backend a déjà un `waitForDecision(uploadId,
120s)`. Si l'user ne clique pas en 2 min, le visiteur voit « refusé
timeout ». Ça reste comme aujourd'hui.

### Impact code

| Fichier | Action |
|---------|--------|
| `include/ltr/app/app_state.hpp` | +`std::vector<WebIncomingOffer> webInbox` |
| `src/app/app_controller.cpp` | onEvent WebIncomingOfferEvent → push dans inbox (plus de modale immédiate) ; +acceptWebUpload(uploadId) / refuseWebUpload(uploadId) accept UN id + acceptAllWebUploads() / refuseAllWebUploads() |
| `include/ltr/ui/screens/main_screen.hpp` | +helpers rects badge + modale |
| `src/ui/screens/main_screen.cpp` | +drawInboxBadge dans header + gestion clic → ouvrir modale |
| `include/ltr/ui/screens/incoming_offer_screen.hpp` | étendre pour afficher N demandes avec boutons individuels + globaux |
| `src/ui/screens/incoming_offer_screen.cpp` | refonte rendu liste |

### Backward compat flow natif TCP

Le flow d'offre native (`IncomingOfferEvent` via TCP LTR1) reste dans
la modale existante (PIN + accept). Seul le flow web inbox change. Les
2 cohabitent : modale PIN pour natif, modale inbox pour web.

---

## 3. Décisions produit validées

| # | Question | Décision |
|---|----------|----------|
| Q1 | Mode « Télécharger tout » | **C (hybride)** : ZIP serveur < 4 Go, séquentiel JS ≥ 4 Go |
| Q2 | Gestion des announces multiples | **Inbox non-invasive** : badge compact + modale liste complète au clic |
| Q3 | Comportement observé | Confirmé (multi-modales empilées = problème) |

---

## 4. Livrables

### Backend
- [ ] `GET /api/download/bundle/:sid` avec StreamingZipSource fusionné
- [ ] `DownloadTicketStore::listBySession(sid)` helper
- [ ] `AppState::webInbox` (remplace la modale immédiate)
- [ ] AppController : push inbox au lieu de ouvrir modale ; accept/refuse
      par uploadId + all

### UI
- [ ] Badge compact dans le header (pill accent « 📥 N »)
- [ ] Modale inbox liste complète (boutons individuels + globaux + fermer)
- [ ] JS : bouton « Télécharger tout (N · taille) » au-dessus de la liste
      des reçus web
- [ ] CSS : styles bouton + liste bundle

### Tests
- [ ] Test backend listBySession
- [ ] Smoke manuels :
  - Host envoie 5 fichiers au web → bouton Télécharger tout → reçoit 1 zip
  - Visiteur envoie 3 fichiers un par un (3 actions séparées) → host
    voit badge "3" → clic → modale liste avec 3 entrées → Accepter tout →
    1 folder picker → les 3 arrivent dedans
  - Mélange : 2 announces web + 1 offer natif TCP → flow natif garde
    sa modale PIN, flow web va dans l'inbox

### Doc
- [ ] Mise à jour `docs-agents/WEB.md` pour le bundle + inbox

---

## 5. Décision structurante bonus

**Le badge header se met à jour en temps réel** (via AppState drain
dans le tick). Quand un announce arrive :
1. `WebIncomingOfferEvent` posté par backend
2. `AppController::onEvent` push dans `state_.webInbox`
3. Prochain frame UI : badge s'affiche / count augmente

Quand l'user clique Accept/Refuse :
1. `AppController::acceptWebUpload(uploadId)` pop de l'inbox
2. Dispatch au `WebUploadAnnounceStore::resolveAccept/Refuse`
3. Prochain frame UI : badge count diminue / disparaît si 0

---

## 6. Hors scope V1

- Notifications OS natives au receipt d'un announce (repoussé UX-5)
- Preview miniature des fichiers entrants
- Historique des demandes refusées

---

## Journal

### 2026-04-24
- Analyse livrée
- Décisions Q1=C, Q2=inbox compact+modale, Q3=confirmé
- Placement inbox validé : badge compact non-invasif + modale C au clic
- Document créé, prêt pour `/feature full`
