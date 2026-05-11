# Audit V1.6.5 — Stabilité, Sessions résilientes, Historique

**Sprint** : `stability-sessions-history-v1-6-5`
**Date** : 2026-05-03
**Périmètre** : 4 vagues (Stabilité HTTP, Resume P2P, Sessions résilientes, Historique TOFU)
**Build** : Release propre — 21/21 tests passent (4 nouveaux V1.6.5)

---

## 1. Score Global

| Dimension       | Score    | 🚨 | ⚠️ | ℹ️ | Statut |
|-----------------|----------|----|----|----|--------|
| Complexité      | 70/100   | 1  | 1  | 0  | ⚠️ |
| Lisibilité      | 90/100   | 0  | 1  | 0  | ✅ |
| DRY             | 90/100   | 0  | 1  | 0  | ✅ |
| Documentation   | 95/100   | 0  | 0  | 1  | ✅ |
| SOLID           | 80/100   | 0  | 2  | 0  | ✅ |
| Dette technique | 65/100   | 0  | 3  | 1  | ⚠️ |
| 🔒 Sécurité     | 70/100   | 0  | 2  | 2  | ⚠️ |
| **GLOBAL**      | **80/100** |    |    |    | **✅ VALIDÉ AVEC RÉSERVES** |

**Verdict** : Le sprint V1.6.5 est **VALIDÉ AVEC RÉSERVES** (score ≥ 60). L'architecture est saine, les tests sont robustes (21/21), les nouveaux modules `PeersHistory` / `TransferHistory` sont propres. Les réserves portent sur (a) la longueur des handlers `streamFile`/`streamZip`, (b) une intégration JS incomplète du refresh silencieux, (c) un placeholder de sécurité (PIN remember) à durcir pour V2.

---

## 2. Détail par Dimension

### 2.1 Complexité — 70/100

#### 🚨 [Critique] Long Method — `streamFile`

- **Fichier** : `src/web/routes/download_routes.cpp:51-196`
- **Mesure** : 146 lignes de fonction (seuil critique > 50)
- **Détail** : la lambda `set_content_provider` capture 9 variables, contient 4 paths de termination (cancel, EOF, write fail, OK), recalcule throttle + ETA inline.
- **Correction proposée** : extraire 3 helpers dans un fichier dédié `download_streamer.cpp` :
  - `applyRange(req, res, totalSize) → {streamStart, streamLen}` (lignes 67-92)
  - `emitProgressIfDue(now, sentTotal, lastTime, lastBytes, startTime, sz, sid, bus)` (lignes 171-193)
  - `terminateProvider(reason, sessionId, doneEmitted, bus, svc)` (factorise le triplet `bus.post + *doneEmitted=true + svc.releaseCancelFlag(sid)` répété 3-4×).
  La fonction `streamFile` réduit alors à ~50 lignes.
- **Pénalité** : −20

#### ⚠️ [Majeur] Long Method — `streamZip`

- **Fichier** : `src/web/routes/download_routes.cpp:199-289`
- **Mesure** : 91 lignes (seuil majeur > 30, critique > 50)
- **Détail** : structure quasi-identique à `streamFile` (cancel → errored → EOF → progress throttle).
- **Correction proposée** : profite des helpers ci-dessus + un `ProgressThrottle` partagé.
- **Pénalité** : −10

✅ Tout le reste est propre :
- `peers_history.cpp` (211 l), `transfer_history.cpp` (235 l), `range_parser.cpp` (29 l) — fonctions courtes (la plus longue : `load()` ~30 l, acceptable pour parse + purge).
- HMAC `hmacSha256Hex` 27 lignes — bornes RFC 2104 respectées.
- `purgeOlderThan` (PeersHistory) : iterator-erase pattern O(N) — correct.
- `enforceCap` (TransferHistory) : `vector::erase` O(N) sur un drop en bloc — correct (commentaire audit était une fausse alerte : pas d'O(N²)).

**Score : 70/100**

---

### 2.2 Lisibilité — 90/100

#### ⚠️ [Majeur] Magic numbers résiduels

- **Fichier** : `src/web/routes/download_routes.cpp:43-44`
  ```cpp
  constexpr std::chrono::milliseconds kProgressInterval{100};
  constexpr std::uint64_t kProgressByteStep = 1 * 1024 * 1024;
  ```
  → OK (constantes nommées).
- **Fichier** : `src/web/web_session_store.cpp:64-75` (`makeToken`) — `16` octets, `0xFF`, hex chars. Valeurs cryptographiques standard, bien commentées.
- **Fichier** : `assets/web/js/p2p_transport.js:19-23` — `15_000`, `20_000`, `5_000` ms. Valeurs nommées (`DISCONNECT_TTL_MS` etc.), bien.
- **Fichier** : `src/web/routes/auth_routes.cpp:28-31` — manipulation UUID v4 (`bytes[6] | 0x40`, `bytes[8] | 0x80`) — **non commentée** (RFC 4122 v4 marker bytes). Mineur, mais crypto-aware.
- **Fichier** : `src/infra/transfer_history.cpp:16-18`
  ```cpp
  constexpr std::size_t kMaxEntries  = 1000;
  constexpr std::int64_t kRetentionSec = 180LL * 24 * 3600;
  ```
  OK, nommées + commentées.
- **Pénalité** : −10 (un seul groupe de magic numbers, ailleurs c'est propre)

✅ Excellents points :
- Tous les noms de fonctions sont des verbes (`makePersistentToken`, `verifyPersistentToken`, `releaseCancelFlag`, `enforceCap`, `purgeOlderThan`).
- Cohérence camelCase partout en C++, snake_case dans les routes JSON.
- Aucune ligne > 120 chars dans les fichiers nouveaux. Quelques alignements `auto =` artisanaux dans `peers_history.cpp:96-100` mais lisibles.
- Pas d'abréviations cryptiques (sauf `sid` dans logs — convention projet OK).

**Score : 90/100**

---

### 2.3 DRY — 90/100

#### ⚠️ [Majeur] Quasi-duplication progress-throttle

- **Fichiers** : `download_routes.cpp:171-193` et `download_routes.cpp:264-286`
- **Détail** : le bloc « si (sinceLast ≥ kProgressInterval || bytesSinceLast ≥ kProgressByteStep) → calcule speedBps + ETA + post(TransferProgressEvent) → reset trackers » est dupliqué presque verbatim entre `streamFile` et `streamZip`.
- **Correction proposée** : extraire en helper :
  ```cpp
  void emitProgressIfDue(std::chrono::steady_clock::time_point now,
                         std::uint64_t sent,
                         /* refs */ time_point& lastT, uint64_t& lastB,
                         time_point startT, uint64_t total,
                         const std::string& sid, EventBus& bus);
  ```
- **Pénalité** : −10

✅ Excellent :
- `readNamedCookie` factorisée dans `route_helpers.cpp:8-18` puis utilisée par `readTokenCookie` et `readRememberCookie` — DRY exemplaire (item du sprint).
- Logout efface les 2 cookies (`ltr_token` + `ltr_remember`) en suivant le même pattern.
- `nowSec()` privé dans 2 anonymous namespaces (`peers_history.cpp` + `transfer_history.cpp`) — duplication mineure mais acceptable (chaque module reste autonome) ; pourrait migrer vers `core::format::nowSec()` dans une prochaine itération.
- `releaseCancelFlag(sessionId)` répété 7× dans download_routes : c'est nécessaire (chaque path de termination) mais pourrait être encapsulé via RAII (`CancelFlagGuard{svc, sid}`) ou un helper `terminate(...)`.

**Score : 90/100**

---

### 2.4 Documentation — 95/100

#### ℹ️ [Mineur] Manque WHY pour la marker UUID

- **Fichier** : `src/web/routes/auth_routes.cpp:30-31`
  ```cpp
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  ```
  → ajouter commentaire `// RFC 4122 — version=4 (bytes[6]) + variant=10x (bytes[8])`.
- **Pénalité** : −5

✅ Documentation exemplaire :
- Tous les nouveaux headers (`range_parser.hpp`, `peers_history.hpp`, `transfer_history.hpp`) ont un docblock complet : rôle, format JSON, rétention, exemples.
- Chaque hook V1.6.5 dans `app_controller.cpp` est marqué `// V1.6.5 — Wave X item Y :` avec WHY.
- `web_session_store.cpp:227` : commentaire RFC 2104 pour HMAC manuel (bonne pratique).
- `download_routes.cpp:116-122` : explique le bug 0% corrigé (item A) et POURQUOI le `TransferProgressEvent{0}` doit être avant `set_content_provider`.
- `pin_storage.js:8-16` : sécurité expliquée — chiffrement AES-GCM, dérivation HKDF, comportement si cert change.

**Score : 95/100**

---

### 2.5 SOLID — 80/100

#### ⚠️ [Majeur] OCP — `if/switch` sur le type de signal P2P

- **Fichier** : `src/web/routes/p2p_routes.cpp:19-23`
  ```cpp
  bool isValidSignalType(const std::string& t) {
      return t == "offer" || t == "answer" || t == "ice"
          || t == "refuse" || t == "cancel" || t == "bye"
          || t == "ice-restart" || t == "ice-restart-answer";
  }
  ```
  Ajouter un type = modifier cette fonction (closed for extension).
- **Correction proposée** : `static const std::unordered_set<std::string> kValidSignalTypes = {...};` + `return kValidSignalTypes.count(t) > 0;`. Plus simple à étendre sans toucher la fonction.
- **Pénalité** : −10

#### ⚠️ [Majeur] SRP — `WebService` accumule les responsabilités

- **Fichier** : `include/ltr/web/web_service.hpp:32-156`
- **Mesure** : 11 méthodes publiques + 4 sous-systèmes possédés (`HttpServer`, `WebSessionStore`, `DownloadTicketStore`, `SseBroadcaster`, `WebUploadAnnounceStore`, `httpsServer_`).
- **Détail** : la classe est aux limites du « God Object » (>10 méthodes, 154 lignes). Cancellation flag map + zip-and-announce + HTTPS lifecycle + keepalive loop dans une même classe.
- **Note** : c'est un état préexistant V1.6.4, pas une régression V1.6.5. Le sprint a ajouté juste 1 méthode (`releaseCancelFlag`) + 1 ligne dans `start()`.
- **Correction proposée** : extraire `CancelFlagRegistry` (la map + les 3 méthodes acquire/release/cancel) en classe dédiée.
- **Pénalité** : −10

✅ Bons points :
- `PeersHistory` et `TransferHistory` ont chacune une responsabilité claire (1 fichier JSON × 1 entité).
- `RangeInfo` + `parseRangeHeader` : pure function, isolée, testable. Excellent découpage SRP.
- DI via constructeur dans `AppController` (pas de singletons / new dans la logique métier).
- Pas de violation Liskov, pas d'interfaces > 5 méthodes.

**Score : 80/100**

---

### 2.6 Dette technique — 65/100

#### ⚠️ [Majeur] Code mort — `handle401` / `handle401Async` / `tryRefreshSession`

- **Fichier** : `assets/web/js/common.js:33-67`
- **Mesure** : 3 fonctions exportées dans `window.LTR`, **aucune** ne possède de call site dans le reste du code JS du sprint (vérifié via `grep`).
- **Détail** : la primitive de silent-refresh `/api/auth/refresh` (item H, partie client) est implémentée mais jamais branchée à `upload.js`, `p2p_transport.js`, etc. Résultat : un 401 dans ces flows redirige toujours brutalement vers `/login` sans tenter le refresh, alors que le cookie `ltr_remember` permettrait de récupérer transparemment.
- **Impact** : cassure de l'UX promise par item H : « recréation silencieuse de session ». Le cookie est émis et le endpoint serveur fonctionne, mais le client ne le consomme pas pour les requêtes API.
- **Correction proposée** : remplacer chaque `if (res.status === 401) goToLogin()` (ex: `p2p_transport.js:129-130`) par `await handle401Async(res)` + branche `'retry'` qui rejoue la requête initiale.
- **Pénalité** : −10

#### ⚠️ [Majeur] `console.log` debug en production (paste flow + p2p_session)

- **Fichier** : `assets/web/js/upload.js:40,44,52,76,80,86,163,173,202-226` et `p2p_session.js:30,283,313,321,351,412,417,429`
- **Mesure** : ~25 `console.log` actifs dans des chemins de prod (paste, P2P session).
- **Détail** : pré-existants V1.4 / V1.6.3 — pas une régression V1.6.5 mais le sprint Stabilité avait l'occasion de les nettoyer ou de les router vers `clientLog` (qui les envoie au backend de façon contrôlée).
- **Correction proposée** : `find/replace` `console.log(` → `clientLog('debug',` (voire suppression simple pour les paths verbeux de paste).
- **Pénalité** : −10

#### ⚠️ [Majeur] `setTimeout(..., 5000)` pour reset `refreshInFlight`

- **Fichier** : `assets/web/js/common.js:47`
  ```js
  setTimeout(() => { refreshInFlight = null; }, 5000);
  ```
- **Détail** : le reset asynchrone évite de coalescer indéfiniment, mais 5000 ms est arbitraire (pas de constante nommée, pas de WHY documenté). Si le serveur est lent à répondre `auth/refresh`, deux refresh peuvent partir en parallèle, ce qui annule la dédup.
- **Correction proposée** : déplacer le reset à l'intérieur du `await` (après `r.ok` connu) plutôt qu'en `setTimeout`. Ou nommer `REFRESH_COOLDOWN_MS = 5000` + commenter.
- **Pénalité** : −10

#### ℹ️ [Mineur] Catch swallow silencieux multiples en JS

- **Fichier** : `p2p_session.js`, `transfer_registry.js`, `p2p.js` — pattern `} catch {}` ou `} catch (e) {}` sans log : ~12 occurrences (cf. grep).
- **Détail** : majoritairement justifiés (close()/cancel() sur ressources déjà closed, vibrate() sur browsers sans support). Mais `p2p.js:233` (`} catch {}`) sur `trustP2pPeer` pourrait masquer une corruption IndexedDB sans alerte.
- **Correction proposée** : ajouter `clientLog('warn', '[tofu-p2p] trustP2pPeer failed')` dans le catch de `trustP2pPeer`.
- **Pénalité** : −5

✅ Bons points :
- Aucun `TODO` / `FIXME` / `HACK` dans le code C++ V1.6.5.
- Tous les `catch (...)` C++ sont commentés ou répondent par un statut HTTP explicite.
- Pas de `new`/`delete` manuel — RAII partout.
- Les écritures atomiques (`tmp + rename`) sont correctement appliquées dans `peers_history.cpp:191-196` et `transfer_history.cpp:209-214`.

**Score : 65/100**

---

### 2.7 Sécurité — 70/100

#### ⚠️ [Majeur] HMAC verify : comparaison non constant-time

- **Fichier** : `src/web/web_session_store.cpp:316`
  ```cpp
  if (expectedSig != sig) return std::nullopt;
  ```
- **Détail** : `std::string::operator!=` short-circuite au 1er char différent → vulnérable timing attack théorique pour deviner la signature. En LAN trusted c'est marginal, mais pour un cookie persistent 30j sur HTTPS public, ça mérite d'être renforcé.
- **Correction proposée** : helper `bool constantTimeEquals(const std::string& a, const std::string& b)` (XOR all bytes, return result == 0) — 5 lignes.
- **Pénalité** : −10

#### ⚠️ [Majeur] PIN remember = placeholder (item I)

- **Fichier** : `assets/web/js/pin_storage.js`
- **Détail** : la clé AES-GCM est dérivée de `certFingerprint` qui est **publiquement exposé** via `/api/cert-info`. Un attaquant XSS ou un voleur de device peut récupérer le fingerprint via `fetch('/api/cert-info')` puis dériver la clé HKDF identique. Le seul gain réel est de **lier le PIN chiffré au cert HTTPS** (régénération cert → PIN illisible) — c'est utile contre un device cloné où le cert n'est plus valide.
- **Vérification** : le code source (`pin_storage.js:9-16`) **documente correctement** la limite : « WebCrypto disponible UNIQUEMENT en secure context, en HTTP plain savePin() rejette ». ✅
- **Manque** : la documentation NE mentionne PAS explicitement que **le secret n'est pas réellement secret** (cert public). La phrase devrait être ajoutée : « V1 : placeholder. La clé est dérivée d'un secret public — un attaquant ayant accès au localStorage peut déchiffrer. À durcir avec WebAuthn pour V2. »
- **Correction proposée** : ajouter ce commentaire en haut de `pin_storage.js`.
- **Pénalité** : −10

#### ℹ️ [Mineur] `pinHash8` exposé dans le token

- **Fichier** : `src/web/web_session_store.cpp:222-225`
- **Détail** : `pinHash8 = SHA-256(pin).substr(0,8)` = 32 bits = 4×10⁹ valeurs. Pour un PIN à 6 chiffres (10⁶ valeurs possibles), un attaquant qui intercepte le token peut bruteforcer offline en quelques secondes (10⁶ SHA-256). En LAN, le token transite sur HTTP plain (pas HTTPS sur la 1re connexion) → interception triviale par un voisin Wi-Fi.
- **Atténuation existante** : `pinHash8` n'est PAS le PIN — bruteforce → PIN trouvé, mais sans le secret HMAC (= fingerprint cert) le token est invalide. Et le PIN change à chaque restart du host.
- **Vérification logs** : `auth_routes.cpp:142-143` log `device_id.substr(0,8) + token.substr(0,8)` mais **PAS** `pinHash8` ni le PIN. ✅ Pas d'exposition logs.
- **Correction proposée** : tronquer à 4 chars hex (16 bits) pour réduire l'info exposée — assez pour détecter changement de PIN, trop court pour bruteforce ciblé. Ou hasher avec un salt fixe stocké dans le hmacSecret.
- **Pénalité** : −5

#### ℹ️ [Mineur] Regex DTLS fingerprint suffisamment stricte mais ANCRÉE seulement à gauche

- **Fichier** : `assets/web/js/p2p.js:181`
  ```js
  const m = ln.match(/^a=fingerprint:sha-256\s+([0-9A-Fa-f:]+)/i);
  ```
- **Détail** : pas d'ancre fin de ligne, donc la classe `[0-9A-Fa-f:]+` capture greedy jusqu'au prochain caractère hors-classe. Pour un SDP standard c'est OK. Pour un SDP malformé contenant un suffixe légitime (pas vu en pratique), le fingerprint pourrait être pollué — mais la classe est restrictive (que hex+colon), donc le risque est limité.
- **Correction proposée** : ajouter `[\s$]` à la fin pour matcher whitespace ou EOL : `/^a=fingerprint:sha-256\s+([0-9A-Fa-f:]+)(?:\s|$)/i`.
- **Pénalité** : −5

✅ Excellents points :
- Tokens HMAC bien construits, exp Unix, vérif PIN actuel + secret + format. 9 cas couverts par `test_persistent_token.cpp`.
- Cookie `ltr_remember` : `HttpOnly` + `SameSite=Lax` + `Max-Age=2592000` (30j).
- Logout efface les 2 cookies. ✅
- `IndexedDB` TOFU P2P ne stocke pas de secrets, juste des fingerprints publics (lookup-only).
- Auto-purge `pin_storage.loadPin` en cas d'échec déchiffrement (cert change) → pas de fuite.
- HMAC `hmacSha256Hex` : implémentation RFC 2104 correcte (ipad/opad, blockSize 64, hash si key > 64).

**Score : 70/100**

---

## 3. Synthèse & Recommandations

### Top 3 — Corrections prioritaires (avant prochain sprint)

1. **Refactor `streamFile` / `streamZip`** (download_routes.cpp) : extraire `applyRange`, `emitProgressIfDue`, `terminateProvider`. Réduit la complexité de 146→~50 lignes par fonction et supprime la duplication progress-throttle. Coût ~1h.
2. **Brancher `handle401Async`** : remplacer les `if (res.status === 401) goToLogin()` épars par `await handle401Async(res)` dans `upload.js`, `p2p_transport.js`. Coût ~30 min. **Sans ça, l'item H Wave 3 est à moitié livré côté client**.
3. **Constant-time HMAC compare** : 5 lignes dans `web_session_store.cpp` pour combler la vuln timing-attack théorique. Coût 10 min.

### Top 2 — Améliorations sécurité V2

4. **Documenter le placeholder PIN remember** dans `pin_storage.js` : ajouter le warning « clé dérivée d'un secret public, à durcir avec WebAuthn pour V2 » en tête de fichier.
5. **WebAuthn pour PIN remember** : remplacer HKDF(certFingerprint) par une clé dérivée de l'authenticator local. Sprint dédié V2.

### Bons points à conserver

- Tests : 21/21 pass, 4 nouveaux tests V1.6.5 robustes (37 assertions cumulées sur range_parser + persistent_token + peers_history + transfer_history).
- Nouveaux modules `PeersHistory` / `TransferHistory` : SRP propre, écritures atomiques, mutex-protégés, tests roundtrip JSON.
- Documentation des hooks V1.6.5 dans `app_controller.cpp` : chaque ajout est commenté avec WHY.
- Range Requests RFC 7233 (item D) : implémenté, testé, exhaustivement géré côté `streamFile` (status 206 + `Content-Range` + `streamStart` seek).
- Symétrie iceRestart sender ↔ receiver (item G) : `tryIceRestart`, `handleIceRestartIncoming`, `handleIceRestartAnswer` bien découplées, timer `iceRestartTimer` correctement annulé sur `connected` (p2p_transport.js:99-103).
- TOFU P2P (item L) : extraction SDP stricte (regex anchored, classe restrictive), toast bloquant via Promise (`p2p.js:277-287`), fallback `confirm()` natif si `showTofuToast` indisponible.
- `purgeOlderThan` (PeersHistory) appelle bien `saveLocked()` après purge (vérif point 3 du brief audit). ✅
- `releaseCancelFlag` invoqué dans **chaque path** de termination de `streamFile` (3 paths : cancel, EOF, write fail) et `streamZip` (4 paths : cancel, errored, EOF, plus une à 235). ✅ exhaustivité confirmée.

---

## 4. Validation

**Score global : 80/100 — ✅ VALIDÉ AVEC RÉSERVES**

Le sprint V1.6.5 peut passer en phase Documentation. Les 3 items prioritaires ci-dessus doivent figurer dans le backlog du sprint suivant (V1.6.6 — finalisation refresh silencieux + refactor streamFile/streamZip + résume avec offset serveur-side).

Aucun blocage majeur ; la branche est mergeable.
