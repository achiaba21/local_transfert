# Rapport d'audit — Sprint Web P2P V1.3

**Date :** 2026-05-02
**Scope :** 2 vagues. 1 fichier nouveau (transfer_registry.js, 333 LOC),
6 fichiers modifiés. p2p.js : 716 → 907 LOC.

---

## 📊 Scores

| Dimension       | Score     | Problèmes   | Statut |
| --------------- | --------- | ----------- | ------ |
| Complexité      | 70/100    | 🚨1 ⚠️1     | ✅     |
| Lisibilité      | 88/100    | ℹ️1         | ✅     |
| DRY             | 82/100    | ⚠️1         | ✅     |
| Documentation   | 90/100    | —           | ✅     |
| SOLID           | 85/100    | —           | ✅     |
| Dette technique | 75/100    | ⚠️1 ℹ️1     | ✅     |
| **GLOBAL**      | **82/100** |           | **✅** |

---

## 🚨 Problèmes Critiques

### Complexité — `p2p.js` 907 lignes (> seuil 500)

**Fichier :** `assets/web/js/p2p.js`
**Mesure :** 907 lignes ; le fichier mélange transport (RTCPeerConnection,
DataChannel), session (sendNextFile, handleReceiverControl), UI
(markCardSending, modal queue, sticky), helpers (safeSend, awaitDrain,
connKey).

**Décision :** accepté V1.3 — l'extraction d'un module
`p2p_transport.js` séparé est planifiée V1.4 quand la logique se
stabilisera. Pour l'instant, la cohésion intra-fichier reste correcte
(sections marquées par bandes de commentaires `// ====`). À surveiller
si on dépasse 1000 lignes.

---

## ⚠️ Problèmes Majeurs

### Complexité — `sendNextFile` 94 lignes, 4 niveaux d'imbrication

**Fichier :** `assets/web/js/p2p.js:392-500`
**Mesure :** 94 lignes ; imbrication try → while → while → while
(read stream → chunks → backpressure). 4 chemins de skip pour gestion
per-file failure (`fs.error = 'meta'/'read'/'send'/'end'`).

**Décision :** accepté V1.3 — chaque chemin a une raison fonctionnelle
distincte (intégrité par étape : annonce / lecture / chunks /
finalisation). Extraction en sous-fonctions ajouterait de l'indirection
sans simplifier réellement la logique séquentielle.

### DRY — Pattern « skip + continue » dupliqué 4 fois

**Fichier :** `assets/web/js/p2p.js`
**Mesure :** 4 occurrences de :
```js
fs.status = 'failed'; fs.error = 'X';
syncFileStatus(state, fs);
state.currentFileIdx += 1;
sendNextFile(state).catch(() => {});
return;
```
**Plan V2 :** extraire un helper `skipFailedFile(state, fs, errorTag)`.

### Dette — Fonction god `sendNextFile`

Cf. ⚠️ ci-dessus. Symptôme du même problème.

---

## ℹ️ Améliorations Suggérées

### Lisibilité — Magic number `1000` dans `ackWatchdog`

**Fichier :** `p2p.js:328`
**Mesure :** `setInterval(..., 1000)` — fréquence du watchdog d'ack.
Nommage : `ACK_WATCHDOG_INTERVAL_MS = 1000`.

### Dette — Pas de UNIT TEST pour transfer_registry.js

Le module a 333 LOC avec localStorage, render(), retry — testable
unitairement. Pas urgent (logique JS browser, pas critique sécurité).
À ajouter quand on aura un harness JS (jest/vitest).

---

## ✅ Points positifs

### Wave 1 — Robustesse

- 4 watchdogs explicites couvrent les principaux silent stalls :
  `noDataWatchdog` (receveur 10 s), `ackWatchdog` (sender, 1 s polling),
  `connectTimer` (20 s ICE), `disconnectTimer` (15 s flottement)
- Intégrité par fichier : `cur.received !== cur.size` → marqué failed,
  pas de download. Empêche les fichiers tronqués téléchargés en
  silence
- Drain timeout 30 s : la fin de session ne peut plus boucler
  indéfiniment
- Disconnect transitoire : pause des sends pendant flottement Wi-Fi,
  reprise auto à `connected`. Avant on n'agissait que sur `failed`
  → chunks perdus
- Composite key `${deviceId}:${role}` : permet A↔B simultané
  proprement, pas d'écrasement

### Wave 2 — Liste UI persistante

- TransferRegistry parfaitement isolé : 1 fichier, 1 responsabilité,
  testable en isolation
- localStorage ne stocke QUE les métadonnées (Blobs jamais persistés)
  → pas de risque de quota
- Cap 100 entrées avec drop FIFO → robuste
- Boot avec migration : entries `pending`/`sending` après refresh
  → marquées `failed reason='session_perdue'` (UX honnête)
- Notifications complètes : toast + WebAudio (sinusoïdal 880 Hz fade)
  + `navigator.vibrate(200)` mobile
- Animation fade-in slide-down 200 ms — discrete et performante
- Mobile-first respecté : tient sur 360 px

### Architecture

- `cleanup(state, label)` au lieu de `cleanup(deviceId)` → plus
  robuste pour le bidirectionnel
- `_cleaned` flag empêche les doubles cleanups
- `safeSend` async qui patiente pendant `disconnectedSince` actif —
  élégant
- `syncFileStatus` helper centralise le pont state/Registry
- Tous les timers (ttl, ack, watchdog, disconnect) explicitement
  clearTimeout/clearInterval dans cleanup

### Backward compat

- 14/14 tests passent (aucune régression backend)
- V1.2 nominal flow inchangé
- Tabs Host conservée pour les transferts host↔web (pas de mélange)
- Sprints précédents (UX-1..4, Resume MVP, UI Layout, V1.1, V1.2)
  intacts

### Sécurité

- Aucun changement backend, le signaling reste auth + validation
- localStorage : juste des métadonnées (pas de secret)
- Aucune nouvelle dépendance externe

---

## Verdict

**Score : 82/100**

**✅ VALIDÉ** — Les 4 lots sont livrés et fonctionnels :
- Lot 1 (robustesse) : watchdogs + intégrité + drain + disconnected
- Lot 2 (liste UI) : Registry + tabs + retry + notif son/vibration
- Lot 3 (ack) : receveur émet, sender utilise pour vraie progression
  + détection silent stall
- Lot 4 (composite key) : A↔B simultané, cleanup propre

Les ⚠️ sur la taille de p2p.js (907 LOC) et sendNextFile (94 lignes)
sont structurels — accepté V1.3, refactor V1.4 prévu (extraction
`p2p_transport.js`).

**À surveiller V2 :** `skipFailedFile` helper pour éliminer la
duplication × 4 ; nommer le magic number 1000 ms ; tests unitaires
JS pour transfer_registry.js.
