# AGENTS.md — LocalTransfer

> Fichier d'amorçage pour toute session Codex travaillant sur ce projet.
> Lire d'abord, puis pointer vers la doc plus détaillée au besoin.

## En une ligne

Application **desktop C++17 / SFML 2.6** de transfert de fichiers
peer-to-peer sur réseau local (LAN), cross-platform **macOS ↔ Windows**.
Inspiré de LocalSend/AirDrop, zéro configuration, zéro cloud.

## Stack & contraintes

- **C++17 obligatoire** (pas 20, pas 14)
- **SFML 2.6.1** (fenêtre + graphique + sockets)
- **CMake 3.20+** avec `FetchContent` pour toutes les deps
- **Cross-platform** : macOS (Clang), Windows (MSVC 2022), Linux ok (non testé)
- Deps externes **header-only uniquement** : nlohmann/json, picosha2,
  tinyfiledialogs

## Ports réseau

| Port | Protocole | Usage |
|------|-----------|-------|
| 45454 | UDP | beacon de découverte (broadcast 255.255.255.255) |
| 45455 | TCP | transferts |

## Structure des dossiers

```
.
├── CMakeLists.txt
├── cmake/Dependencies.cmake          # FetchContent des deps
├── include/ltr/                      # headers publics (namespace ltr::*)
│   ├── core/       (types, event_bus, logger, format)
│   ├── domain/     (device, file_meta, transfer_*)
│   ├── network/    (protocol, discovery, transfer_server, transfer_client)
│   ├── infra/      (config, filesystem, hash)
│   ├── app/        (app_state, app_controller)
│   └── ui/         (theme, rounded_rect, screen, ui_app, widgets/, screens/)
├── src/                              # implémentations (même arborescence)
├── tests/          (test_protocol, test_hash)
├── assets/fonts/   (police Inter — fallback système si absente)
├── .ai-outputs/
│   ├── docs/       (documentation HTML publiée)
│   └── specs/      (business-spec, architecture, ui-proposal, audit)
└── docs-agents/    (CE DOSSIER — markdowns pour futurs agents)
    ├── PROJECT.md         quickstart + vue d'ensemble
    ├── ARCHITECTURE.md    couches, flux, diagrammes
    ├── DEVELOPMENT.md     conventions C++, ajouter une feature
    └── UI_GUIDELINES.md   design system + règles SFML maison
```

## Règles d'or (non négociables)

1. **RAII partout** — pas de `new`/`delete` manuel, toujours `unique_ptr` ou
   `shared_ptr`.
2. **Headers → `include/ltr/...`, impls → `src/...`**. Un `.hpp` par classe,
   avec `#pragma once`.
3. **Namespace ltr::** avec sous-namespaces (`ltr::domain`, `ltr::core`,
   `ltr::network`, `ltr::infra`, `ltr::app`, `ltr::ui`).
4. **Thread-safety via `EventBus`** — jamais d'accès direct à l'UI depuis
   un thread réseau. Les threads postent des `core::Event`, l'UI draine à
   chaque frame.
5. **Toutes les chaînes affichées** passent par `ltr::ui::utf8()` avant
   d'être fournies à SFML. Sinon les accents s'affichent en blocs 🔴.
6. **Tous les boutons et cards** utilisent `RoundedRect` (widget maison)
   pour un rendu moderne cohérent. Pas de `sf::RectangleShape` nu dans
   l'UI sauf séparateurs 1 px.
7. **Constants centralisées** dans `ltr::ui::Colors`, `Spacing`, `Radius`,
   `FontSize` (cf. `include/ltr/ui/theme.hpp`). Pas de magic numbers.
8. **Pas de dépendance externe nouvelle** sans discussion — on limite à
   SFML + 3 single-header.

## Points de vigilance pour futur code

- **SFML `sf::Text::setString(std::string)`** interprète en **Latin-1**. Pour
  de l'UTF-8, utiliser `sf::String::fromUtf8(...)` via le helper
  `ltr::ui::utf8()`.
- **SFML `sf::UdpSocket`** n'active pas `SO_BROADCAST` automatiquement : on
  passe par `BroadcastUdpSocket` (héritage + setsockopt per-OS).
- **Un thread UI unique** (main thread) possède l'OpenGL context. Les
  autres threads ne doivent **jamais** toucher `sf::RenderWindow`.
- **Chunks TCP** de 256 KB (`core::kChunkSize`). Ne pas baisser sous 64 KB
  (perf) ni monter au-dessus de 1 MB (RAM).

## Commandes build

```bash
# configure (~30 min la 1re fois, FetchContent télécharge SFML + deps)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# compile (3-5 min)
cmake --build build --config Release -j

# lancer
./build/local_transfer        # macOS
build\Release\local_transfer  # Windows

# tests
cmake -S . -B build -DLTR_BUILD_TESTS=ON
ctest --test-dir build --output-on-failure
```

## Avant de commit / livrer

- [ ] Build propre des deux côtés (`cmake --build` sans erreur)
- [ ] Les 2 tests passent (`ctest`)
- [ ] Aucun accent affiché en "▬▬" dans l'UI (= oubli de `utf8()`)
- [ ] Aucun `new/delete`, `TODO`/`FIXME`, `catch(...)` vide
- [ ] Si nouveau widget UI → radius + shadow cohérents avec le thème
