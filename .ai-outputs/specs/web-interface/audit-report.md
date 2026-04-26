# 🔍 Rapport d'Audit — Feature web-interface

> **Date** : 2026-04-22
> **Portée** : 40 fichiers créés + 9 modifiés (~3000 lignes C++/HTML/CSS/JS)
> **Score final** : **93.3/100** ✅ VALIDÉ

---

## 📊 Scores par dimension

### Avant corrections

| Dimension       | Score  | Problèmes        | Statut |
|-----------------|--------|------------------|--------|
| Complexité      | 95/100 | 🚨0 ⚠️0 ℹ️1     | ✅      |
| Lisibilité      | 85/100 | 🚨0 ⚠️1 ℹ️1     | ⚠️      |
| DRY             | 70/100 | 🚨0 ⚠️3 ℹ️0     | ⚠️      |
| Documentation   | 95/100 | 🚨0 ⚠️0 ℹ️1     | ✅      |
| SOLID           | 95/100 | 🚨0 ⚠️0 ℹ️1     | ✅      |
| Dette technique | 60/100 | 🚨2 ⚠️0 ℹ️0     | ⚠️      |
| **GLOBAL**      | **83.3/100** |             | ✅      |

### Après corrections (4 fixes critiques/majeurs appliqués)

| Dimension       | Avant  | Après   | Δ   |
|-----------------|--------|---------|-----|
| Complexité      | 95     | 95      | +0  |
| Lisibilité      | 85     | 95      | +10 |
| DRY             | 70     | 85      | +15 |
| Documentation   | 95     | 95      | +0  |
| SOLID           | 95     | 95      | +0  |
| Dette technique | 60     | 100     | +40 |
| **GLOBAL**      | **83.3** | **94.2** | **+10.9** |

---

## 🚨 Problèmes Critiques identifiés et corrigés

### C1 — `TransferDoneEvent` émis AVANT la fin réelle du stream

**Fichier** : `src/web/routes/download_routes.cpp:158` (avant fix)
**Constat** : `bus.post(TransferDoneEvent{...})` était invoqué juste après `set_content_provider()`. Or cette méthode planifie seulement la callback — les chunks sont streamés plus tard, dans un thread cpp-httplib, après le retour de la lambda externe. Conséquence : l'UI voyait le transfert « terminé » alors que les `TransferProgressEvent` commençaient seulement. Inversion de sémantique.
**Mesure** : événement émis **N-1** ticks en avance (avant tous les progress events).
**Correction** : déplacement du `post Done` à l'intérieur de la callback provider, guardé par un flag `shared_ptr<bool> doneEmitted`, déclenché quand `got == 0` (fin de fichier atteinte).

### C2 — Code mort : `WebService::makeToken()` + `makeUuidV4()`

**Fichier** : `src/web/web_service.cpp:40-73` (avant fix)
**Constat** : Deux fonctions statiques implémentées (35 lignes totales) mais **jamais appelées**. La génération de token se fait dans `WebSessionStore::authenticate` et dans `upload_routes::makeSessionId`.
**Mesure** : 35 lignes de code mort + 2 déclarations dans le header.
**Correction** : suppression complète des deux méthodes (header + impl).

---

## ⚠️ Problèmes Majeurs identifiés

### M1 — `readTokenCookie` dupliqué 4× (DRY) ✅ CORRIGÉ

**Fichiers** : `auth_routes.cpp`, `upload_routes.cpp`, `download_routes.cpp`, `events_routes.cpp` (avant fix)
**Constat** : même fonction de 10 lignes, recopiée 4 fois à l'identique.
**Correction** : extraction dans `include/ltr/web/routes/route_helpers.hpp` + `src/web/routes/route_helpers.cpp`. Les 4 duplications sont remplacées par un `#include`.

### M2 — Magic number `256 * 1024` (Lisibilité) ✅ CORRIGÉ

**Fichier** : `src/web/routes/download_routes.cpp:117` (avant fix)
**Constat** : `std::min<std::size_t>(length, 256 * 1024)` — constante inline alors que `core::kChunkSize` existe et vaut exactement 256 * 1024 dans `include/ltr/core/types.hpp`.
**Correction** : remplacement par `core::kChunkSize`.

### M3 — Génération hex 32 chars dupliquée 3× (DRY) ⚠️ NON CORRIGÉ (reporté)

**Fichiers** : `web_session_store.cpp:60-68`, `download_ticket_store.cpp:14-23`, (et ex-`WebService::makeToken` supprimé).
**Constat** : boucle identique de 16 bytes → 32 hex chars dans 2 stores différents.
**Raison du report** : les stores sont conçus autonomes (découplés) — partager une fonction utilitaire obligerait à créer un header supplémentaire pour une économie de 8 lignes × 2 = 16 lignes. ROI faible vs complexité d'organisation. À consolider si apparaît une 3e occurrence.

### M4 — Génération UUID v4 dupliquée (DRY) — RÉSOLU par C2

**Fichier** : `upload_routes.cpp::makeSessionId` (anciennement dupliqué dans `WebService::makeUuidV4`).
**Statut** : le code mort C2 supprimé a mécaniquement éliminé cette duplication. Reste une seule implémentation dans `upload_routes.cpp`. OK.

---

## ℹ️ Améliorations mineures

### m1 — Callback imbriquée dans `download_routes` (Complexité)
**Constat** : 3 niveaux d'imbrication dans la lambda set_content_provider.
**Action** : accepté en l'état — extraction en fonction nommée alourdirait le code (capture d'environnement + forward de nombreux shared_ptr). Correct tant que ≤ 1 lambda complexe par fichier.

### m2 — Commentaire confus de 12 lignes (Lisibilité) ✅ CORRIGÉ
**Fichier** : `download_routes.cpp:146-157` (avant fix)
**Correction** : remplacé par un commentaire de 3 lignes expliquant le positionnement de `Done` dans la callback, au bon endroit désormais.

### m3 — Écart PIN web/natif pas dans business-spec.md (Documentation)
**Statut** : documenté dans `architecture.md §11` + `WEB.md` (Point de vigilance #5). Non propagé à `business-spec.md` (source R3).
**Action** : à propager si l'utilisateur valide explicitement l'écart. Reste une note dans WEB.md pour l'instant.

---

## ✅ Points de vigilance vérifiés (du prompt d'audit)

| # | Point vérifié                                            | Statut   |
|---|----------------------------------------------------------|----------|
| 1 | Thread-safety des stores (mutex partout, pas de retour de référence protégée) | ✅ OK |
| 2 | Dispatch `AppController::requestSend` propre, pas de régression natif         | ✅ OK |
| 3 | Threads `keepaliveThread_` + `listenerThread_` joined au shutdown             | ✅ OK |
| 4 | Écart PIN web/natif documenté dans WEB.md                                     | ✅ OK (note m3) |
| 5 | `TransferDoneEvent` émis au bon moment (fin réelle du stream)                 | ✅ CORRIGÉ (C1) |
| 6 | Self-binary macOS en RAM complète — acceptable V1, noté pour V2               | ✅ Accepté |
| 7 | Duplication `readTokenCookie` — mutualisée                                    | ✅ CORRIGÉ (M1) |
| 8 | UI : `Label` applique `utf8()` automatiquement, SharePanel utilise RoundedRect (via `Card`), constantes via `Theme` | ✅ OK |

---

## 🧪 État des tests après corrections

```
Test #1: protocol              Passed   1.15 sec
Test #2: hash                  Passed   0.79 sec
Test #3: web_session_store     Passed   0.77 sec
Test #4: download_ticket       Passed   0.84 sec
Test #5: qr_code               Passed   0.83 sec
Test #6: http_smoke            Passed   1.33 sec

100% tests passed, 0 tests failed out of 6
```

✅ Build Release propre, 0 warning ajouté par les fixes.

---

## ✅ Verdict Final

```
╔══════════════════════════════════════════════════════════════╗
║  ✅ AUDIT VALIDÉ                                              ║
╠══════════════════════════════════════════════════════════════╣
║                                                               ║
║  Score Final : 94.2/100                                      ║
║                                                               ║
║  Problèmes critiques corrigés : 2/2                          ║
║  Problèmes majeurs corrigés   : 2/3 (M3 reporté, ROI faible) ║
║  Mineurs corrigés              : 1/3                          ║
║                                                               ║
║  Tests  : 6/6 passent                                         ║
║  Build  : Release sans erreur ni warning nouveau              ║
║                                                               ║
║  → Passage à l'étape Documentation (agent-doc)               ║
║                                                               ║
╚══════════════════════════════════════════════════════════════╝
```

## Recommandations pour V2 (pas bloquantes)

1. Consolider `M3` (génération hex) quand un 3e appelant apparaîtra
2. Streaming macOS `.app` zip via content provider cpp-httplib (au lieu de RAM complète)
3. Rate-limit/lockout sur échecs PIN répétés
4. Persistance disque des sessions web (survit au redémarrage app)
5. HTTPS via cert self-signed acceptable sur LAN privé + bouton « Faire confiance »
