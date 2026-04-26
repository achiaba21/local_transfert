# LocalTransfer

Application desktop C++17/SFML de transfert de fichiers en peer-to-peer sur
réseau local. Cross-platform **macOS ↔ Windows**.

## Fonctionnalités (V1)

- Découverte automatique des pairs sur le LAN (UDP broadcast)
- Transfert fichiers et dossiers en TCP direct
- Code PIN d'appairage à l'écran
- Progression temps réel (%, vitesse, ETA)
- Multi-destinataires
- Aucune configuration requise

## Dépendances

Récupérées automatiquement via CMake `FetchContent` :

- [SFML 2.6.1](https://www.sfml-dev.org/) — fenêtre, graphique, sockets
- [nlohmann/json 3.11.3](https://github.com/nlohmann/json) — JSON
- [picosha2](https://github.com/okdshin/PicoSHA2) — SHA-256
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) —
  sélection de fichiers native

Prérequis système : CMake ≥ 3.20, compilateur C++17, Git.

### Dépendances de build SFML (natives)

- **macOS** : Xcode Command Line Tools (`xcode-select --install`)
- **Windows** : Visual Studio 2022 avec le workload "Desktop C++"
- **Linux** (optionnel) : `libx11-dev libxrandr-dev libxcursor-dev libudev-dev libfreetype-dev libopenal-dev libflac-dev libvorbis-dev libgl1-mesa-dev libegl1-mesa-dev`

## Build

```bash
# depuis la racine du projet
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

L'exécutable est produit dans `build/` (ou `build/Release/` sous MSVC).

### Build avec tests

```bash
cmake -S . -B build -DLTR_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Assets

Placer une police TrueType dans `assets/fonts/Inter-Regular.ttf`.
Sans police bundle, l'application tentera de charger une police système (voir
`src/ui/theme.cpp`).

## Architecture

Voir `.ai-outputs/specs/local-file-transfer/architecture.md`.

## Ports utilisés

- **UDP 45454** — beacon de découverte
- **TCP 45455** — transferts

Le pare-feu doit autoriser ces ports en entrée/sortie sur le réseau local.

## Limitations connues (V1)

- Pas de drag-and-drop (sélection via bouton "Parcourir")
- Pas de chiffrement des données (LAN de confiance uniquement)
- Pas de reprise sur interruption
- Support desktop uniquement (pas de mobile)

## Licence

Projet personnel, non diffusé.
