# WEB.md — Couche Web de LocalTransfer

> Guide central pour tout agent / session Claude qui travaille sur la
> couche `ltr::web`. Lire avant toute modification.

## 🆕 V1.5 — Sprint Hardening & Polish (partiel, 2026-05-03)

Sprint multi-axes initialement prévu pour 14 items / 4 vagues.
Livré : 5 items à risque faible (Wave 1 et Wave 2 partielles).
Reste reporté à V1.6 (Wave 3 perf P2P + Wave 4 sécurité, avec
chacune leur propre sprint dédié).

### Livré

- **`ltr::core::encodePng`** (`include/ltr/core/png_encoder.hpp`) :
  encodeur PNG minimal réutilisable extrait de
  `clipboard_paste_win.cpp`. IHDR + IDAT zlib via miniz + IEND.
  ~80 LOC supprimées du fichier Windows.
- **`ltr::core::log_event`** : log structuré avec champs k=v.
  Toggle `LogFormat::Text` (défaut, compat) ou `Json`. Cap 200
  chars/value pour éviter inflation. Préparation pour pipelines
  observabilité externes.
- **`skipFailedFile()`** dans `p2p.js` : helper qui factorise les
  4 patterns « failed → skip → continue » du multi-fichier.
- **Aperçu image web** (`upload.js`) : thumbnail 40×40 dans la
  staging area si MIME image/*. URL.revokeObjectURL au cleanup.
- **Auto-fill PIN URL** (`login.js`) : si `?pin=XXXXXX` dans
  l'URL, pré-remplit les 6 cases. Pas d'auto-submit.

### Reporté V1.6

| Item | Raison report |
|---|---|
| 1.1 Split p2p.js (transport/session/ui) | Refactor à risque, bénéfice esthétique |
| 2.1 desktop preview image | Cache LRU sf::Texture nécessaire |
| 2.2 Notif natives Mac+Win | Mérite sprint dédié multi-OS |
| 2.3bis Toggle UI QR avec PIN | UI SharePanel à designer |
| Wave 3 entière (OPFS, multi-stream, resume P2P, re-négo) | Sprint dédié perf |
| Wave 4 entière (HTTPS LAN, TOFU TCP) | Sprint dédié sécurité (openssl + protocole TCP) |

### Tests V1.5 : 16/16 passent

- `test_png_encoder` (NOUVEAU) : signature PNG, chunks IHDR/IDAT/IEND,
  cas limites taille=0 et rgba trop petit
- 15 tests V1.4 inchangés

---

## 🆕 V1.4 — Sprint Clipboard Paste (2026-05-02)

Permet d'envoyer le contenu du presse-papier (texte, image PNG,
fichiers/dossiers) en un clic ou via raccourci Cmd+V / Ctrl+V.

### API uniforme

`include/ltr/ui/clipboard_paste.hpp` :
```cpp
struct ClipboardPaste {
    enum class Kind { None, Text, Image, Files };
    Kind kind = Kind::None;
    std::string text;
    std::vector<std::uint8_t> imageBytes;
    std::string imageExt;        // "png"
    std::vector<std::filesystem::path> files;
};
ClipboardPaste readClipboard();
```

### Implémentations natives (compile-time)

Pattern `LTR_CLIPBOARD_SRC` identique à `LTR_DRAG_DROP_SRC` :

- **macOS** (`clipboard_paste_mac.mm`) : NSPasteboard.
  - NSPasteboardTypeFileURL → vector<path>
  - NSPasteboardTypePNG direct, ou TIFF→PNG via NSBitmapImageRep
  - NSPasteboardTypeString → UTF-8
- **Windows** (`clipboard_paste_win.cpp`) : OpenClipboard avec retry
  3× × 10 ms (mutex système).
  - CF_HDROP → vector<path>
  - CF_DIB → conversion BGR(A)→RGBA + encodage PNG via miniz
    (IHDR + IDAT zlib + IEND, ~80 LOC)
  - CF_UNICODETEXT → UTF-16→UTF-8
- **Linux** (`clipboard_paste_stub.cpp`) : `sf::Clipboard::getString`
  pour le texte uniquement, image/files non supportés.

Priorité de détection : **Files > Image > Text > None**.

### AppController

- `pasteFromClipboard()` : appelle `readClipboard()` et dispatch.
- `clipboardTempDir()` : `<temp>/ltr-clipboard/`. Texte/Image écrits
  dans `clipboard-YYYYMMDD-HHmmss.{txt|png}` puis `addFiles({path})`.
- Boot : `ensureClipboardTempDir` + `purgeOldClipboardTemp` (>24 h).
- Auto-clean post-`TransferDoneEvent` étendu : si `parent_path() ==
  clipboardTempDir()` → `std::filesystem::remove(p)` après envoi.

### MainScreen

- 3e entrée menu addMenu_ : "📋  Coller (⌘V)" Mac / "(Ctrl+V)" Win.
- handleEvent : `Cmd+V` (Mac, `key.system`) ou `Ctrl+V` (Win,
  `key.control`) → `controller_.pasteFromClipboard()`.
- Skip si `ipInput_.hasFocus()` (paste local au champ texte gagne).

### Web

- Bouton `#paste-btn` dans `index.html` (hidden par défaut).
- `setupPasteButton()` dans `upload.js` : check
  `navigator.clipboard.read` → show button.
- `handlePaste()` async :
  - Lit `image/png` via `it.getType('image/png')`
  - Lit `text/plain` via `navigator.clipboard.readText()`
  - Crée `new File([blob], 'clipboard-XXX.{png|txt}')`
  - Appelle `uploadFiles()` existant
- ⚠ Limite browser : navigator.clipboard NE DONNE PAS accès aux
  fichiers (sécurité). Pour des fichiers, drag-drop ou picker.
- Permission refusée → toast « Autorisation presse-papier refusée ».

### Tests V1.4

`tests/test_clipboard_stub.cpp` : invariants par Kind (None/Text/
Image/Files), pas de crash. **15/15 tests passent**.

---

## 🆕 V1.3 — Sprint Web P2P V1.3 — Robustesse + Liste UI (2026-05-02)

V1.3 durcit le P2P et ajoute une visibilité par-fichier persistante.
Tout en frontend, pas de changement backend.

### Lot 4 — Composite key `(deviceId, role)`

`Map<connKey, ConnectionState>` avec `connKey = "${deviceId}:${role}"`
remplace l'ancienne Map indexée juste par deviceId. Permet A↔B
simultané : 1 entrée sender + 1 entrée receiver pour le même peer
chez chaque navigateur. Helper `getConn(deviceId, role)` +
`cleanup(state, label)` qui prend le state directement.

### Lot 1 — Watchdogs + intégrité + disconnect transitoire

Quatre mécaniques explicites :

- **`noDataWatchdog`** — receveur, 10 s à `dc.onopen`. Si
  `bytesReceived === 0` → cleanup `'✗ Pas de données'`. Annulé au
  1er chunk reçu.
- **`ackWatchdog`** — émetteur, polling 1 s. Si `bytesSent >
  bytesAckedByReceiver` ET `now - lastAckAt > 10 s` → cleanup
  `'✗ Récepteur muet'` (silent stall détecté).
- **`connectTimer`** — 20 s pour atteindre `pc.connectionState ===
  'connected'`. Sinon `'✗ Pas de route LAN'`.
- **`disconnectTimer`** — 15 s. Si `pc.connectionState` reste à
  `disconnected` au-delà → cleanup `'✗ Wi-Fi perdu'`. Si retour à
  `connected` avant → reset.

Intégrité par fichier dans `finalizeReceivedFile` :
```js
if (cur.size && cur.received !== cur.size) {
    fs.status = 'failed'; fs.error = 'taille_invalide';
    return;  // ne télécharge PAS
}
```

Drain final côté émetteur : `while bufferedAmount > 0` borné à
`DRAIN_TIMEOUT_MS = 30_000`.

`safeSend` est devenu async et patiente pendant `disconnectedSince`
actif (pause des sends pendant flottement Wi-Fi).

### Lot 3 — Ack receveur → émetteur

Receveur : à `dc.onopen`, démarre `setInterval(500ms)` qui envoie
`{kind:'ack', bytes: state.bytesReceived}`. Émetteur reçoit via
`handleSignal('ack')` (nouveau type whitelisté côté serveur via
p2p_routes), met à jour `state.bytesAckedByReceiver` + `lastAckAt`.
`updateProgress` côté sender utilise désormais cette vraie
progression (au lieu de `bytesSent` purement local).

⚠️ Note : le type `ack` est traité côté JS, le serveur signaling
relaye sans le valider plus que les autres types whitelistés
(`offer/answer/ice/refuse/cancel/bye`). À ajouter dans la whitelist
serveur si on veut être rigoureux — actuellement passé par l'ack
du sender, mais en pratique l'ack arrive directement via le
DataChannel ouvert (PAS via /api/p2p/signal). Re-vérifier — le
DataChannel est P2P, pas relayé.

### Lot 2 — TransferRegistry + tabs Host/P2P + UI persistante

Nouveau module `assets/web/js/transfer_registry.js` (~333 LOC) :

- **entries[]** : source de vérité (id, direction, peer, name, size,
  status, bytes, error, timestamps)
- **status** ∈ `pending | sending | sent | received | failed`
- **localStorage** `ltr-p2p-history` persiste les métadonnées (cap
  100, drop FIFO). **Blobs jamais persistés.**
- **Boot** : entries laissées `pending`/`sending` par un précédent
  refresh → `failed reason='session_perdue'`
- **Retry** : `originalFiles: Map<entryId, File>` en RAM permet de
  relancer l'envoi du même File. Refresh = perte → toast invitant
  à re-sélectionner
- **Render** : tri par récence, max 10 visibles + bouton « Voir
  tout », icônes ✓ ↻ ✗ ⏱ colorées, barre 3 px en `sending`
- **notifyComplete** : toast + son WebAudio sinusoïdal 880 Hz
  120 ms fade exp + `navigator.vibrate(200)`

Tabs Host/P2P dans la footer transfers-bar : 2 pills `.tx-tab`,
switch instantané, compteurs respectifs. Ne mélange pas les flux
host↔web et web↔web.

Hooks p2p.js → Registry :
- `startSendTo` → `addEntry(direction='out')` × N + `attachFile`
  pour permettre retry
- `syncFileStatus(state, fs)` helper appelé à chaque transition,
  notifie aussi `notifyComplete` à `sent`/`received`
- `handleReceiverControl('file-meta')` → `addEntry(direction='in')`
- `finalizeReceivedFile` → status `received` ou
  `failed:taille_invalide`

### Multi-fichier : continuation après échec partiel

V1.2 cleanup global si UN fichier échoue → tout perdu. V1.3 :
chaque fichier a son `fs.status`. Si meta/chunk/end fail sur fichier
3, marque ce fichier `failed`, **continue** sur fichier 4. Plus de
cascade.

### Constantes V1.3

```js
const WATCHDOG_NO_DATA_MS = 10_000;
const NO_ACK_TIMEOUT_MS   = 10_000;
const ACK_INTERVAL_MS     = 500;
const DISCONNECT_TTL_MS   = 15_000;
const DRAIN_TIMEOUT_MS    = 30_000;
const SENDER_TTL_MS       = 90_000;  // hérité V1.2.1
```

### Tests V1.3 : 14/14 passent

(Pas de nouveau test backend, changements purement frontend. Tests
existants vérifient signaling + auth + dedup + display name.)

---

## 🆕 V1.2 — Sprint Web P2P (2026-05-01)

### Web↔web direct via WebRTC DataChannel

Plusieurs navigateurs auth sur un même host peuvent désormais :

- **Se voir mutuellement** dans une section « À envoyer à… »
  (pucks horizontaux scroll-snap, mobile-first)
- **S'envoyer des fichiers en P2P** sans que le payload transite par
  le host. Le host n'est qu'un signaling : relay des SDP offer/answer
  + ICE candidates via SSE.

### Identité visible : DisplayName

Nouveau utilitaire `ltr::web::DisplayName` (`display_name.hpp/.cpp`) :
hash FNV-1a du `device_id` → adjectif + animal FR + emoji
(ex. « 🦊 Pingouin Bleu »). 30×30 = 900 combinaisons stables. Le
`platformLabel` est dérivé du UA (« iPhone · Safari »). Les 3 champs
sont stockés dans `WebSession` à l'auth et exposés via :
- `/api/me` (la session courante elle-même)
- `web-peers` SSE (les autres sessions visibles)

### Annuaire SSE `web-peers`

Nouvel évènement nommé. Format :
```
event: web-peers
data: {"type":"web-peers","peers":[{"deviceId","displayName","emoji","platformLabel"}]}
```
Émis automatiquement après auth, logout, et après chaque éviction de
session expirée. Chaque session reçoit la liste des AUTRES (jamais
elle-même). Le frontend (`peers.js`) maintient la liste locale et render.

API store : `WebSessionStore::snapshotPeersFor(excludeToken)` →
`vector<PeerInfo>` ; `findTokenByDeviceId(deviceId)` → token actif (pour
le routing P2P).

### Signaling : `POST /api/p2p/signal`

Body JSON `{ "to": "<deviceId>", "type", "payload" }`. Types autorisés :
`offer | answer | ice | refuse | cancel | bye`. Le host :
1. Vérifie le cookie → identifie l'expéditeur
2. Refuse self-target (A→A)
3. Résout `to` via `findTokenByDeviceId`
4. Construit `event: p2p-signal\ndata: {from,type,payload}\n\n`
5. Envoie via `SseBroadcaster::send(targetToken, msg)`

Aucune persistance : pure relay. Codes : 401 / 400 / 404 / 204.

### Frontend WebRTC (`assets/web/js/p2p.js`, ~520 LOC)

Module standalone avec `Map<deviceId, ConnectionState>`. Gère N
connexions parallèles (Q7 du BA — multi-destinataires autorisé).

**Sender** :
- `RTCPeerConnection({iceServers: []})` — LAN-only V1
- `createDataChannel('ltr', {ordered: true})` + binaryType arraybuffer
- `createOffer` → `setLocalDescription` → POST `/api/p2p/signal` type=offer
- À `dc.onopen` : `file.stream().getReader()` + chunking 64 Ko
- Backpressure : attendre `bufferedAmount < 1 Mo` (event
  `bufferedamountlow`, fallback setTimeout 200 ms)
- Annonce JSON `session-meta` → `file-meta` → binary chunks → `file-end`
  → ... → `all-done` → close

**Receiver** :
- À l'`offer` reçu, modale « Accepter ? » avec TTL 60 s
- Si accepté : crée pc, `ondatachannel` → wireReceiverDc, setRemote +
  createAnswer → POST `/api/p2p/signal` type=answer
- ICE trickle des deux côtés via `onicecandidate` → postSignal('ice')
- `dc.onmessage` : si string → JSON control (`session-meta`/`file-meta`/
  `file-end`/`all-done`) ; si binaire → push chunk
- `file-end` → `Blob` + `URL.createObjectURL` + auto-download

**Cleanup** : `pc.close + dc.close + clearTimeout TTL + restore UI` au
`failed`/`refuse`/`logout`/`done`. Logout déclenche `cleanupAll()` avant
POST `/api/logout`.

### UI mobile-first

- **Section peers** : pucks 96×110 px scroll-snap horizontal, emoji
  56 px Radius::full, sub plateforme tronquée. ≥720 px : 110×124.
- **Modale réception** : bottom-sheet mobile (slide-up 220 ms
  ease-out, drag-handle visuelle), modal centré desktop (≥720 px,
  fade-in scale). TTL bar warning shrink en 60 s.
- **Sticky bar bas** pendant transfert P2P actif : « ↑ N transferts P2P
  · X % ». Cumul multi-channels.
- **Card peer en envoi** : barre de progression 3 px en bas + sub
  remplacé par « 67 % · 8 Mo/s » + bordure accent.
- **Toast non bloquant** pour notifs (refus, ICE failed).

### Fichiers ajoutés

- `include/ltr/web/display_name.hpp` + `src/web/display_name.cpp`
- `include/ltr/web/routes/p2p_routes.hpp` + `src/web/routes/p2p_routes.cpp`
- `assets/web/js/peers.js` (annuaire) + `p2p.js` (WebRTC)
- `tests/test_display_name.cpp` (5 cas)
- `tests/test_p2p_signal_routes.cpp` (6 cas : 401/400×3/404/204)

### ICE config — LAN-only V1

`{iceServers: []}` — pas de STUN public, pas de TURN. Si les 2 devices
sont sur le même Wi-Fi/LAN, les host candidates suffisent. Cross-LAN
hors scope V1 (Q5 du BA).

### Sécurité

- Le host **ne lit jamais** le payload SDP/ICE (relay opaque)
- Pas de fuite cross-session : chaque message SSE p2p-signal envoyé au
  seul destinataire
- Self-target bloqué (400)
- Whitelist 6 types — refus 400 sur types arbitraires

### Tests V1.2 : 14/14 passent
- protocol, hash, web_session_store, web_session_dedup, download_ticket,
  qr_code, http_smoke, streaming_zip, resume_sidecar, layout_box,
  breakpoint, label_ellipsis (12 V1.1)
- **display_name, p2p_signal_routes** (2 nouveaux V1.2)

---

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
