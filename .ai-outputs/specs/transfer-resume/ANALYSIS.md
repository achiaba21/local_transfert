# Transfer Resume — Robustesse réseau + envois simultanés + reprise

> Document de suivi / référence pour le sprint de robustesse du flow
> TCP LTR1. À relire en début de session.

**Créé :** 2026-04-24
**Scope :** couche `ltr::network` (TransferClient, TransferServer, protocol),
couche `ltr::app` (AppController, AppState), couche `ltr::ui` (MainScreen
zone TRANSFERTS)
**État :** décisions validées, prêt pour `/feature full` Sprint 1.

---

## 📊 Phases

| Phase | Titre | Inclus V1 | Effort |
|-------|-------|-----------|--------|
| 1 | Resume MVP (sidecar + ResumeOffer + bouton Reprendre) | ✅ | 5-8 j |
| 2 | Classification erreurs + auto-retry configurable | ✅ | 3-5 j |
| 3 | Heartbeat Ping/Pong + timeouts applicatifs | ✅ | 2-3 j |
| 4 | Persistance sender cross-restart | ✅ | 3-5 j |
| UI | Bouton « Reprendre tout » global + par card | ✅ | 1-2 j |

**Sprint 1 livrable = Phases 1 + 2 + 3 + 4 + UI globale = ~14-23 j**

---

## 🚚 Livré — Wave 1+2+3 MVP (2026-04-24)

**Wave 1 — Foundation ✅**
- `include/ltr/infra/resume_sidecar.hpp` + `.cpp` : I/O JSON atomique
  (sidecar receiver + pending-sessions sender)
- `core::ErrorCategory` enum (Unknown/Network/Protocol/Permanent/Cancelled)
- `TransferFailedEvent` gagne `.category`
- 4 nouveaux `MessageType` (ResumeOffer 0x0A, ResumeResponse 0x0B, Ping
  0x0C, Pong 0x0D) — codes réservés, handlers complets en Wave 3+
- `infra::Config` : `autoRetryCount` (default 2) + `resumeSidecarTtlHours`
  (default 24)
- `test_resume_sidecar` : 7 cas testés (roundtrip, corrompu, purge TTL,
  pending-sessions roundtrip, etc.) — 9/9 tests OK

**Wave 2 — Backend réseau ✅ (MVP)**
- `TransferServer` :
  - Constructor prend `resumeSidecarTtlHours`
  - Purge sidecars expirés au `start()`
  - Écrit le sidecar global au début de session
  - Met à jour le sidecar à chaque `FileEnd` (status Done) + rename .part → final
  - Sur erreur réseau : conserve `.part` + met à jour sidecar Partial
  - Sur cancel explicite : supprime `.part` + sidecar
  - Sur Done global : supprime sidecar
- `TransferClient` :
  - Nouvelle méthode `resumeSession(sid, peer, files, pin)` — MVP
    redémarre avec le MÊME sessionId (le serveur écrase le sidecar)
  - Classification `Network`/`Cancelled` sur les erreurs critiques
- `AppController` :
  - `resumeTransfer(sid)` : retrouve peer + sourcePaths/pinCode du
    `UiTransfer`, relance via `resumeSession`
  - `resumeAllTransfers()` : séquentiel 1 par 1 (Q1=A)
  - `ignoreTransfer(sid)` : passif, retire la card
  - `UiTransfer` gagne `sourcePaths`, `peerId`, `pinCode`, `resumable`,
    `lastErrorCategory`
  - Routing `ErrorCategory` dans `onEvent TransferFailedEvent`

**Wave 3 — UI MVP ✅**
- Card Failed resumable : 2 boutons stackés « Reprendre » + « Ignorer »
- Header TRANSFERTS : bouton global « Reprendre tout (N) » si ≥1
  resumable
- Clics wire vers `resumeTransfer` / `resumeAllTransfers` / `ignoreTransfer`
- Build Release propre, 9/9 tests, app démarre

---

## ⏳ Non livré (reste à faire — Wave 3b / Sprint Transfer Resume V2)

Par ordre d'importance :

1. **Négociation skipBytes RÉELLE** via `ResumeOffer` / `ResumeResponse` :
   actuellement le MVP redémarre from 0 (le serveur écrase son sidecar).
   Le gain vrai du resume (ne pas retransmettre les bytes déjà OK) n'est
   PAS encore réalisé. C'est le livrable critique restant.

2. **Heartbeat Ping/Pong** applicatif (timeout 20 s) : refactor non-blocking
   `sf::SocketSelector` dans `runSender` + `sessionWorker`. Wi-Fi bloqué
   = TCP timeout OS (30-120 s) aujourd'hui.

3. **Auto-retry silencieux** 2× sur Network avec backoff 1s/4s : logique
   dans `TransferClient::runSender`. UI "Reconnexion 1/2…" non implémenté.

4. **Cross-restart** (`pending-sessions.json` load/save au
   startup/failure) : infra côté `resume_sidecar.hpp` déjà prête
   (`loadPendingSessions` / `savePendingSessions`), mais pas câblée dans
   `AppController`.

5. **Banner startup** « N transferts en attente » : UI non implémentée.

6. **Icône refresh.png** : pas générée (le bouton « Reprendre » affiche
   juste le texte pour V1).

7. **Tests supplémentaires** : resume source modifié, peer absent au
   resume, Wi-Fi cut mid-send — tests manuels uniquement V1.

8. **Doc `docs-agents/NETWORK.md`** : pas créée.

---

## 1. État actuel (audit code)

### ✅ Envois simultanés — déjà supporté

- `TransferClient::sendFiles` spawn un thread par appel (1 thread par
  destinataire)
- `AppController::requestSend()` boucle sur `selectedPeerIds` → N threads
  parallèles
- `TransferServer::acceptLoop` accepte chaque connexion entrante dans un
  worker thread dédié

### ❌ Resume après erreur — non supporté

- `TransferServer::sessionWorker` sur échec : `remove(tmp .part)` →
  données partielles perdues
- `TransferClient::runSender` : thread exit sur erreur, aucun état gardé
- UI : Failed avec auto-clean 30 s, aucune reprise possible

### ❌ Gestion erreurs transitoires — aucune

- Pas de retry auto sur read/write error
- Pas de keepalive applicatif → Wi-Fi bloqué 10 s = TCP timeout OS
  (30-120 s)
- Pas de différentiation Network vs Permanent

---

## 2. Problèmes concrets adressés

| # | Problème | Solution |
|---|----------|----------|
| 1 | Wi-Fi blip 2-5 s sur un envoi 5 Go | Auto-retry + sidecar + resume |
| 2 | Envoi vers 3 peers, 2 échouent | Reprise par card ou globale |
| 3 | App sender crash au milieu | Persistance disk côté sender |
| 4 | Target device fermé pendant l'envoi | Sidecar conservé 1 jour, retry quand target revient |
| 5 | Disk full côté receveur | Category Permanent, pas de resume |
| 6 | Source file modifié entre échec et resume | sha256Prefix check → restart silencieux |
| 7 | N sessions en parallèle vers même peer | sessionId UUID → isolation |

---

## 3. Décisions produit validées (8 questions)

| # | Question | Décision |
|---|----------|----------|
| 1 | TTL sidecars `.ltr-resume.json` | **1 jour** (au-delà, purge auto au démarrage server) |
| 2 | Nombre d'auto-retries silencieux | **Configurable** via `infra::Config::autoRetryCount` (default 2) |
| 3 | Resume cross-restart (sender) | **Inclus V1** (persistance disk `pending-sessions.json`) |
| 4 | Bouton « Ignorer » action sur receiver | **Passif** (laisser expirer, pas de RPC cleanup) |
| 5 | Multi-file resume dans une session | **Oui** (resume repart au bon fichier + bon offset) |
| 6 | Source modifié entre fail et resume | **Restart silencieux** (log mais pas bloquer) |
| 7 | Heartbeat Ping/Pong | **Phase 1** (inclus dès le sprint 1) |
| 8 | UI | **« Reprendre tout » global + bouton par card** |

---

## 4. Design — aperçu

### 4.1 Côté RECEIVER — sidecar

```
~/Downloads/LocalTransfer/MyFolder/sub/photo.jpg.part
~/Downloads/LocalTransfer/MyFolder/sub/photo.jpg.ltr-resume.json
```

`.ltr-resume.json` :
```json
{
  "sessionId": "abc123...",
  "senderDeviceId": "uuid-sender",
  "createdAt": "2026-04-24T10:00:00Z",
  "lastUpdateAt": "2026-04-24T10:05:30Z",
  "expectedSize": 5242880000,
  "bytesReceived": 3221225472,
  "sha256Prefix": "<hash des 4 Ko début du source>",
  "relativePath": "MyFolder/sub/photo.jpg"
}
```

Purge sidecars > 1 jour au démarrage `TransferServer::start`.

### 4.2 Côté SENDER — persistance disk (cross-restart)

`pending-sessions.json` dans le config dir :

```json
[
  {
    "sessionId": "abc123",
    "peerId": "uuid-peer",
    "peerName": "MacBook Pro",
    "peerIp": "192.168.1.3",
    "peerTcpPort": 45455,
    "pinCode": "472931",
    "sourcePaths": ["/Users/me/MyFolder", "/Users/me/photo.jpg"],
    "totalBytes": 5242880000,
    "bytesTransferred": 3221225472,
    "retryAttempts": 0,
    "lastErrorCategory": "network",
    "createdAt": "2026-04-24T10:00:00Z"
  }
]
```

Écrit à chaque fail (catégorie Network ou Protocol). Lu au démarrage
de `AppController::start` → restore dans `state.transfers` avec status
Failed+resumable+crossRestart. UI affiche « 3 transferts en attente,
[Reprendre tout] ».

### 4.3 Protocol LTR1 — extensions

Nouveaux message types :

| Code | Type | Sens | Payload |
|------|------|------|---------|
| `0x0A` | `ResumeOffer` | C→S | JSON `Offer` + `"resume": true` + `sha256Prefix` par fichier |
| `0x0B` | `ResumeResponse` | S→C | JSON : `files[{relPath, action: "continue"|"restart"|"skip", skipBytes}]` |
| `0x0C` | `Ping` | bidir | `{ts}` heartbeat toutes les 10 s pendant streaming |
| `0x0D` | `Pong` | bidir | `{ts}` ack pong |

Backward compat : si server LTR1 legacy reçoit `ResumeOffer` → `Reject`
« protocole_non_supporté » → client fallback sur `Offer` classique
(restart complet).

### 4.4 Classification erreurs

```cpp
enum class ErrorCategory {
    Network,      // TCP disconnect, timeout, ping-pong miss → retry + resume OK
    Protocol,     // frame invalide, crc mismatch → retry prudent
    Permanent,    // disk full, perm denied → pas de retry, pas de resume
    Cancelled,    // user click → pas de resume
};
```

Politique :

| Catégorie | Auto-retry silencieux | Resumable | UI Failed card |
|-----------|-----------------------|-----------|---------------|
| Network | `autoRetryCount`× (backoff 1s, 4s, ...) | ✅ | [Reprendre] [Ignorer] |
| Protocol | 1× | ✅ | [Reprendre] [Ignorer] |
| Permanent | — | ❌ | [Ignorer] + texte raison |
| Cancelled | — | ❌ | [Ignorer] |

### 4.5 Heartbeat Ping/Pong

- Pendant streaming (entre FileChunk), chaque côté envoie un `Ping`
  toutes les 10 s
- Timeout 20 s sans Pong reçu → session morte → `TransferFailedEvent
  {category:"network"}` → sidecar sauvegardé → resume possible

### 4.6 UI extensions

```cpp
struct UiTransfer {
    // ... existant ...
    // V1.1.9 - Sprint Transfer Resume
    std::vector<std::filesystem::path> sourcePaths;
    std::string   peerId;
    std::string   pinCode;
    bool          resumable{false};
    int           retryAttempts{0};
    int           maxRetries{0};     // copié depuis cfg au fail
    std::string   lastErrorCategory; // "network" / "protocol" / "permanent" / "cancelled"
    bool          crossRestart{false}; // chargé depuis pending-sessions.json
};
```

**Card Failed resumable :**
```
┌────────────────────────────────────────────────────┐
│ ↑  MacBook Pro          [ Reprendre ]  [Ignorer]   │
│    ✗ Coupure réseau · 3.2 Go / 5 Go (64 %)         │
│ ███████████████████████████░░░░░░░░░░░░░           │
└────────────────────────────────────────────────────┘
```

**Header zone TRANSFERTS avec cards resumables :**
```
TRANSFERTS · 3      [Reprendre tout]      [◀] [▶]
```

Bouton « Reprendre tout » : visible si ≥1 card resumable. Clic → resume
sur toutes séquentiellement (ou en parallèle ? V1 = séquentiel pour
éviter surcharge).

---

## 5. Config — nouveaux champs

```cpp
struct infra::Config {
    // ... existant ...
    int autoRetryCount{2};          // 0 = désactive retry silencieux
    int resumeSidecarTtlHours{24};  // purge au démarrage
};
```

JSON :
```json
{
  "autoRetryCount": 2,
  "resumeSidecarTtlHours": 24
}
```

Pas d'UI Settings pour ces champs en V1 — édit manuel config.json ou
valeurs par défaut.

---

## 6. Edge cases

| Cas | Détection | Comportement |
|-----|-----------|-------------|
| Source file modifié entre fail & resume | `sha256Prefix` mismatch | `restart` silencieux (log warn) |
| Receiver a supprimé `.part` manuellement | Sidecar OK, fichier absent | `restart` |
| Sidecar corrompu | parse fail | Delete + `restart` |
| Sidecar >24h | timestamp | Delete au startup + `restart` lors du resume |
| App sender crash | `pending-sessions.json` | Restore au startup → UI « 3 en attente » |
| Peer target fermé au resume | connect refused | Card reste Failed, user réessaye plus tard |
| Reçu 2× même fichier (1 OK, 1 resume) | `uniqueTargetPath` ajoute `" (1)"` | Pas de conflit |
| Disk full pendant resume | `ofs.write` fail | `Permanent` → Failed non-resumable |

---

## 7. Ordre d'implémentation intra-sprint

1. **Jour 1-3** : Sidecar côté receiver + persistance sender disk
2. **Jour 4-6** : Protocol extensions (ResumeOffer, ResumeResponse,
   Ping, Pong)
3. **Jour 7-9** : TransferClient::resumeSession + backend logic
4. **Jour 10-11** : Classification ErrorCategory + auto-retry silencieux
5. **Jour 12-13** : Heartbeat applicatif
6. **Jour 14-15** : UI boutons Reprendre par card + Reprendre tout
   global
7. **Jour 16-18** : Tests unitaires intensifs + smoke tests réseau réels
   (Wi-Fi coupé, app kill mid-send)
8. **Jour 19-20** : Bug bash + documentation

---

## 8. Risques

| Risque | Probabilité | Impact | Atténuation |
|--------|-------------|--------|-------------|
| Sidecar corrompu → corruption fichier | Faible | Très haut | Verify SHA256 avant de continuer, delete et restart si douteux |
| Resume infini en boucle (bug retry count) | Moyenne | Moyen | Cap hard à 5 tentatives max, après → non-resumable |
| Peer accepte le 1er Offer mais reject au ResumeOffer | Moyenne | Moyen | Fallback propre : offrir à l'user de restart from scratch |
| pending-sessions.json corrompu | Faible | Moyen | Try/catch load, backup.bak, warn puis continuer |
| Clock drift (sidecar timestamp invalide) | Basse | Faible | Utiliser `steady_clock` en interne + `system_clock` pour JSON, compare tolérant |
| Heartbeat faux-positifs (GC pause, fsync long) | Moyenne | Faible | Timeout 20 s (large) + log le cas pour stats |

---

## 9. Livrables du Sprint 1

- [ ] Receiver : conserve `.part` sur fail + écrit sidecar `.ltr-resume.json`
- [ ] Receiver : purge sidecars >24h au démarrage `TransferServer::start`
- [ ] Sender : persistance `pending-sessions.json` au fail + load au
  démarrage
- [ ] Protocol : `ResumeOffer`, `ResumeResponse`, `Ping`, `Pong` (nouveaux
  MessageType codes)
- [ ] `TransferClient::resumeSession(sessionId)` + backend negotiation
- [ ] `ErrorCategory` enum + propagation dans `TransferFailedEvent`
- [ ] Auto-retry silencieux `autoRetryCount`× sur Network avec backoff
- [ ] Heartbeat applicatif Ping/Pong (timeout 20 s)
- [ ] UI : bouton « Reprendre » par card Failed resumable + bouton
  « Reprendre tout » global dans le header TRANSFERTS
- [ ] UI : sous-statut « Reconnexion… » pendant auto-retry
- [ ] Config : champs `autoRetryCount` + `resumeSidecarTtlHours`
- [ ] Tests : simuler Wi-Fi cut, app kill mid-send, source modifié
- [ ] Aucune régression : les 8 tests existants passent, protocole LTR1
  legacy reste fonctionnel (fallback Offer classique)

---

## Journal

### 2026-04-24
- Analyse initiale livrée
- 8 décisions produit validées
- Document créé
- Sprint 1 = Phases 1+2+3+4+UI combinées (~14-23 j)
- Prêt pour `/feature full`
