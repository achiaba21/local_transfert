# include/ltr/web/

> Headers publics de la couche web. Tous sous `namespace ltr::web`.

## Entrée publique

`web_service.hpp` — `WebService` est la **façade** qu'utilise l'extérieur
(en l'occurrence : `AppController`). Elle agrège tout le reste.

## Convention

- Un header par classe / type
- Membres privés en `_` suffixe (convention du projet)
- Stores thread-safe (mutex interne) — méthodes publiques auto-contenues
- Chainable setters (`setXxx() -> Xxx&`) comme dans `ltr::ui`

## Pour en savoir plus

- Vue d'ensemble, flux, endpoints : `docs-agents/WEB.md`
- Implémentations : `src/web/`
- Assets statiques sources : `assets/web/`
- Assets embarqués générés : `build/generated/ltr/web/assets/`

## Règles à ne pas casser

1. **Aucune inclusion SFML ici** sauf `qr_code.hpp` (retourne `sf::Image`,
   accepté car consommé uniquement par l'UI)
2. **Aucun accès `AppState`** — communication via `EventBus` uniquement
3. **C++17 strict** — pas de `std::format`, pas de `<ranges>`
4. Header-only ou single-file pour toutes les deps (cpp-httplib ✅,
   qrcodegen ✅ 1 .cpp, miniz ✅ .c files)
