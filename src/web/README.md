# src/web/

> Implémentations de la couche web. Voir `include/ltr/web/` pour les
> déclarations et `docs-agents/WEB.md` pour le guide global.

## Fichiers

### Façade
- `web_service.cpp` — Orchestre server + stores + SSE + keepalive thread

### Transport HTTP
- `http_server.cpp` — Wrapper `httplib::Server` + thread d'écoute

### Stores thread-safe
- `web_session_store.cpp` — Sessions authentifiées (PIN → token → Device)
- `download_ticket_store.cpp` — Tickets one-shot host → browser

### Signaling SSE
- `sse_channel.cpp` — Queue message bloquante 1 session
- `sse_broadcaster.cpp` — Multiplexeur de canaux par session

### Utilitaires
- `qr_code.cpp` — `QrCode::render()` (qrcodegen → sf::Image)

### Self-binary (conditionnel selon OS — UNE SEULE compilée)
- `self_binary_mac.cpp` — Zip `.app` bundle via miniz
- `self_binary_win.cpp` — Stream `.exe` via `GetModuleFileName`
- `self_binary_posix.cpp` — Stream binaire via `/proc/self/exe`

### Routes (voir `routes/README.md`)
- `routes/route_registrar.cpp`
- `routes/auth_routes.cpp`
- `routes/upload_routes.cpp`
- `routes/download_routes.cpp`
- `routes/events_routes.cpp`
- `routes/self_routes.cpp`
- `routes/static_routes.cpp`

## Règles d'implémentation

1. **Threads** : cpp-httplib gère 1 thread par requête. Nos callbacks sont
   invoqués depuis ces threads workers → JAMAIS d'accès SFML / AppState.
   Communication uniquement via `svc.bus().post(...)`.
2. **Mutex** : les stores ont leur propre `std::mutex`. Les routes
   n'ajoutent pas de verrous supplémentaires.
3. **RAII** : `std::unique_ptr` pour les services, `std::shared_ptr` pour
   ce qui est passé à des lambdas longue vie (ex: filestream dans
   `download_routes`).
4. **Erreurs** : tout `catch(...)` doit logger via `core::log_error`.
   Ne jamais avaler silencieusement.
5. **Chemins** : `std::filesystem::path` partout, jamais de concat
   manuelle `a + "/" + b`.
