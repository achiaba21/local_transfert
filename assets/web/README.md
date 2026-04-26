# assets/web/

> Sources du frontend web **vanilla** servi par le `WebService` C++.
>
> V1.1.7 : rangement par type (HTML / CSS / JS) dans des sous-dossiers
> distincts pour la clarté. Les URLs HTTP restent à la racine (`/app.js`,
> `/style.css`, etc.) — c'est `ltr_embed_file()` dans `CMakeLists.txt` qui
> fait le pont entre chemin disque et route web.

## Arborescence

```
assets/web/
├── html/               Pages HTML
│   ├── index.html      → GET /     (protégé par cookie)
│   └── login.html      → GET /login
├── css/                Feuilles de style
│   └── style.css       → GET /style.css
├── js/                 Modules JS (vanilla)
│   ├── common.js       → GET /common.js
│   ├── upload.js       → GET /upload.js
│   ├── download.js     → GET /download.js
│   ├── app.js          → GET /app.js
│   └── login.js        → GET /login.js
├── icons/              Icônes SVG (disponibles, actuellement non utilisées)
└── README.md           Ce fichier
```

## Pages (HTML)
| Fichier | Route | Rôle |
|---|---|---|
| `html/login.html` | `GET /login` | Formulaire PIN (public, pas d'auth) |
| `html/index.html` | `GET /` (protégé) | Interface principale (envoyer / recevoir) |

## CSS
| Fichier | Rôle |
|---|---|
| `css/style.css` | Tokens alignés sur `ltr::ui::Theme` + layout + composants |

## Modules JS
V1.1.7 : refactor en modules ciblés par responsabilité (au lieu d'un gros `app.js`).

| Fichier | Ordre chargement | Responsabilité |
|---|---|---|
| `common.js` | 1 | Utilitaires partagés exposés via `window.LTR.*` : `clientLog`, `goToLogin`, `iconFor`, `formatBytes`, `detectPlatform`, `supportsFolderPick`, etc. |
| `upload.js` | 2 | Drop zone + 2 file inputs (fichiers / dossier `webkitdirectory`) + flow announce → upload chunked avec progression |
| `download.js` | 3 | EventSource SSE `/api/events` + rendering de la liste "Reçus" + XHR download avec barre de progression |
| `app.js` | 4 | Bootstrap : host-info, session check, install banner, logout, heartbeat `/api/ping` 10s, init des modules upload+download |
| `login.js` | — (page /login uniquement) | Saisie PIN + POST `/api/auth` + navigation vers `/` |

**Convention** : chaque module attache ses points d'entrée publics à `window.LTR.*`. Pas de modules ES (pas de `type="module"`) pour éviter les subtilités CORS/mobile et garder le système d'embed C++ simple.

### Icônes (SVG)
`icons/upload.svg`, `icons/download.svg`, `icons/file.svg` — disponibles pour usage futur. Les rendu actuels utilisent des emojis Unicode (plus universels).

## Conventions

### Design tokens
CSS vars `:root` en miroir exact de `include/ltr/ui/theme.hpp` :
`--accent`, `--bg`, `--surface`, `--separator`, `--success`, `--error`, `--warning`, `--r-sm/md/lg/pill`, `--sp-xs/sm/md/lg/xl/xxl`, `--fs-h1/body/small/overline/pin`.

**Règle** : toute modif du `Theme` SFML doit être propagée ici.

### Contraintes
- **Vanilla JS + CSS** : pas de bundler, pas de framework, pas de Node
- **Taille** : < 80 Ko total (tous les `.js` + CSS + HTML), checkable via `du -sh assets/web/`
- **Responsive** : mobile-first, breakpoint desktop 720 px
- **Accessibilité** : contraste AA, focus visibles, aria-labels, tap-targets ≥ 44 px mobile

## Threading côté serveur

Quelques règles **immuables** pour les agents qui touchent à la couche web :

1. **JAMAIS** de travail lourd (> 50 ms) dans `AppController` qui est sur le thread UI.
   → Le zippage d'un dossier avant pushFiles a été déplacé dans un `std::thread` détaché (`WebService::zipAndAnnounce`) pour ne pas freezer l'UI SFML.
2. **JAMAIS** d'accès à `AppState` depuis un thread web (workers cpp-httplib, keepalive, zip worker).
   → Tout passe par `core::EventBus::post()` qui est thread-safe.
3. **JAMAIS** d'appel bloquant à la lib SFML depuis un thread non-UI.

## Embed CMake

Les fichiers ici sont **embarqués dans le binaire C++** via `EmbedFile.cmake` (custom command). À chaque modif :

```bash
cmake --build build
# CMake détecte le changement → regénère <fichier>.hpp → recompile static_routes.cpp
# Cycle : 2-5 s
```

Pas de hot-reload : itération = modif + rebuild.

Chaque nouveau fichier doit être :
1. Ajouté à `CMakeLists.txt` via `ltr_embed_file(...)`
2. Inclus dans `LTR_WEB_GEN_HEADERS`
3. Référencé dans `src/web/routes/static_routes.cpp` avec une route `server.Get("/nom", ...)`

## Itération dev (tips)

- **Hard refresh navigateur** après chaque rebuild : iOS Safari cache agressivement → `Réglages → Safari → Effacer historique et données`
- **Logs côté JS** : `window.LTR.clientLog('info', 'msg')` → visible dans `/tmp/localtransfer.log` avec préfixe `[client:*]`
- **Inspection iOS Safari** depuis Mac : câble USB + Mac Safari → Dev Tools → iPhone dans la liste
