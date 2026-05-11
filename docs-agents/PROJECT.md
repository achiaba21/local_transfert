# PROJECT.md — LocalTransfer · Vue d'ensemble

## Quickstart (5 min)

```bash
# Depuis la racine du projet
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
./build/local_transfer
```

Le premier `cmake -S . -B build` prend **~30 min** car `FetchContent` clone
SFML (~160 Mo), nlohmann/json, cpp-httplib, picosha2, QR-Code-generator,
miniz et tinyfiledialogs. Les lancements suivants sont instantanés grâce au
cache. OpenSSL doit être disponible sur la machine de build.

## Qu'est-ce que c'est ?

Une application **desktop** qui permet de transférer des fichiers et dossiers
entre machines sur le même réseau local, sans cloud. Les pairs natifs
macOS/Windows utilisent UDP + TCP direct ; les téléphones, tablettes et autres
navigateurs passent par l'interface web embarquée, avec transferts host ↔ web
et web ↔ web via WebRTC DataChannel.

### Flux utilisateur type

1. L'utilisateur ouvre l'app sur ses deux machines natives, ou ouvre l'URL/QR
   web depuis un navigateur LAN.
2. Chaque machine se détecte automatiquement via UDP broadcast.
3. Les navigateurs authentifiés par PIN apparaissent aussi comme devices web.
4. L'utilisateur sélectionne un destinataire dans la sidebar.
5. Il clique sur "Parcourir", colle depuis le presse-papier ou utilise le web.
6. Il clique sur "ENVOYER".
7. Le destinataire accepte si une décision est requise.
8. Le transfert part en TCP natif, HTTP(S) host ↔ web ou WebRTC web ↔ web.
9. Les fichiers arrivent dans `~/Downloads/LocalTransfer/` ou dans le
   navigateur selon le sens du flux.

## Pourquoi cette stack ?

| Choix | Raison |
|------|--------|
| **C++ plutôt que Rust/Go** | Contrainte utilisateur. SFML est C++ natif. |
| **SFML 2.6** (pas 3.0) | 3.0 est trop récent (breaking changes), docs abondantes en 2.6 |
| **Pas Qt / Dear ImGui** | Contrainte utilisateur : rendu 100 % SFML |
| **UDP broadcast** (pas mDNS) | Zéro dep externe, suffisant pour un LAN domestique |
| **HTTPS LAN auto-signé** | Nécessaire pour WebCrypto, clipboard browser et session web persistante ; empreinte affichée pour vérification manuelle. |
| **TOFU plutôt que PKI** | Modèle local pragmatique : première empreinte connue, warning si changement. |
| **CMake FetchContent** | Build reproductible sans pré-install de SFML |

## Périmètre livré

- ✅ Découverte auto UDP
- ✅ Transfert fichiers + dossiers (arborescence préservée)
- ✅ Multi-destinataires (broadcast ou 1-à-1)
- ✅ Code PIN d'appairage
- ✅ Progression temps réel (%, vitesse, ETA)
- ✅ Cross-platform macOS / Windows
- ✅ UI moderne (coins arrondis, ombres, spacing généreux)
- ✅ Drag-and-drop et presse-papier natifs selon plateforme
- ✅ **Interface web embarquée** (HTTP 45456, HTTPS 45457) — tout navigateur
  LAN peut échanger sans installation.
- ✅ Transfert host ↔ navigateur via HTTP(S), avec tickets de download
- ✅ Transfert navigateur ↔ navigateur via WebRTC DataChannel
- ✅ Certificat HTTPS auto-signé, empreinte affichée et TOFU TCP/P2P
- ✅ Sessions web plus résilientes : TTL 5 min, cookie persistant optionnel,
  PIN mémorisable côté navigateur en HTTPS
- ✅ Mécanismes de reprise partiels : sidecars TCP, retry upload HTTP,
  Range requests download HTTP, partiels OPFS côté P2P web
- ✅ Historique persistant des pairs et transferts côté host

Voir `docs-agents/WEB.md` pour le détail de la couche web.

## Hors périmètre / partiel

- ❌ Vrai resume bout en bout non uniforme : upload HTTP avec offset et
  renégociation WebRTC DataChannel restent reportés.
- ❌ Chiffrement bout en bout des chunks TCP natifs. Le TCP natif vérifie
  l'empreinte du pair via TOFU, mais ne chiffre pas encore le payload.
- ❌ App native mobile iOS / Android. Les mobiles sont supportés via navigateur.
- ❌ Notification système native
- ❌ Deux instances sur la même machine (conflit port UDP)
- ❌ Vue historique desktop complète et sidebar de pairs récents offline
  reportées malgré les données persistées.

## Où trouver quoi

| Besoin | Fichier |
|--------|---------|
| Principes et règles d'or | `CLAUDE.md` (racine) |
| Architecture détaillée + diagrammes | `docs-agents/ARCHITECTURE.md` |
| **Couche web (HTTP + navigateur)** | `docs-agents/WEB.md` |
| Conventions de code C++ | `docs-agents/DEVELOPMENT.md` |
| Design system + widgets UI | `docs-agents/UI_GUIDELINES.md` |
| Spécification métier | `.ai-outputs/specs/local-file-transfer/business-spec.md` |
| Architecture technique formelle | `.ai-outputs/specs/local-file-transfer/architecture.md` |
| Propositions UI validées | `.ai-outputs/specs/local-file-transfer/ui-proposal.md` |
| Rapport d'audit qualité | `.ai-outputs/specs/local-file-transfer/audit-report.md` |
| Doc publique HTML | `.ai-outputs/docs/local-file-transfer.html` |
| Logique métier entités | `include/ltr/domain/` |
| Protocole réseau | `include/ltr/network/protocol.hpp` |
| Orchestration | `src/app/app_controller.cpp` |
| Point d'entrée | `src/main.cpp` |
