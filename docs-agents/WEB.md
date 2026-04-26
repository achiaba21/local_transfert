# WEB.md — Couche Web de LocalTransfer

> Guide central pour tout agent / session Claude qui travaille sur la
> couche `ltr::web`. Lire avant toute modification.

## 🆕 V1.1.8 — Changements notables (2026-04-24)

### ZIP streamé à la volée (host → web) — suppression du fichier temp

- **Avant V1.1.8** : un envoi de dossier déclenchait `folder_zipper::zipDirectoryToFile` qui écrivait `/tmp/ltr-zip-*.zip`. Le navigateur ne recevait rien tant que le zip disque n'était pas terminé → perception « app figée » sur dossiers lourds (plusieurs Go).
- **V1.1.8** : nouveau type `StreamingZipSource` (`include/ltr/web/streaming_zip_source.hpp`) qui assemble le zip directement dans la `DataSink` de cpp-httplib chunk par chunk. Aucun fichier intermédiaire sur disque.
- Format : **ZIP 2.0 STORE + data descriptor** (bit 3 du general purpose flag). Permet de streamer sans pré-calculer le CRC32 → pas de double lecture du fichier source.
- Content-Length **exact** calculé à l'annonce via `computeZipSize()` : `Σ(92 + 2·nameLen + fileSize) + 22` → barre de progression native côté navigateur, pas de chunked encoding.

### DownloadTicket étendu (TicketKind)

```cpp
enum class TicketKind { File, StreamingZip };

struct DownloadTicket {
    TicketKind kind;
    std::filesystem::path path;          // kind == File
    std::vector<ZipEntry> zipEntries;    // kind == StreamingZip
    // ...
};
```

`DownloadTicketStore::get()` a remplacé `consume()` — **non-destructif**.
Les tickets sont désormais **rejouables** pendant les 15 min du TTL
(utile si le visiteur annule puis reclique). Seule voie de sortie :
`evictExpired()`.

### Fichiers supprimés

- `include/ltr/web/folder_zipper.hpp`
- `src/web/folder_zipper.cpp`
- Le flag `DownloadTicket::isTemp` + la logique de cleanup associée dans `download_routes.cpp` (plus de temp à nettoyer)

### Nouveau test

- `tests/test_streaming_zip.cpp` — vérifie octet-exact la formule `computeZipSize` + valide que le zip produit est parsable par miniz avec CRC32 correct.
- 8/8 tests passent (7 existants + 1 nouveau).

### Dispatch download_routes

```cpp
if (tkt.kind == TicketKind::StreamingZip) streamZip(tkt, svc, res);
else                                       streamFile(tkt, svc, res);
```

Progress events throttlés à 100 ms / 1 MB pour ne pas flooder l'EventBus.

---

## 🆕 V1.1.7 — Changements notables (2026-04-23)

### Flow d'upload enrichi (web → host)
- Le visiteur peut maintenant envoyer un **dossier complet** via `<input type="file" webkitdirectory>` sur desktop + Android Chrome. iOS Safari ne supporte pas (bouton masqué, note dans l'UI).
- Le chemin relatif (`webkitRelativePath`) voyage dans le header `X-Relative-Path` et est sanitisé côté serveur (`sanitizeRelativePath` dans `upload_routes.cpp`) pour rejeter les tentatives de path traversal (`..`, chemin absolu, null byte).
- Le serveur fait `create_directories(targetDir / relPath.parent_path())` pour reconstruire l'arborescence.

### Zippage non-bloquant (host → web)
- **Ancienne** implémentation : `WebService::pushFiles` zippait les dossiers dans le **thread UI** → freeze de 5-30 s sur l'app desktop pendant la compression.
- **V1.1.7** : le zip est déplacé dans `WebService::zipAndAnnounce`, exécuté dans un `std::thread` **détaché**. L'UI reste 100% réactive. Le SSE `files-offer` est émis quand les tickets sont prêts.
- Mode **STORE** (`MZ_NO_COMPRESSION`) au lieu de `MZ_DEFAULT_COMPRESSION` : 5-10× plus rapide sur contenus déjà compressés (photos, vidéos, zips, PDF), taille quasi-identique.

### JS côté web refactoré en modules
- `app.js` monolithique → `common.js` (utils) + `upload.js` + `download.js` + `app.js` (bootstrap minimal).
- Namespace partagé via `window.LTR`.
- Pas de module ES (simplifie l'embed CMake et évite les subtilités CORS iOS).
- Documentation mise à jour dans `assets/web/README.md`.

### Règles de threading renforcées
1. **Travail lourd ≥ 50 ms** → toujours dans un `std::thread` détaché, jamais sur le thread UI
2. **Accès `AppState`** → uniquement depuis le thread UI (drain `EventBus` à chaque frame)
3. **EventBus::post()** est le seul moyen pour un thread worker de faire remonter un état à l'UI

---

## 🆕 V1.1 — Changements notables (2026-04-22)

Voir `.ai-outputs/specs/web-interface-v1-1/` pour les specs complètes.

### Architecture session/device repensée

| Avant V1.1 | V1.1 |
|---|---|
| `Device.id = session_token` → duplication à chaque re-auth | `Device.id = deviceId` (stable, `localStorage` navigateur) |
| Cookie non qualifié (persistent par défaut) | **Cookie de session** (meurt à la fermeture d'onglet) |
| `touch()` SSE dans le `else` uniquement → session expirait à tort | **`touch()` à chaque itération** du handler SSE |
| `keepaliveLoop` poussait un ping SSE | Plus de ping serveur-side (redondant et contre-productif) |
| Transfert desktop→web bloqué à 0 % si pas de clic | Statuts `Proposed` → `InProgress` → `Done`/`Expired`/`Cancelled` |
| Download via `<a href download>` → JSON pollué si 401 | **`fetch + blob`** + détection 401 → redirect `/login` |
| Auth + interface dans la même page | **2 pages** : `/login` + `/` (redirect 302) |
| PIN auto-submit au remplissage | Clic explicite uniquement |
| Upload input `hidden` → iOS Safari cassé | Input CSS offscreen + bouton explicite « Choisir des fichiers » |
| Fichiers desktop non-vidés post-envoi | **Cases à cocher + auto-clean** post `TransferDone` |

### Nouveaux concepts

- **`deviceId`** : UUID stable par navigateur, stocké dans `localStorage`. Transmis au serveur dans le body du `POST /api/auth`. Sert d'identité dans la liste desktop.
- **`sessionToken`** : UUID éphémère généré côté serveur à chaque auth. Stocké en cookie de session. Meurt à la fermeture de l'onglet ou au `POST /api/logout`.
- **Statuts `Proposed` / `Expired`** : (voir `include/ltr/domain/transfer_status.hpp`)
  - `Proposed` : fichier envoyé côté web, ticket créé, attend que le visiteur clique « Télécharger »
  - `Expired` : 15 minutes sans clic → ticket purgé, `TransferFailedEvent{reason="expired"}` émis

### Nouvelles routes

| Route | Rôle |
|---|---|
| `GET /login` | Page de connexion (public, pas de cookie requis) |
| `GET /login.js` | JS de la page /login |
| `GET /` | Guard : redirige vers `/login` si pas de cookie valide |
| `POST /api/logout` | Invalide la session + efface le cookie + 204 |

### Points de vigilance spécifiques V1.1

1. **Ne pas pousser de ping SSE depuis `keepaliveLoop`** — redondant avec le ping natif du handler SSE, et empêchait `touch()` de s'exécuter (le `got=true` court-circuitait le `else`).
2. **`touch()` doit être AVANT `waitAndPop`** dans la boucle SSE, pas dans le `else`.
3. **Cookie de session = pas d'attribut `Max-Age` ni `Expires`** — c'est ce qui le rend éphémère.
4. **`authenticate` requiert `deviceId`** non vide. Si le client ne peut pas le fournir (navigation privée), le serveur en génère un et le renvoie via header `X-Device-Id`.
5. **Auto-clean post TransferDone** : `AppController` retire les fichiers du `selectedFiles` au reçu de `TransferDoneEvent` pour les sessions qu'il a initiées (via `sessionPaths_`).

---



## Raison d'être

La couche web expose un **serveur HTTP plain sur le LAN** (port **45456**)
permettant :

1. À un visiteur sur le LAN (mobile / laptop / tablette, n'importe quel OS)
   d'**ouvrir une URL dans son navigateur** et d'échanger des fichiers sans
   installation.
2. Aux sessions navigateur authentifiées d'**apparaître dans la liste de
   pairs** de l'app desktop SFML, aux côtés des pairs natifs.
3. À chaque host de **servir son propre binaire** (auto-propagation) via
   `/download/self`.

Elle ne remplace pas le protocole TCP `LTR1` existant — elle coexiste.

## Architecture haute-niveau

```
┌────────────────── UI SFML (ltr_ui) ─────────────────────┐
│   AppState.peers contient Native + Web dans une liste   │
│   unifiée. SharePanel affiche QR + URL + PIN.           │
├────────────── AppController (ltr_core) ─────────────────┤
│   requestSend() dispatche selon peer.kind :             │
│     Native → TransferClient (TCP LTR1)                  │
│     Web    → WebService::pushFiles (tickets + SSE)      │
├──── network (inchangé) ──┬──── web (nouvelle couche) ───┤
│  DiscoveryService        │  HttpServer (cpp-httplib)    │
│  TransferServer          │  WebService (façade)         │
│  TransferClient          │  WebSessionStore (map mutex) │
│  Protocol LTR1           │  DownloadTicketStore         │
│                          │  SseBroadcaster (canal/sess) │
│                          │  SelfBinary (OS-specific)    │
│                          │  QrCode (wrapper qrcodegen)  │
└──────────────────────────┴──────────────────────────────┘
```

## Ports réseau

| Port  | Protocole | Usage                                      |
|-------|-----------|--------------------------------------------|
| 45454 | UDP       | Discovery beacon (inchangé)                |
| 45455 | TCP       | Protocole LTR1 natif (inchangé)            |
| 45456 | TCP       | **HTTP web** (nouveau) — fallback 45457-66 |

## Endpoints HTTP

| Méthode | Route                     | Auth    | Description |
|---------|---------------------------|---------|-------------|
| GET     | `/`                       | non     | `index.html` embarqué |
| GET     | `/app.js`, `/style.css`, `/icons/*.svg` | non | Assets embarqués |
| GET     | `/api/host-info`          | non     | Infos host (name, platform, selfDownloadUrl) |
| POST    | `/api/auth`               | PIN     | Crée une session, set cookie `ltr_token` |
| GET     | `/api/me`                 | cookie  | Whoami |
| POST    | `/api/upload`             | cookie  | Upload multipart `file` |
| GET     | `/api/events`             | cookie  | SSE long-lived (pushs host→browser) |
| GET     | `/api/download/:ticketId` | cookie  | Stream d'un fichier (consomme le ticket) |
| GET     | `/download/self`          | non     | Binaire de l'app courante |

## Flux d'authentification

```
Browser                         HttpServer / WebSessionStore
--------                        -----------------------------
GET /  ────────────────────>    index.html (embarqué)
GET /api/me  ──────────────>    401 (pas de cookie)
POST /api/auth {pin}  ─────>    vérifie vs WebService::accessPin_
                                si OK : génère token 32 hex + Set-Cookie
                                        + post PeerSeenEvent{kind=Web}
                                si KO : 401
<────── Set-Cookie ltr_token
GET /api/events  ──────────>    SseBroadcaster::attach(token)
                                boucle waitAndPop → écrit event
                                (keepalive via WebService::keepaliveLoop
                                 toutes les 2s + touch de la session)
```

## Flux upload (browser → host)

```
Browser                         Routes                       EventBus
--------                        ------                       --------
POST /api/upload                upload_routes.cpp
(multipart file)                read cookie, validate
                                write to downloadDir        <───── IncomingOfferEvent
                                                             ←──── TransferStartedEvent
                                                             ←──── TransferProgressEvent
                                                             ←──── TransferDoneEvent
                                200 {sessionId, bytes, path}
```

## Flux download (host → browser)

```
AppController::requestSend()
  peer.kind == Web
  → WebService::pushFiles(token, sid, files)
         │
         ├── issue DownloadTicket × N (store in memory)
         ├── post TransferStartedEvent(sid)
         └── broadcast SSE 'files-offer' {tickets}
                                                    ┌─ Browser reçoit SSE
                                                    │  puis pour chaque ticket:
                                                    │
GET /api/download/:ticketId  <──────────────────────┘
  download_routes.cpp
  consume ticket (atomique)
  set_content_provider → stream par chunks 256 KB
    → post TransferProgressEvent par chunk
    → post TransferDoneEvent à la fin
```

## Modèle de concurrence

- **cpp-httplib** gère son pool de threads interne (1 par connexion).
- **SSE** : la route `/api/events` reste ouverte longtemps ; chaque appel
  attache son thread au `SseBroadcaster` qui draine le `SseChannel`
  associé à la session. `waitAndPop` bloque 1s puis émet un ping
  (touch session + commentaire SSE `: keepalive`).
- **Keepalive thread** interne à `WebService` toutes les 2s :
  1. Snapshot sessions → ré-émet `PeerSeenEvent` pour chaque
  2. `evictExpired` (30s inactif) → `PeerLostEvent` + detach broadcaster
  3. `tickets.evictExpired` (5min)
- **Thread UI** draine l'`EventBus` à chaque frame — jamais d'accès
  direct SFML / AppState depuis un thread `ltr::web`.

## Points de vigilance

1. **cpp-httplib + `#ifdef CPPHTTPLIB_OPENSSL_SUPPORT`** : NE PAS définir
   ce symbole (même à 0). Le header teste `#ifdef`, pas la valeur. Un
   `-DCPPHTTPLIB_OPENSSL_SUPPORT=0` déclenche l'inclusion OpenSSL !
2. **miniz** : nécessite TOUS les `.c` du repo (miniz.c, miniz_tdef.c,
   miniz_tinfl.c, miniz_zip.c) via `file(GLOB miniz*.c)`. Le header
   inclut `miniz_export.h` qu'on stubbe nous-même (fichier généré par
   le CMake upstream qu'on n'utilise pas).
3. **Embed assets** : `reinterpret_cast` interdit en `constexpr` →
   utiliser `inline const std::string_view` pour les vues, `constexpr
   unsigned char[]` pour les tableaux.
4. **Thread-safety** : tous les stores ont un `std::mutex` interne.
   Aucun retour de référence sur des membres protégés — on retourne
   par valeur (snapshot).
5. **PIN web** : stable pour toute la session d'app (regénéré au start),
   **distinct** du PIN per-transfer natif (4 digits aléatoires). Source :
   `WebService::accessPin_`, retournée par `accessPinRef()`.
6. **Écart R3 spec métier** : la spec dit "PIN identique au natif".
   En réalité le natif = per-transfer 4 digits, le web = stable 6 digits.
   Le PIN web est affiché dans la `SharePanel` comme "code d'accès web".

## Comment ajouter une route

1. Créer `include/ltr/web/routes/xxx_routes.hpp` avec `void registerXxx(WebService&);`
2. Créer `src/web/routes/xxx_routes.cpp` qui implémente `registerXxx`.
   Utiliser `svc.httpServer().raw()` pour accéder au `httplib::Server`.
   Si l'auth est requise : lire le cookie via `readTokenCookie()`,
   valider via `svc.sessions().validate(token)`.
3. Ajouter l'appel dans `src/web/routes/route_registrar.cpp`.
4. Ajouter le `.cpp` dans `CMakeLists.txt` sous `add_library(ltr_core STATIC ...)`.

## Comment modifier un asset web

1. Éditer `assets/web/index.html`, `app.js`, `style.css`, ou `icons/*.svg`.
2. Re-builder : `cmake --build build` (CMake regénère les `.hpp`
   embarqués via custom_command, puis recompile les `.cpp` touchés).

**Pas de rechargement à chaud** — c'est le prix du binaire auto-suffisant.

## Sessions web en tant que domain::Device

Une session authentifiée devient un `domain::Device` :

```cpp
Device d;
d.id           = token;           // 32 hex chars (= sessionToken)
d.name         = "iOS (Safari)";  // parsé du User-Agent
d.platform     = "iOS";
d.kind         = PeerKind::Web;
d.sessionToken = token;
```

Ce Device est posté sur l'`EventBus` via `PeerSeenEvent` — **même flow
que le UDP beacon natif**. L'UI existante (MainScreen, DeviceListItem)
le gère sans modification. Seule la pill "Web" est ajoutée dans
`DeviceListItem::draw` quand `kind == PeerKind::Web`.

## Dispatcher côté AppController

Dans `requestSend()` :

```cpp
if (target.kind == domain::PeerKind::Web) {
    sid = makePinCode() + "-web-" + target.id.substr(0, 8);
    web_->pushFiles(target.sessionToken, sid, files);
} else {
    sid = client_->sendFiles(target, files, currentPinCode_);
}
```

C'est le **seul point de branchement** : tout le reste (UiTransfer,
events, progression) est strictement identique entre natif et web.

## Exclusions V1 (à savoir avant de proposer)

- Pas de HTTPS (LAN de confiance assumé)
- Pas de mDNS / Bonjour (QR + URL manuelle suffisent)
- Pas de persistance des sessions (redémarrage app = re-PIN)
- Pas de rate-limit sur le PIN (à noter pour V2)
- Pas de signature des binaires servis (hors scope code — Apple Developer
  ID et Windows Authenticode à faire avant release publique)
