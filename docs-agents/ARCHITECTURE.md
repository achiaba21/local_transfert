# ARCHITECTURE.md — LocalTransfer

## Couches (Clean Architecture allégée)

```
┌──────────────────────────────────────────────────────────────┐
│  UI (SFML)                                                   │
│    — ltr::ui   → widgets, screens, theme, rounded_rect       │
│                  + QrCodeView, SharePanel                    │
├──────────────────────────────────────────────────────────────┤
│  App (orchestration)                                         │
│    — ltr::app  → AppController (dispatch Native/Web), AppState│
├──────────────────────────────────────────────────────────────┤
│  Domain (entités pures)      │  Core (utilitaires)           │
│    — ltr::domain             │  — ltr::core                  │
│      Device+PeerKind, FileMeta│    EventBus, Logger,          │
│      TransferRequest,        │    types (kWebPort = 45456),  │
│      TransferSession         │    format                     │
├──────────────────────┬───────┴───────┬──────────────────────┤
│  Network             │  Web (nouveau)│  Infra                │
│    — ltr::network    │  — ltr::web   │  — ltr::infra         │
│      Protocol LTR1   │  HttpServer   │    Config, FileSystem,│
│      DiscoveryService│  WebService   │    HashService        │
│      TransferServer  │  Sessions     │                       │
│      TransferClient  │  SSE, Tickets │                       │
│                      │  SelfBinary   │                       │
│                      │  QrCode       │                       │
└──────────────────────┴───────────────┴──────────────────────┘
```

**Nouvelle couche `ltr::web`** : sœur de `ltr::network`, expose un serveur
HTTP plain sur le port **45456** (fallback 45457-45466). Les sessions
navigateur authentifiées deviennent des `Device{kind=Web}` et apparaissent
dans la même liste unifiée que les pairs natifs. Voir `WEB.md` pour les
détails (flux, endpoints, dispatcher `AppController::requestSend`).

**Règle de dépendance** : une couche ne dépend **que** de celles en dessous.

- `domain` et `core` n'ont aucune dépendance (sauf SFML::System pour
  `sf::IpAddress` dans `Device`)
- `network` et `infra` dépendent de `domain` + `core`
- `app` dépend de tout sauf `ui`
- `ui` dépend de tout

## Modèle de concurrence

```
┌──────────────────────────────────────────────────────────┐
│  Thread UI (main) — boucle SFML, OpenGL, input           │
│    ▲                                                      │
│    │  drain EventBus à chaque frame                      │
│    │                                                      │
│  ┌─┴─────────────┬──────────────┬────────────────────┐   │
│  │ Discovery     │ TransferSrv  │ TransferWorker(s)  │   │
│  │ 3 threads :   │ 1 accept     │ 1 par session :    │   │
│  │  - beacon UDP │   thread +   │   - read/write     │   │
│  │  - listener   │  N workers   │     chunks TCP     │   │
│  │  - TTL sweep  │   par sess.  │   - hash, progress │   │
│  └───────────────┴──────────────┴────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

**EventBus** (`ltr::core::EventBus`) : queue thread-safe de `std::variant`
contenant tous les types d'événements (peer vu/perdu, offre entrante,
progression, erreur...). Consommé uniquement par le thread UI.

## Flux nominal — envoi réussi

```
Émetteur A                      Destinataire B
─────────────────────           ─────────────────────
DiscoveryService ──UDP HELLO──> DiscoveryService
EventBus.post(PeerSeen)         EventBus.post(PeerSeen)

[user clique Envoyer]
TransferClient ──TCP OFFER────> TransferServer
                                (crée PendingSession,
                                 EventBus.post(IncomingOffer))

                                [user clique Accepter]
                <──TCP ACCEPT─── TransferServer

boucle fichiers :
  ──FILE_HEADER──>
  ──FILE_CHUNK──>  (× N)        écrit dans .part
  ──FILE_END────>

──TCP DONE──>                    renomme .part → final
                                 EventBus.post(TransferDone)
EventBus.post(TransferDone)
```

## Protocole réseau

### Discovery — UDP 45454 (broadcast)

Beacon JSON toutes les 2 s sur `255.255.255.255:45454` :

```json
{"proto":"LTR1","kind":"HELLO","id":"uuid-v4","name":"Mac de Serge",
 "platform":"macOS","tcpPort":45455}
```

TTL de 6 s côté listener : un pair disparu de la liste si plus de beacon.

### Transfert — TCP 45455

Framing binaire, chaque message :

```
+--------+--------+----------+---------+
| magic  | type   |  length  | payload |
| "LTR1" |  u8    |  u32 BE  |  ...    |
|  4 o   |  1 o   |   4 o    |   N o   |
+--------+--------+----------+---------+
```

Types (`ltr::network::MessageType`) :

| Code | Type | Payload | Description |
|:----:|------|---------|-------------|
| 0x01 | OFFER | JSON | `sessionId, senderName, pinCode, files[]` |
| 0x02 | ACCEPT | JSON | `sessionId` |
| 0x03 | REJECT | JSON | `sessionId, reason` |
| 0x04 | FILE_HEADER | JSON | `index, relativePath, size` |
| 0x05 | FILE_CHUNK | **binaire** | ≤ 256 KB de données brutes |
| 0x06 | FILE_END | JSON | `index, sha256` (optionnel) |
| 0x07 | DONE | (vide) | fin du transfert |
| 0x08 | CANCEL | JSON | `reason` |
| 0x09 | ERROR | JSON | `code, message` |

**Invariant critique** : après un `FILE_HEADER` de taille N, on lit
exactement N octets via 1 ou plusieurs `FILE_CHUNK` avant le message
suivant.

## Diagramme de classes (essentiel)

```
Device ──────────┐
FileMeta ────────┤
TransferRequest  │    EventBus ◄── drain ── AppController
TransferSession  │      ▲                       │
                 │      │ post                  ├── DiscoveryService (3 threads)
AppState ◄───────┘      │                       ├── TransferServer  (1+N threads)
                        │                       └── TransferClient  (N threads)
                        │                               │
                        └────── network threads ────────┘
```

## Fichiers clés et leur rôle

| Fichier | Rôle |
|---------|------|
| `src/app/app_controller.cpp` | Orchestrateur central, possède tous les services, draine l'EventBus |
| `src/network/protocol.cpp` | Encode/décode les frames, `readFrame`/`writeFrame` bloquants |
| `src/network/discovery_service.cpp` | 3 threads : beacon + listener + TTL sweep |
| `src/network/transfer_server.cpp` | Accept loop + worker par session (réception) |
| `src/network/transfer_client.cpp` | Worker par envoi sortant |
| `src/network/broadcast_socket.cpp` | Subclass `sf::UdpSocket` pour activer SO_BROADCAST |
| `src/ui/ui_app.cpp` | Event loop SFML, navigation écrans, resize |
| `src/ui/screens/main_screen.cpp` | Écran principal (header/sidebar/centre/bas) |
| `src/ui/screens/incoming_offer_screen.cpp` | Modale offre entrante |
| `src/ui/rounded_rect.cpp` | Primitive rectangle arrondi (maison) |

## Règles d'invariant

1. **L'UI ne touche jamais aux sockets directement.** Elle appelle
   `AppController::requestSend/acceptIncoming/...` qui délèguent aux
   services réseau.
2. **Les services réseau ne touchent jamais à `AppState` directement.** Ils
   postent des événements sur l'`EventBus`.
3. **`AppController::onEvent`** est le seul endroit qui mute `AppState` en
   réaction à un event réseau.
4. **Chaque session TCP a son propre thread worker** pour ne pas bloquer
   les autres.
5. **Le port UDP listener est bind une seule fois** (45454) — donc deux
   instances sur la même machine échouent. C'est intentionnel (limite V1).
