# LocalTransfer

Application desktop C++17/SFML de transfert de fichiers en peer-to-peer sur
réseau local. Cross-platform **macOS ↔ Windows**.

## Fonctionnalités

- Découverte automatique des pairs sur le LAN (UDP broadcast)
- Transfert fichiers et dossiers en TCP direct
- Code PIN d'appairage à l'écran
- Progression temps réel (%, vitesse, ETA)
- Multi-destinataires
- Interface web embarquée pour les navigateurs du LAN
- HTTPS LAN avec certificat auto-signé et empreinte affichée
- WebRTC DataChannel pour les transferts navigateur ↔ navigateur
- Reprise partielle selon le flux : sidecars TCP, retry HTTP upload,
  Range requests HTTP download, récupération de partiels P2P côté navigateur
- Historique local des pairs et des transferts
- Aucune configuration requise

## Dépendances

Récupérées automatiquement via CMake `FetchContent` :

- [SFML 2.6.1](https://www.sfml-dev.org/) — fenêtre, graphique, sockets
- [nlohmann/json 3.11.3](https://github.com/nlohmann/json) — JSON
- [cpp-httplib 0.18.1](https://github.com/yhirose/cpp-httplib) —
  serveur HTTP/HTTPS embarqué
- [OpenSSL](https://www.openssl.org/) — HTTPS LAN, certificats et HMAC
- [picosha2](https://github.com/okdshin/PicoSHA2) — SHA-256
- [QR-Code-generator](https://github.com/nayuki/QR-Code-generator) — QR code
- [miniz](https://github.com/richgel999/miniz) — ZIP streamé et PNG minimal
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) —
  sélection de fichiers native

Prérequis système : CMake ≥ 3.20, compilateur C++17, Git, OpenSSL.
Sur macOS, le build cherche par défaut OpenSSL dans
`/opt/homebrew/opt/openssl@3`.

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

Les polices Geist sont attendues dans `assets/fonts/Geist-Regular.ttf` et
`assets/fonts/Geist-Bold.ttf`. Elles sont copiées à côté de l'exécutable au
build. Sans police bundle, l'application tente de charger Inter puis une
police système (voir `src/ui/theme.cpp`).

## Architecture

Voir `docs-agents/ARCHITECTURE.md` et `docs-agents/WEB.md`.

## Ports utilisés

- **UDP 45454** — beacon de découverte
- **TCP 45455** — transferts
- **HTTP 45456** — interface web embarquée, fallback 45457-45465
- **HTTPS 45457** — interface web sécurisée si certificat disponible,
  fallback 45458-45459

Le pare-feu doit autoriser ces ports en entrée/sortie sur le réseau local.

## Limitations connues

- Drag-and-drop natif disponible selon plateforme, mais le sélecteur de
  fichiers reste le chemin principal.
- HTTPS protège la couche web. Le protocole TCP natif utilise une vérification
  TOFU d'empreinte, mais les chunks ne sont pas encore chiffrés bout en bout.
- La reprise n'est pas uniforme : TCP et downloads HTTP ont des mécanismes
  dédiés, les uploads HTTP retentent depuis le début, et le vrai resume
  WebRTC DataChannel est reporté.
- Support desktop natif macOS / Windows. Les mobiles passent par l'interface
  web, pas par une app native.
- Deux instances sur la même machine restent limitées par les ports fixes,
  notamment le listener UDP 45454.

## Licence

Projet personnel, non diffusé.
