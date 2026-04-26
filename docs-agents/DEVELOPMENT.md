# DEVELOPMENT.md — Conventions et guide de contribution

## Conventions C++

### Nommage

| Élément | Convention | Exemple |
|---------|-----------|---------|
| Classe / struct | PascalCase | `TransferSession` |
| Méthode | camelCase | `sendFiles()` |
| Variable locale / paramètre | camelCase | `sessionId` |
| Membre privé | camelCase_ | `bytesTransferred_` |
| Constante | kPascalCase OU SCREAMING_SNAKE | `kChunkSize`, `LOG_INFO` |
| Énum | enum class, PascalCase | `TransferStatus::Pending` |
| Namespace | lowercase | `ltr::network` |
| Fichier | snake_case | `transfer_session.hpp` |
| Macro | SCREAMING_SNAKE (à éviter) | — |

### Organisation d'un fichier

```cpp
// include/ltr/xxx/mon_truc.hpp
#pragma once

#include <standard>          // STL d'abord, triées
#include <algorithm>

#include <SFML/Graphics.hpp> // puis deps externes
#include <nlohmann/json.hpp>

#include "ltr/autre/dep.hpp" // puis internes

namespace ltr::xxx {

// Doc-comment court au-dessus de la classe.
class MonTruc {
public:
    explicit MonTruc(int n);     // explicit sur les ctor à 1 arg
    ~MonTruc();

    MonTruc(const MonTruc&) = delete;  // non-copiable si contient threads/sockets
    MonTruc& operator=(const MonTruc&) = delete;

    void faitQuelqueChose();
    int valeur() const noexcept { return n_; }  // getter inline

private:
    int n_;
    std::unique_ptr<Impl> impl_;
};

} // namespace ltr::xxx
```

### Règles

1. **`#pragma once`** systématiquement (jamais d'include-guards manuels).
2. **`const`-correctness** : toute méthode qui ne mute pas est `const`.
3. **`noexcept`** sur les getters et les méthodes de destruction / swap.
4. **Pas de `new`/`delete`** — toujours `std::unique_ptr` / `std::shared_ptr`.
5. **Pas d'héritage profond** — préférer la composition.
6. **`explicit`** sur les constructeurs à un seul argument sauf raison.
7. **`= delete`** les copies sur les classes qui possèdent threads, sockets,
   fichiers.

### Threading

- Tout accès partagé → `std::mutex` + `std::lock_guard`. Jamais
  d'`std::unique_lock` sauf besoin de `condition_variable`.
- `std::atomic<bool>` pour les flags `running_` / `cancel_`.
- Les threads rejoignent dans le destructeur : `if (t.joinable()) t.join();`.
- **Pas d'accès à `AppState` ou `sf::RenderWindow` hors thread UI.**
  Toujours passer par `EventBus::post`.

### Gestion d'erreurs

- Erreurs attendues (fichier inexistant, réseau rompu) → `log_warn` +
  early return. Pas d'exception.
- Exceptions pour les bugs de programmation uniquement (`std::logic_error`).
- **Pas de `catch(...)` vide.** Si on catch-all, on log + on continue
  proprement.

## Ajouter une nouvelle fonctionnalité UI

### 1. Nouveau widget

- Créer `include/ltr/ui/widgets/mon_widget.hpp` + `src/ui/widgets/mon_widget.cpp`
- Pattern chainable `setXxx()` retournant `MonWidget&`
- Méthodes `handleEvent(const sf::Event&)` et `draw(sf::RenderTarget&) const`
- Utiliser les tokens du thème (`Spacing::`, `Colors::`, `Radius::`,
  `FontSize::`) — **pas de magic numbers**
- Textes : toujours via `ltr::ui::utf8()` avant `sf::Text::setString`

Ajouter le fichier à `CMakeLists.txt` sous `add_library(ltr_ui ...)`.

### 2. Nouvel écran

- Hériter de `ltr::ui::Screen` (3 méthodes pures virtuelles)
- L'enregistrer dans `UIApp` (créer dans le constructeur, dispatch dans
  `run()`)

### 3. Nouvelle action utilisateur

Plutôt que d'appeler directement un service réseau depuis un widget :

```cpp
// Dans AppController — méthode publique
void requestFoo() { client_->doFoo(...); }

// Dans le widget/écran — boutton callback
sendBtn_.onClick([this]{ controller_.requestFoo(); });
```

## Ajouter un nouveau message de protocole

1. Ajouter le code dans `ltr::network::MessageType` (ne pas réordonner les
   codes existants !).
2. Documenter dans `docs-agents/ARCHITECTURE.md` section "Protocole".
3. Étendre `TransferServer::sessionWorker` et/ou `TransferClient::runSender`
   pour le produire/consommer.
4. Si ça affecte l'UI → ajouter un type d'événement dans `core::Event` +
   handler dans `AppController::onEvent`.
5. Ajouter un test dans `tests/test_protocol.cpp`.

## Ajouter une dépendance externe

**Par défaut, refuser.** On vise le minimum. Si vraiment nécessaire :

- Single-header uniquement (intégration via FetchContent)
- Licence permissive (MIT, BSD, public domain)
- Ajout dans `cmake/Dependencies.cmake` avec tag fixé
- Justification dans le PR / commit

## Build & tests

```bash
# Dev : configure une fois, rebuilder à chaque modif
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Tests
cmake -S . -B build -DLTR_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

# Nettoyage complet (long à reconfigure)
rm -rf build
```

## Debug macOS

```bash
# Logs complets
LTR_LOG_LEVEL=debug ./build/local_transfer

# Voir sockets ouverts
lsof -i -P | grep local_transfer

# Tracer les appels système
sudo fs_usage -w -f filesys local_transfer
```

## Debug Windows (Visual Studio)

1. Ouvrir `build/local_transfer.sln`
2. Définir `local_transfer` comme StartUp Project
3. F5 pour debugger
4. Les symboles PDB sont générés en Debug

## Checklist avant PR

- [ ] `cmake --build` propre sur les 2 OS
- [ ] `ctest` passe
- [ ] Aucun warning nouveau côté notre code (SFML peut en générer, pas
      grave)
- [ ] Pas de `TODO` / `FIXME` / `XXX` / `HACK`
- [ ] Pas de `std::cout` / `printf` de debug laissés (utiliser `Logger`)
- [ ] Pas d'accents affichés en "▬▬" dans l'UI (= oubli de `utf8()`)
- [ ] Classes avec threads/sockets sont non-copiables (`= delete`)
- [ ] Tout ajout UI utilise `RoundedRect` + tokens du thème
