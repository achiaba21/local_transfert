# include/ltr/web/routes/

> Déclarations des handlers HTTP. Un fichier par groupe fonctionnel.

## Convention

Chaque fichier expose **une seule fonction libre** `registerXxx(WebService&)`.
Le fichier `route_registrar.hpp` orchestre tous les enregistrements au
démarrage via `registerAll(svc)`.

## Groupes existants

| Fichier              | Routes                                           |
|----------------------|--------------------------------------------------|
| `auth_routes.hpp`    | `POST /api/auth`, `GET /api/me`, `GET /api/host-info` |
| `upload_routes.hpp`  | `POST /api/upload`                               |
| `download_routes.hpp`| `GET /api/download/:ticketId`                    |
| `events_routes.hpp`  | `GET /api/events` (SSE)                          |
| `self_routes.hpp`    | `GET /download/self`                             |
| `static_routes.hpp`  | `GET /`, `/app.js`, `/style.css`, `/icons/*.svg` |

## Ajouter une nouvelle route

1. Créer `xxx_routes.hpp` (juste la déclaration de `registerXxx`)
2. Créer `src/web/routes/xxx_routes.cpp` (implémentation)
3. Ajouter l'appel dans `route_registrar.cpp::registerAll`
4. Ajouter le `.cpp` dans `CMakeLists.txt`

## Lecture du cookie

Toutes les routes authentifiées utilisent le même helper local
`readTokenCookie(req)` (pas de factorisation partagée en V1 — à mutualiser
dans un util si > 5 duplications).

## Pattern pour endpoint authentifié

```cpp
server.Post("/api/xxx", [&svc](const httplib::Request& req,
                                httplib::Response& res) {
    const auto token = readTokenCookie(req);
    const auto sess = svc.sessions().validate(token);
    if (!sess) { res.status = 401; /* ... */ return; }
    svc.sessions().touch(token);
    // ... logique métier ...
});
```
