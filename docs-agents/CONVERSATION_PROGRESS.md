# CONVERSATION_PROGRESS.md — État de la roadmap Business

Ce fichier récapitule où la conversation est arrivée, ce qui a été livré, ce
qui reste à faire, et sur quelles sources les décisions se basent.

## 1) Demande initiale

Objectif utilisateur: transformer la roadmap d'amélioration business en
fonctionnalités concrètes, en respectant:

- Single Responsibility Principle;
- Dependency Inversion Principle;
- architecture existante du projet LocalTransfer;
- gratuité du grand public sans quota artificiel.

La roadmap produit/marketing a d'abord été formalisée dans:

- `docs-agents/BUSINESS_GROWTH_PLAN.md`

## 2) Sources utilisées

Les décisions et implémentations se basent sur:

- `docs-agents/PROJECT.md`
  - état réel du produit;
  - interface web embarquée;
  - HTTPS LAN;
  - QR;
  - WebRTC web ↔ web;
  - historique persistant;
  - limites hors périmètre.
- `docs-agents/BUSINESS.md`
  - packaging Personal / Business / Enterprise;
  - quota `500 Go / mois / host`;
  - policies;
  - licensing;
  - audit.
- `docs-agents/WEB.md`
  - détails web host ↔ browser;
  - P2P web ↔ web;
  - historique host;
  - diagnostics;
  - état des features web.
- Code existant:
  - `include/ltr/infra/transfer_history.hpp`
  - `src/infra/transfer_history.cpp`
  - `src/web/routes/history_routes.cpp`
  - `src/network/transfer_client.cpp`
  - `src/network/transfer_server.cpp`
  - `src/web/routes/upload_routes.cpp`
  - `src/web/routes/download_routes.cpp`
  - `src/app/app_controller.cpp`

## 3) Roadmap validée

La roadmap retenue est:

1. Phase 1 — Offre Business vendable.
2. Phase 2 — Portail client externe.
3. Phase 3 — Contrôle IT.
4. Phase 4 — Enterprise.
5. Phase 5 — Growth grand public.

## 4) Phase 1 livrée

Phase 1 a été démarrée et un premier socle technique a été implémenté.

### 4.1 Policies Business

Ajout:

- `include/ltr/infra/business_policy.hpp`
- `src/infra/business_policy.cpp`

Responsabilité unique:

- représenter les plans Business;
- charger/sauvegarder les policies;
- fournir un `PolicyService` qui dépend d'une interface `PolicyRepository`.

Principes appliqués:

- SRP: parsing JSON et logique policy séparés du réseau et de l'UI;
- DIP: `PolicyService` dépend de `PolicyRepository`, pas directement d'un
  fichier JSON.

Fichier runtime prévu:

- `cfgDir/business_policy.json`

Par défaut:

- `PersonalFree`;
- quota désactivé;
- donc aucun changement de comportement pour le grand public.

### 4.2 Quota mensuel host

Ajout:

- `include/ltr/infra/quota_service.hpp`
- `src/infra/quota_service.cpp`

Responsabilité unique:

- calculer la période mensuelle UTC;
- décider si un transfert est autorisé;
- réserver les octets attendus;
- commit les octets réellement transférés;
- libérer une réservation si le transfert échoue ou est annulé.

Principes appliqués:

- SRP: le quota ne sait rien de SFML, HTTP, TCP ou UI;
- DIP: les couches réseau dépendent de l'interface `TransferQuota`, pas de
  `JsonQuotaRepository`.

Fichier runtime prévu:

- `cfgDir/quota_usage.json`

Règles livrées:

- quota par host;
- reset mensuel UTC;
- `hard-block`;
- P2P pur non compté;
- Personal Free sans quota.

Flux comptés:

- TCP sortant;
- TCP entrant;
- HTTP upload web → host;
- HTTP download host → web;
- bundle ZIP host → web.

### 4.3 Export audit

Ajout:

- `include/ltr/infra/audit_export_service.hpp`
- `src/infra/audit_export_service.cpp`
- endpoint `/api/host-history/export`

Responsabilité unique:

- transformer un snapshot `TransferHistory` en CSV ou JSON exportable.

Formats:

- `/api/host-history/export?format=json`
- `/api/host-history/export?format=csv`

Champs exportés:

- `sessionId`;
- `peerDeviceId`;
- `peerName`;
- `direction`;
- `kind`;
- `fileCount`;
- `totalBytes`;
- `status`;
- `startedAt`;
- `finishedAt`;
- `error`.

### 4.4 Injection dans l'application

Modification:

- `src/app/app_controller.cpp`
- `include/ltr/app/app_controller.hpp`

Le controller instancie:

- `JsonPolicyRepository`;
- `PolicyService`;
- `JsonQuotaRepository`;
- `QuotaService`.

Puis injecte seulement l'abstraction `TransferQuota` dans:

- `TransferClient`;
- `TransferServer`;
- `WebService`.

### 4.5 Intégration réseau

Modifications:

- `include/ltr/network/transfer_client.hpp`
- `src/network/transfer_client.cpp`
- `include/ltr/network/transfer_server.hpp`
- `src/network/transfer_server.cpp`
- `include/ltr/web/web_service.hpp`
- `src/web/routes/upload_routes.cpp`
- `src/web/routes/download_routes.cpp`

Comportement:

- réservation avant transfert;
- rejet `quota_exceeded` si dépassement;
- commit seulement sur transfert terminé;
- release automatique si échec avant commit.

## 5) Tests ajoutés

Ajout:

- `tests/test_business_policy.cpp`
- `tests/test_quota_service.cpp`
- `tests/test_audit_export_service.cpp`

Modification:

- `tests/CMakeLists.txt`

Validation effectuée:

```bash
cmake --build build --config Release -j
ctest --test-dir build --output-on-failure -j1
```

Résultat:

- build OK;
- 25/25 tests passent.

Note:

- les tests web ouvrent des ports localhost; ils ont été relancés hors sandbox
  et en série pour éviter les collisions de ports.

## 6) État du workspace

Le workspace contenait déjà des changements non liés avant l'implémentation
Business, notamment dans:

- `assets/web/*`;
- `include/ltr/web/web_session_store.hpp`;
- `src/web/web_session_store.cpp`;
- `tests/test_http_smoke.cpp`;
- `tests/test_web_session_store.cpp`;
- `tests/test_web_folder_logic.js`;
- `src/web/routes/history_routes.cpp`;
- `include/ltr/web/routes/history_routes.hpp`;
- `docs-agents/BUSINESS.md`.

Ces changements n'ont pas été revert.

Fichiers créés dans cette conversation:

- `docs-agents/BUSINESS_GROWTH_PLAN.md`;
- `docs-agents/CONVERSATION_PROGRESS.md`;
- `include/ltr/infra/business_policy.hpp`;
- `src/infra/business_policy.cpp`;
- `include/ltr/infra/quota_service.hpp`;
- `src/infra/quota_service.cpp`;
- `include/ltr/infra/audit_export_service.hpp`;
- `src/infra/audit_export_service.cpp`;
- `tests/test_business_policy.cpp`;
- `tests/test_quota_service.cpp`;
- `tests/test_audit_export_service.cpp`.

## 7) Phase 2 demandée mais non commencée

L'utilisateur a demandé de passer à la phase suivante avec les mêmes principes,
puis a immédiatement demandé ce récapitulatif Markdown.

Donc:

- Phase 2 n'est pas encore implémentée;
- aucune modification Phase 2 n'a été faite après cette demande;
- seules des lectures préparatoires des assets web ont été lancées.

## 8) Phase 2 restante — Portail client externe

Objectif:

> Construire un portail de dépôt local sécurisé pour les clients externes.

Fonctionnalités à faire:

- mode "dépôt client" distinct du partage web général;
- lien/QR de dépôt;
- expiration de session;
- formulaire nom déposant;
- consentement;
- dépôt de fichiers via web;
- reçu de dépôt;
- historique filtrable par dépôt;
- intégration quota Business;
- intégration audit;
- messages non techniques.

Architecture recommandée:

- `DepositSessionService`
  - crée et valide les sessions de dépôt;
  - gère expiration;
  - ne connaît pas HTTP.
- `DepositSessionRepository`
  - interface de persistance;
  - implémentation JSON dans `cfgDir`.
- `DepositReceiptService`
  - construit les reçus de dépôt;
  - ne connaît pas l'UI.
- `DepositPolicyService`
  - règles de taille max, expiration, consentement obligatoire;
  - peut lire `BusinessPolicy`.
- Routes web dédiées:
  - `/deposit/:token`;
  - `/api/deposit/session`;
  - `/api/deposit/upload`;
  - `/api/deposit/receipt/:id`.
- UI web dédiée:
  - page ou mode dépôt séparé du dashboard web authentifié.

Principes à garder:

- ne pas mettre la logique dépôt dans `upload_routes.cpp`;
- ne pas faire dépendre le service dépôt de `httplib`;
- ne pas faire dépendre les règles métier de `nlohmann::json`;
- garder les routes comme adaptateurs;
- garder le quota via `TransferQuota`;
- garder l'audit via `TransferHistory` ou un service dédié qui l'enrichit.

## 9) Phase 3 restante — Contrôle IT

À faire après Phase 2:

- appliquer `security.requireHttps`;
- rendre HTTP désactivable ou redirigé vers HTTPS;
- appliquer `network.allowP2P`;
- appliquer `network.allowedCidrs`;
- ajouter rétention configurable;
- ajouter nettoyage automatique des fichiers reçus;
- verrouiller certaines options UI si policy active;
- distinguer clairement erreur réseau, quota et policy.

## 10) Phase 4 restante — Enterprise

À faire plus tard:

- licence signée vérifiée localement;
- tolérance offline 30 jours;
- mode dégradé;
- déploiement silencieux;
- import config centralisée;
- logs SIEM ou webhooks;
- journalisation changements de policies;
- option relay P2P audit-able si besoin confirmé;
- SSO en dernier, pas comme prérequis.

## 11) Phase 5 restante — Growth grand public

À faire:

- garder Personal Free sans quota;
- améliorer onboarding QR;
- clarifier "AirDrop cross-platform";
- pages marketing simples;
- tutoriels courts;
- ne pas ajouter de friction compte/paiement au core gratuit.

## 12) Points d'attention pour la suite

- Le quota Business est désactivé par défaut; pour tester le blocage, il faut
  créer ou modifier `business_policy.json`.
- Les transferts P2P web ↔ web ne sont pas comptés tant que les données ne
  transitent pas par le host.
- Le TCP natif n'est pas encore chiffré bout en bout; ne pas le promettre.
- La Phase 2 doit éviter de transformer le dashboard web existant en page
  métier confuse: un portail de dépôt doit être séparé et simple.
- Les changements existants non liés dans le workspace doivent être préservés.
