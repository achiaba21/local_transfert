# Rapport d'audit — Sprint Web P2P (V1.2)

**Date :** 2026-05-01
**Scope :** 3 vagues. 8 fichiers nouveaux, 12 modifiés. ~1100 lignes
ajoutées.

---

## 📊 Scores

| Dimension       | Score    | Problèmes   | Statut |
| --------------- | -------- | ----------- | ------ |
| Complexité      | 75/100   | ⚠️1 ℹ️1     | ✅     |
| Lisibilité      | 90/100   | ℹ️1         | ✅     |
| DRY             | 85/100   | ℹ️1         | ✅     |
| Documentation   | 90/100   | —           | ✅     |
| SOLID           | 90/100   | —           | ✅     |
| Dette technique | 80/100   | ⚠️1 ℹ️1     | ✅     |
| **GLOBAL**      | **85/100** |           | **✅** |

---

## ⚠️ Problèmes Majeurs

### Complexité — `sendNextFile` long (65 lignes, imbrication 4)

**Fichier :** `assets/web/js/p2p.js:227-292`
**Mesure :** 65 lignes ; 4 niveaux d'imbrication (try > while > while >
while). La logique : flush meta JSON → boucle ReadableStream → boucle
chunks → boucle backpressure.
**Décision :** accepté V1 — chaque niveau a une raison fonctionnelle
distincte (lecture stream / chunking / flow-control). Extraction en
helpers (`writeChunk`, `awaitDrain`) possible mais ajoute de l'indirection
sans simplification réelle. À refactorer si la logique grandit.

### Dette — `catch {}` silencieux dans 6 endroits

**Fichier :** `assets/web/js/p2p.js` (l. 142, 145, 232-233, 311-312)
+ `cleanup()` (try/catch sur close)
**Mesure :** Catch vides sur `pc.addIceCandidate`, `dc.send` final,
`pc.close`, `JSON.parse`. Tous best-effort post-cleanup ou sur API qui
peut légitimement échouer (candidate format toléré côté navigateur).
**Décision :** acceptés V1 — ces erreurs ne doivent PAS interrompre le
cleanup. Si on remonte, on risque de double-cleanup. Documenter la
raison dans un commentaire serait un plus (suggéré).

---

## ℹ️ Améliorations Suggérées

### DRY — Format message SSE construit à 2 endroits

**Fichiers :** `src/web/web_service.cpp::emitWebPeersTo` et
`src/web/routes/p2p_routes.cpp`
**Mesure :** la construction `"event: NAME\ndata: " + json + "\n\n"` est
dupliquée. Extraire un helper `sendNamedEvent(token, name, json)` sur
`SseBroadcaster` simplifierait la prochaine route SSE qui apparaitrait.
**Plan V2 :** ajouter `SseBroadcaster::sendEvent(token, name, jsonData)`.

### Lisibilité — Magic number 200ms dans cleanup

**Fichier :** `assets/web/js/p2p.js:232, 271`
**Mesure :** `setTimeout(..., 200)` apparait 2 fois (close après
all-done, fallback safety pour bufferedamountlow). À nommer en
constante (`CLOSE_GRACE_MS`).

### Complexité — `p2p.js` 522 lignes

**Fichier :** `assets/web/js/p2p.js`
**Mesure :** Module mono-fichier. Au-delà de 600 lignes il faudra
scinder (sender vs receiver vs UI helpers). À surveiller.

### Dette — Pas de cas d'usage simultané sender + receiver pour le même peer

**Mesure :** Si Alice envoie à Bob ET Bob envoie à Alice simultanément,
les deux états entrent dans `conns` indexé par `deviceId` côté Alice et
Bob mais seul un est conservé. La spec valide les transferts parallèles
**vers plusieurs destinataires différents**, pas bidirectionnels avec le
même peer. Probabilité faible. À monitorer en V2.

---

## ✅ Points positifs

### Wave 1 — DisplayName + annuaire
- 30×30 = 900 combinaisons FR stables, hash FNV-1a déterministe
- Cleanly séparé : DisplayName ne touche pas à `domain::Device::name` (pas
  de régression desktop)
- 1 test unitaire (5 cas) — déterminisme, invariance UA, parsing
  plateforme, fallback empty

### Wave 2 — Signaling
- Validation expéditeur cookie + destinataire `findTokenByDeviceId` → pas
  de routing fantôme
- Whitelist 6 types (offer/answer/ice/refuse/cancel/bye) — refus 400
  des payloads malformés
- Self-target bloqué (400) — empêche la confusion d'état
- Test 6 cas (401, 400×3, 404, 204) — couverture solide

### Wave 3 — WebRTC core
- Backpressure correcte : `bufferedAmount > BUFFER_HIGH` avec event
  `bufferedamountlow` + safety setTimeout 200ms
- Chunking 64 KB respecte la spec (max 65535)
- File reading via `file.stream().getReader()` — RAM stable même pour
  gros fichiers (pas de FileReader.readAsArrayBuffer)
- Cleanup symétrique : `pc.close + dc.close + clear timer + restore UI`
- ICE candidates trickle natif (onicecandidate → postSignal)

### UI
- Mobile-first respecté : bottom-sheet → modale desktop via
  `@media (min-width: 720px)` partout où requis
- Tokens CSS centralisés (--accent, --r-md…), pas de hardcoded
- Touch targets ≥44px
- ARIA : role=listbox, role=dialog, aria-live=polite

### Backward compat
- 14/14 tests passent (12 anciens + 2 nouveaux)
- Flow host↔web V1.1 inchangé (fileoffer / download / upload routes
  intactes)
- Sprints UX-1..4, Transfer Resume, Web Batch UX, UI Layout System :
  intacts

### Sécurité
- Aucune fuite cross-session : chaque message SSE p2p-signal est routé
  au seul destinataire via son token
- Le host **ne lit jamais** le payload (pure relay JSON opaque)
- Self-target bloqué (empêche écho)
- Pas de TURN public → données restent en LAN

---

## Verdict

**Score : 85/100**

**✅ VALIDÉ** — Le sprint atteint son objectif principal : annuaire
mutuel des peers web (Lot 1), nom auto stable (Lot 2), signaling sûr
(Lot 3), transferts WebRTC chunked + UI réception adaptive (Lot 4),
gestion erreurs / TTL / cleanup (Lot 5). Pas de régression. Les 2
problèmes ⚠️ sont assumés (logique fonctionnelle, catch best-effort) et
documentés.

À surveiller V2 : extraction d'un `sendEvent` helper sur SseBroadcaster
quand on ajoutera une 4e route SSE ; scinder p2p.js si la logique passe
600 LOC.
