# PROJECT.md — LocalTransfer · Vue d'ensemble

## Quickstart (5 min)

```bash
# Depuis la racine du projet
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
./build/local_transfer
```

Le premier `cmake -S . -B build` prend **~30 min** car `FetchContent` clone
SFML (~160 Mo), nlohmann/json, picosha2 et tinyfiledialogs. Les lancements
suivants sont instantanés grâce au cache.

## Qu'est-ce que c'est ?

Une application **desktop** qui permet de transférer des fichiers et
dossiers entre deux machines (Mac / Windows) sur le même réseau local, sans
configuration et sans cloud.

### Flux utilisateur type

1. L'utilisateur ouvre l'app sur ses deux machines.
2. Chaque machine se détecte automatiquement via UDP broadcast.
3. L'utilisateur sélectionne un destinataire dans la sidebar.
4. Il clique sur "Parcourir" pour choisir des fichiers ou un dossier.
5. Il clique sur "ENVOYER".
6. Le destinataire voit une modale avec un code PIN à vérifier.
7. Il accepte → transfert TCP direct avec barre de progression.
8. Les fichiers arrivent dans `~/Downloads/LocalTransfer/`.

## Pourquoi cette stack ?

| Choix | Raison |
|------|--------|
| **C++ plutôt que Rust/Go** | Contrainte utilisateur. SFML est C++ natif. |
| **SFML 2.6** (pas 3.0) | 3.0 est trop récent (breaking changes), docs abondantes en 2.6 |
| **Pas Qt / Dear ImGui** | Contrainte utilisateur : rendu 100 % SFML |
| **UDP broadcast** (pas mDNS) | Zéro dep externe, suffisant pour un LAN domestique |
| **Pas de TLS** | LAN de confiance assumé. PIN suffit pour l'appairage. |
| **CMake FetchContent** | Build reproductible sans pré-install de SFML |

## Périmètre V1 (livré)

- ✅ Découverte auto UDP
- ✅ Transfert fichiers + dossiers (arborescence préservée)
- ✅ Multi-destinataires (broadcast ou 1-à-1)
- ✅ Code PIN d'appairage
- ✅ Progression temps réel (%, vitesse, ETA)
- ✅ Cross-platform macOS / Windows
- ✅ UI moderne (coins arrondis, ombres, spacing généreux)
- ✅ **Interface web embarquée** (port 45456) — tout navigateur LAN
  peut échanger sans installation. Auto-propagation du binaire par OS.
  Voir `docs-agents/WEB.md`.

## Hors périmètre V1 (repoussé V2)

- ❌ Drag-and-drop natif (nécessite code Cocoa + Win32)
- ❌ Reprise sur interruption (resume)
- ❌ Chiffrement TLS des chunks
- ❌ Support mobile iOS / Android
- ❌ Notification système native
- ❌ Deux instances sur la même machine (conflit port UDP)

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
