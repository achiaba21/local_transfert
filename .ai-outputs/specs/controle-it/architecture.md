# Architecture — Phase 3 : Contrôle IT

> Basée sur `business-spec.md`. Stack inchangée : C++17 / SFML 2.6.1 /
> cpp-httplib / nlohmann/json. Aucune dépendance externe nouvelle.

---

## 1. Vue d'ensemble

Phase 3 = **applicateur de policy**. Les 5 champs `BusinessPolicy`
définis en Phase 1 deviennent vivants. On introduit un nouveau
service `PolicyEnforcementService` (SRP : transformer une policy en
décisions) + plusieurs intercepteurs minces dans les routes/services
existants. Toutes les vérifications passent par une interface mince
`PolicyGuard` pour rester testables sans http.

### Composants impactés

| Couche | Nouveau | Modifié |
|---|---|---|
| `ltr::core` | — | `event_bus.hpp` (ErrorCategory::PolicyDenied) |
| `ltr::infra` | `cidr_matcher.{hpp,cpp}`, `policy_enforcement.{hpp,cpp}`, `retention_service.{hpp,cpp}` | — |
| `ltr::web` | `routes/policy_middleware.{hpp,cpp}` | `web_service.{hpp,cpp}`, `routes/route_registrar.cpp`, `routes/p2p_routes.cpp`, `http_server.{hpp,cpp}` |
| `ltr::app` | — | `app_controller.{hpp,cpp}` (instanciation + branchement purge + HTTPS forcé) |
| `assets/web` | — | `index.html` + `peers.js` (masquage P2P si désactivé) |
| `tests` | `test_cidr_matcher.cpp`, `test_policy_enforcement.cpp`, `test_retention_service.cpp` | `CMakeLists.txt` |

---

## 2. Diagramme

```
ltr::infra::PolicyService    (Phase 1, lit business_policy.json)
        │
        ▼ injecté
ltr::infra::PolicyEnforcementService  (Phase 3, NEW)
        │ expose
        ├─ isP2PAllowed() bool
        ├─ isIpAllowed(ipStr) bool          ← utilise CidrMatcher
        ├─ httpsForced() bool
        ├─ httpsRedirectUrl(host, path) std::string
        ├─ historyRetentionDays() int
        ├─ receivedFilesRetentionDays() int
        │
        ▼ utilisé par
ltr::web::PolicyMiddleware  (NEW, fonctions free)
        ├─ rejectIfP2PDisabled(svc, req, res) bool
        ├─ rejectIfIpBlocked(svc, req, res)  bool
        └─ redirectIfHttpsRequired(svc, req, res, isHttps) bool

ltr::infra::RetentionService  (NEW)
        ├─ purgeHistories(now)         ← purge TransferHistory + DepositHistory + PeersHistory
        └─ purgeReceivedFiles(now)     ← scanne downloadDir + Deposits/

ltr::infra::CidrMatcher       (NEW, pur)
        ├─ parse(cidrStr) Cidr|nullopt
        └─ matches(cidr, ip) bool      ← IPv4 + IPv6

ltr::core::ErrorCategory      (extended)
        └─ PolicyDenied = 5
```

---

## 3. Décisions d'architecture

### 3.1 `CidrMatcher` — module pur, header-only ou .cpp ?

`.hpp + .cpp` pour faciliter les tests sans tirer toute la chaîne
réseau. Parse les notations courantes :
- `192.168.1.0/24` → IPv4 CIDR.
- `192.168.1.42` → IPv4 single host (CIDR /32).
- `::1/128` → IPv6 CIDR.
- `::1` → IPv6 single (/128).

API minimale :
```cpp
struct CidrRange {
    bool isV4{true};
    std::uint32_t v4Addr{0};        // network address (IPv4)
    std::uint32_t v4Mask{0};
    std::array<std::uint8_t,16> v6Addr{};
    int           v6PrefixBits{0};
};
std::optional<CidrRange> parseCidr(const std::string& s);
bool matchCidr(const CidrRange& r, const std::string& ipStr);
```

### 3.2 `PolicyEnforcementService` — pas d'interface

Comme Phase 1/2 pour `DepositLinkService`, on garde la classe finale
sans interface : la dépendance abstraite est sur les choses qu'elle
consomme (`PolicyService&`), pas sur ses propres méthodes. Si un
test veut un mock, il fournit une `PolicyService` qui retourne une
policy custom.

### 3.3 `RetentionService` — un service, deux modes

```cpp
class RetentionService {
    explicit RetentionService(PolicyEnforcementService& policy,
                              std::filesystem::path downloadDir);
    void purgeHistories(TransferHistory& tx, DepositHistoryStore& dep,
                        PeersHistory& peers, std::int64_t now);
    int  purgeReceivedFiles(std::int64_t now);  // retourne nb supprimés
};
```

Appel : à `start()` + thread `retentionLoop_` (24 h tick).

### 3.4 `PolicyMiddleware` — fonctions free pas de classe

Les middlewares cpp-httplib sont des handlers. On fournit 3
fonctions free qui :
- Retournent `true` si elles ont écrit la réponse (= router doit
  s'arrêter).
- Retournent `false` sinon (= passe-plat).

```cpp
namespace ltr::web::routes {
bool rejectIfP2PDisabled(WebService& svc,
                         const httplib::Request& req,
                         httplib::Response& res);
bool rejectIfIpBlocked(WebService& svc,
                       const httplib::Request& req,
                       httplib::Response& res);
bool redirectIfHttpsRequired(WebService& svc,
                              const httplib::Request& req,
                              httplib::Response& res,
                              bool isHttpsServer);
}
```

`isHttpsServer` est nécessaire car le même handler est appelé pour
HTTP ET HTTPS (via MultiServer). Sur HTTPS on ne redirige jamais.

### 3.5 HTTP forcé en redirect

Au lieu de fermer le port 45456, on enregistre **avant tout autre
handler** un `pre_routing_handler` cpp-httplib sur le serveur HTTP
seul (pas HTTPS) qui renvoie 301 vers HTTPS si `httpsForced()`.
Cela permet de garder l'écoute HTTP tout en bloquant toute autre
route.

API : `HttpServer::setPreRoutingHandler(handler)`.

### 3.6 Override localhost / health

Hard-codé dans `PolicyEnforcementService::isIpAllowed()` :
```cpp
if (ip == "127.0.0.1" || ip == "::1" || ip == "localhost") return true;
```

Et la route `/health` est exemptée par `rejectIfIpBlocked` qui
inspecte `req.path`.

### 3.7 Hot-reload : NON Phase 3

`PolicyEnforcementService::reload()` peut être appelé manuellement
mais aucun watcher fichier n'est mis en place automatiquement.

### 3.8 UI verrouillée

- Côté **web** : `peers.js` lit `/api/policy/flags` (nouveau
  endpoint admin, lu après auth PIN) et masque la section P2P si
  `allowP2P=false`.
- Côté **desktop** : pas d'écran de toggle existant à verrouiller
  en Phase 3 (les paramètres HTTPS ne sont actuellement pas
  exposés à l'UI). On laisse en TODO Phase 4.

---

## 4. Structure des fichiers

```
include/ltr/
├── core/event_bus.hpp                     (MODIF — ErrorCategory::PolicyDenied)
├── infra/
│   ├── cidr_matcher.hpp                   (NEW)
│   ├── policy_enforcement.hpp             (NEW)
│   └── retention_service.hpp              (NEW)
├── web/
│   ├── http_server.hpp                    (MODIF — setPreRoutingHandler)
│   ├── web_service.hpp                    (MODIF — setter PolicyEnforcement)
│   └── routes/policy_middleware.hpp       (NEW)
└── app/app_controller.hpp                 (MODIF — owners + thread retention)

src/                                        (mêmes paths)
infra/cidr_matcher.cpp
infra/policy_enforcement.cpp
infra/retention_service.cpp
web/http_server.cpp                         (MODIF — preRoutingHandler)
web/routes/policy_middleware.cpp            (NEW)
web/routes/route_registrar.cpp              (MODIF — branchement middlewares)
web/routes/p2p_routes.cpp                   (MODIF — rejectIfP2PDisabled en tête)
app/app_controller.cpp                      (MODIF)

tests/
├── test_cidr_matcher.cpp                   (NEW)
├── test_policy_enforcement.cpp             (NEW)
├── test_retention_service.cpp              (NEW)
└── CMakeLists.txt                          (MODIF)

CMakeLists.txt                              (MODIF — sources ltr_core)
assets/web/js/peers.js                      (MODIF léger — masque P2P si policy refuse)
```

---

## 5. CONTRAT D'IMPLÉMENTATION

### `ltr::core`
- [ ] `event_bus.hpp` : ajouter `PolicyDenied = 5` à `ErrorCategory`.

### `ltr::infra`
- [ ] `cidr_matcher.{hpp,cpp}` :
  - `parseCidr(string)` → `optional<CidrRange>` (IPv4 + IPv6).
  - `matchCidr(CidrRange, string)` → bool.
  - Pas de dep externe.
- [ ] `policy_enforcement.{hpp,cpp}` :
  - ctor `(PolicyService& policy)`.
  - `isP2PAllowed() / httpsForced() / historyRetentionDays() /
    receivedFilesRetentionDays() / isIpAllowed(ipStr)`.
  - Cache des `CidrRange` parsés au reload.
  - Override implicite : `127.0.0.1`, `::1`, `localhost` → always true.
- [ ] `retention_service.{hpp,cpp}` :
  - ctor `(PolicyEnforcementService& policy, fs::path downloadDir)`.
  - `purgeHistories(...)`.
  - `purgeReceivedFiles(now)` → renvoie le nombre de fichiers
    supprimés. Skip `Deposits/.receipts/`.

### `ltr::web`
- [ ] `http_server.hpp/.cpp` : ajouter
  `setPreRoutingHandler(std::function<httplib::Server::HandlerResponse(
   const Request&, Response&)>)` qui registre via
  `set_pre_routing_handler` côté httplib.
- [ ] `web_service.{hpp,cpp}` :
  - setter `setPolicyEnforcement(PolicyEnforcementService*)`.
  - accessor `policyEnforcement()`.
- [ ] `routes/policy_middleware.{hpp,cpp}` : 3 fonctions free
  documentées dans §3.4.
- [ ] `routes/route_registrar.cpp` :
  - **AVANT** `registerAuth(svc)`, installer le pre-routing
    handler `redirectIfHttpsRequired` côté serveur HTTP seul.
  - **AVANT** chaque handler concret, ajouter check whitelist via
    `rejectIfIpBlocked` (sauf /health).
- [ ] `routes/p2p_routes.cpp` : en tête de chaque handler P2P,
  appeler `rejectIfP2PDisabled` ; return early si true.

### `ltr::app`
- [ ] `app_controller.{hpp,cpp}` :
  - Instancier `PolicyEnforcementService` et `RetentionService`.
  - Brancher dans `WebService::setPolicyEnforcement(...)`.
  - Lancer un thread `retentionLoop_` (sleep 24 h, tick purge).
  - Au démarrage : `retention->purgeHistories(...)` +
    `retention->purgeReceivedFiles(now)`.
  - À l'arrêt : joindre le thread.

### Assets web
- [ ] `assets/web/js/peers.js` : après fetch /api/policy/flags
  (nouveau endpoint lu seulement avec session), si
  `allowP2P=false` cacher la section P2P (`peers-section`).
- [ ] `routes/policy_middleware.cpp` peut exposer en bonus
  `GET /api/policy/flags` (admin, retourne JSON simple).

### Tests
- [ ] `test_cidr_matcher.cpp` : parse 192.168.1.0/24 → match
  192.168.1.5 OK / 10.0.0.1 KO ; IPv6 ::1/128 → ::1 OK /
  fe80::1 KO ; hôte sans CIDR → /32 implicite ; CIDR invalide
  → nullopt.
- [ ] `test_policy_enforcement.cpp` : whitelist vide = tout
  autorisé ; whitelist non-vide = exclusion ; localhost
  toujours autorisé ; `allowP2P=false` retourne false.
- [ ] `test_retention_service.cpp` : crée des fichiers temp avec
  mtime ancien, vérifie purge ; receivedFilesDays=0 → no-op.
- [ ] `tests/CMakeLists.txt` : déclare les 3 tests.

### Build
- [ ] `CMakeLists.txt` racine : ajout des 3 nouveaux .cpp infra +
  policy_middleware à `ltr_core`.

---

## UI_REQUIRED: false

Phase 3 est principalement infra. La seule modification UI est un
masquage conditionnel de la section P2P web via `peers.js`
(modification de quelques lignes JS, pas un nouveau composant).
Aucune option UI/UX à présenter.
