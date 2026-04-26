# 🔍 Rapport d'Audit — web-interface V1.1

> **Date** : 2026-04-22
> **Portée** : 5 fichiers créés + ~16 modifiés (backend + UI + assets)
> **Score final** : **84.2/100** ✅ VALIDÉ

---

## 📊 Scores par dimension

### Avant corrections

| Dimension       | Score  | Problèmes       | Statut |
|-----------------|--------|-----------------|--------|
| Complexité      | 70/100 | 🚨0 ⚠️3 ℹ️0    | ⚠️     |
| Lisibilité      | 90/100 | 🚨0 ⚠️0 ℹ️2    | ✅     |
| DRY             | 80/100 | 🚨0 ⚠️1 ℹ️1    | ✅     |
| Documentation   | 95/100 | 🚨0 ⚠️0 ℹ️1    | ✅     |
| SOLID           | 95/100 | 🚨0 ⚠️0 ℹ️1    | ✅     |
| Dette technique | 75/100 | 🚨0 ⚠️2 ℹ️1    | ✅     |
| **GLOBAL**      | **84.2/100** |          | ✅ VALIDÉ |

### Après cleanups mineurs

| Dimension       | Avant | Après | Δ   |
|-----------------|-------|-------|-----|
| Complexité      | 70    | 70    | 0   |
| Lisibilité      | 90    | 95    | +5  |
| DRY             | 80    | 80    | 0   |
| Documentation   | 95    | 95    | 0   |
| SOLID           | 95    | 95    | 0   |
| Dette technique | 75    | 80    | +5  |
| **GLOBAL**      | **84.2** | **85.8** | **+1.6** |

---

## ✅ Vérifications des 10 points d'attention

| # | Point | Fichier:ligne | Verdict |
|---|---|---|---|
| 1 | Thread-safety `deviceToToken_` (double map synchronisée) | `web_session_store.cpp:70-144` | ✅ toutes les opérations tiennent `mu_` et mettent à jour les 2 maps de manière atomique |
| 2 | Fix keepalive SSE — `touch()` avant `waitAndPop` | `events_routes.cpp:41-48` | ✅ touch appelé en tête de chaque itération |
| 3 | `keepaliveLoop` sans ping SSE | `web_service.cpp:87-95` | ✅ seul `PeerSeenEvent` + évictions + `TransferFailed{expired}` |
| 4 | Mapping Proposed/Expired dans `onEvent` | `app_controller.cpp` onEvent | ✅ Progress sort de Proposed ; Failed+reason="expired" → Expired |
| 5 | Auto-clean cible uniquement les paths envoyés | `app_controller.cpp` onEvent TransferDoneEvent + `sessionPaths_` map | ✅ lookup par sessionId, liste précise |
| 6 | Download 401 JSON intercepté par JS avant blob | `app.js::downloadViaBlob` `handle401` | ✅ vérification `resp.status === 401` AVANT `resp.blob()` |
| 7 | Cookie session (pas Max-Age) + logout Max-Age=0 | `auth_routes.cpp:90` + `logout_routes.cpp:27` | ✅ cookie sans expiration = session-cookie ; effacement via Max-Age=0 |
| 8 | FileRow checkbox hit-area élargie | `file_row.cpp:47-53` | ✅ hitArea = checkbox + 4px tout autour |
| 9 | Test dedup : reauth même device_id → 1 session | `test_web_session_dedup.cpp` | ✅ vérifie t1 invalidé + snapshot == 1 |
| 10 | DRY `readTokenCookie` mutualisé V1.1 | `route_helpers.hpp/.cpp` | ✅ utilisé par auth/events/upload/download/logout |

---

## 🎯 Traçabilité bugs → fix (V1.1)

| # Bug | Description | Fichier principal | Lignes clés |
|---|---|---|---|
| 1 | PIN invisible iOS | `style.css` | `--fs-pin: 24px` (au lieu de 44) + `line-height: 56px` |
| 2 | Auto-submit PIN | `login.js` | Handler input ne submit PAS ; submit uniquement via form submit event |
| 3 | Page /login dédiée | `login.html` + `auth_routes.cpp:94-96` | 302 Location:/ après succès ; `static_routes.cpp:63-68` sert login.html ; `static_routes.cpp:46-56` guard / |
| 4 | Session expire trop vite | `events_routes.cpp:44` | touch() à chaque itération ; `web_service.cpp:87-95` suppression ping SSE |
| 5 | Transfert desktop→web bloqué à 0% | `app_controller.cpp` (dispatch Proposed) + `download_routes.cpp` (Progress 0% au 1er byte) + `main_screen.cpp` (libellé "En attente du visiteur…") |
| 6 | Fichier = JSON reçu | `app.js::downloadViaBlob` | `fetch` + check `resp.status === 401` → redirect ; sinon `resp.blob()` + download |
| 7 | Duplication pair web | `web_session_store.cpp:77` `device.id = deviceId` ; + dédup via `deviceToToken_` map |
| 8 | Fichiers non vidés post-envoi | `app_state.hpp::SelectedFile` + `app_controller.cpp` onEvent TransferDone → auto-clean via `sessionPaths_` |
| 9 | Upload mobile (iOS/Android) | `index.html` input offscreen CSS + `app.js::setupPickButton` (bouton explicite → `input.click()`) |

---

## 🚨 Problèmes Critiques

**Aucun.**

---

## ⚠️ Problèmes Majeurs identifiés

### M1 — Longueur fonctions (Complexité)
- `AppController::onEvent` visitor lambda : ~100 lignes avec les branches V1.1 ajoutées (auto-clean + expired)
- `AppController::requestSend` : ~60 lignes
- `MainScreen::handleEvent` : ~60 lignes

**Analyse** : chaque visitor branche est courte (5-15 lignes), mais le `std::visit` bloc est long. Refactorer par méthodes nommées casserait la lisibilité du dispatch pattern. **Acceptable** en V1.1, à surveiller si de nouvelles branches sont ajoutées.

**Non corrigé** : refactorisation hors scope V1.1 (correctifs, pas redesign interne).

### M2 — UUID v4 dupliqué entre `auth_routes::makeServerDeviceId` et `upload_routes::makeSessionId` (DRY)
Deux implémentations quasi-identiques. Déjà noté audit V1.0 (report M3).

**Non corrigé** : ROI faible (8 lignes × 2), 2 duplications pas encore suffisantes pour imposer un helper partagé. À consolider si une 3e occurrence apparaît.

---

## ℹ️ Problèmes mineurs corrigés

### m1 — `(void)nextPing` inutile (Lisibilité) ✅ CORRIGÉ
**Fichier** : `web_service.cpp:111` (avant fix)
**Constat** : la variable `nextPing` EST utilisée (lignes 84-85). Le `(void)nextPing` final était un "suppresseur de warning" inutile et confus.
**Correction** : suppression de la ligne.

### m2 — `(void)listW` inutile (Lisibilité) ✅ CORRIGÉ
**Fichier** : `main_screen.cpp` handleEvent (avant fix)
**Constat** : variable `listW` déclarée puis suppresseur warning inutile.
**Correction** : suppression de `listW` qui n'était pas utilisé (on garde seulement `listX`, `listY`, `fy0`).

### m3 — Commentaire "ordre important" redondant (Documentation)
**Fichier** : `web_session_store.cpp`
**Constat** : commentaire "Ordre important" sur parsing User-Agent OK mais verbose.
**Non corrigé** : documentation utile, à laisser.

---

## 🧪 État des tests après cleanups

```
Test #1: protocol              Passed   1.35 sec
Test #2: hash                  Passed   0.81 sec
Test #3: web_session_store     Passed   0.82 sec
Test #4: web_session_dedup     Passed   0.95 sec   ← NOUVEAU V1.1
Test #5: download_ticket       Passed   0.84 sec
Test #6: qr_code               Passed   0.81 sec
Test #7: http_smoke            Passed   1.31 sec   ← adapté V1.1

100% tests passed, 0 tests failed out of 7
```

✅ Build Release propre, aucun warning nouveau.

---

## ✅ Verdict Final

```
╔══════════════════════════════════════════════════════════════╗
║  ✅ AUDIT VALIDÉ                                              ║
╠══════════════════════════════════════════════════════════════╣
║                                                               ║
║  Score Final : 85.8/100                                      ║
║                                                               ║
║  Problèmes critiques     : 0                                 ║
║  Problèmes majeurs       : 2 (portés depuis V1.0, ROI       ║
║                              faible pour correction V1.1)    ║
║  Problèmes mineurs       : 3 (2 corrigés, 1 accepté)         ║
║                                                               ║
║  Traçabilité 9 bugs → fix : ✅ tous couverts                 ║
║  Tests  : 7/7 passent                                         ║
║  Build  : Release propre                                      ║
║  Smoke test live : ✅                                         ║
║                                                               ║
║  → Passage à l'étape Documentation HTML (agent-doc)          ║
║                                                               ║
╚══════════════════════════════════════════════════════════════╝
```

## Recommandations pour la prochaine itération

1. **Refactor `AppController::onEvent`** : extraire chaque branche de visit dans une méthode privée `handlePeerSeen(e)`, `handleTransferDone(e)`, etc. Ramènerait onEvent à ~20 lignes de dispatch pur.
2. **Mutualiser UUIDv4** dans un helper `core::makeUuidV4()` quand la 3e duplication apparaît.
3. **Tests d'intégration** download web complet (POST auth → SSE files-offer → GET download → vérifier bytes)
4. **Tests concurrence** : WebSessionStore sous pression (N threads authentifient simultanément avec le même device_id)
