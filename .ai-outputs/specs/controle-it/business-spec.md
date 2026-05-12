# Spécification Métier — Phase 3 : Contrôle IT

> Validée par l'utilisateur le 2026-05-12. Phase suivante : Architecture.

## 1. Contexte

Phase 1 a introduit `BusinessPolicy` avec 5 champs définis mais NON
appliqués au runtime : `security.requireHttps`, `network.allowP2P`,
`network.allowedCidrs`, `retention.historyDays`,
`retention.receivedFilesDays`. Phase 3 = transformer ces champs en
comportements réels au runtime, pour qu'un IT puisse durcir
LocalTransfer sur un parc d'entreprise.

## 2. Objectif

Permettre à un service IT (interne ou prestataire) de **distribuer un
fichier `business_policy.json` sur un parc** et d'obtenir
automatiquement les comportements suivants sur chaque poste :

- Trafic web forcé en HTTPS (HTTP renvoie 301 redirect).
- Mode P2P web↔web désactivable.
- Filtrage des IPs clientes via whitelist CIDR.
- Purge automatique de l'historique et des fichiers reçus sur durée.
- Verrouillage de certaines bascules UI quand la policy le décide.

## 3. Décisions validées

| Question | Choix |
|---|---|
| Mode `requireHttps` | **Redirect 301 HTTP→HTTPS** (les 2 ports restent ouverts ; le port HTTP ne sert que 301) |
| Distribution policy | **Fichier `business_policy.json` dans `configDir`** (MDM/GPO/script à la discrétion de l'IT) |
| `allowedCidrs` | **Whitelist exclusive** : liste vide = tout autorisé ; liste non vide = SEULES les IPs matchant un CIDR sont acceptées (sinon 403) |
| `receivedFilesDays` défaut | **0 = pas de purge** (conservateur, opt-in) |

## 4. Règles Métier

### 4.1 HTTPS forcé (`security.requireHttps`)

- Quand `true` ET HTTPS actif :
  - Le serveur HTTP 45456 ne sert plus AUCUNE route métier ; il
    renvoie systématiquement **301 Moved Permanently** vers la même
    URL en HTTPS sur le port 45457.
  - Les routes `/health`, `/login` etc. sont elles aussi
    redirigées.
- Quand `true` MAIS HTTPS indisponible (cert manquant / erreur) :
  - Le démarrage du serveur s'arrête en erreur explicite côté
    desktop (log + bandeau UI). On n'autorise PAS un fallback HTTP
    silencieux qui contournerait la policy.
- Quand `false` (défaut) : comportement actuel inchangé.

### 4.2 P2P désactivable (`network.allowP2P`)

- Quand `false` :
  - Toutes les routes `/api/p2p/*` retournent **403 Forbidden** avec
    `{"error":"p2p_disabled","reason":"policy"}`.
  - Côté UI web (dashboard) : les onglets/sections P2P sont
    grisés/masqués avec un libellé non technique
    « Désactivé par votre administrateur ».
  - Le SSE `web-peers` est désactivé pour éviter que les navigateurs
    se voient même sans pouvoir s'envoyer de fichiers.
- Quand `true` (défaut) : comportement actuel inchangé.

### 4.3 Whitelist IP (`network.allowedCidrs`)

- Format accepté : `"192.168.1.0/24"`, `"10.0.0.0/8"`,
  `"172.16.0.0/12"`, `"::1/128"`, `"192.168.1.42"` (host sans CIDR
  = `/32`).
- Si la liste est **vide** : tout client web est autorisé (default
  Personal Free).
- Si la liste est **non vide** : un middleware HTTP examine l'IP
  client (`X-Forwarded-For` ignoré — on prend l'IP socket directe
  pour éviter le spoofing). Si l'IP ne matche AUCUN CIDR de la
  liste, retour **403 Forbidden** avec
  `{"error":"ip_blocked","reason":"policy"}`.
- L'admin dashboard est concerné par le filtre. Les routes
  `/health` ne sont jamais filtrées (sinon impossible de checker
  le service depuis un superviseur externe).
- Les loggings du refus sont en `warn` avec l'IP source pour
  permettre à l'IT de diagnostiquer.

### 4.4 Rétention historique (`retention.historyDays`)

- Défaut : `180` (déjà dans Phase 1).
- À chaque démarrage de l'app : purger les entrées de
  `TransferHistory`, `DepositHistory`, `PeersHistory` dont
  `finishedAt < now - historyDays * 86400`.
- Hot-purge : ré-évalué toutes les 24 h via un thread léger.

### 4.5 Nettoyage fichiers reçus (`retention.receivedFilesDays`)

- Défaut : `0` (= pas de purge).
- Si > 0 : à chaque démarrage + toutes les 24 h, scanner
  `downloadDir` et `downloadDir/Deposits/*` et supprimer les
  fichiers dont la date de modification est antérieure à
  `now - receivedFilesDays * 86400`.
- Les dossiers vides sont également supprimés.
- Un log structuré récapitule à chaque purge le nombre de fichiers
  supprimés et l'espace libéré.

### 4.6 Verrouillage UI

- Si `requireHttps = true` : le toggle « activer/désactiver HTTPS »
  desktop (s'il existe) est grisé avec tooltip « Verrouillé par
  votre administrateur ».
- Si `allowP2P = false` : les onglets P2P web sont masqués.
- Si `historyDays` ou `receivedFilesDays` sont définis par la
  policy : les contrôles UI correspondants (s'il y en a) sont
  affichés en lecture seule.

### 4.7 Distinction des erreurs (`ErrorCategory`)

- Nouvelle valeur dans `ErrorCategory` :
  `PolicyDenied = 5`.
- Utilisée par les routes Phase 3 quand un refus vient d'une
  policy IT (pas d'un quota mensuel ni d'une erreur réseau).
- Côté UI desktop : message non technique
  « Refusé par votre administrateur. »

## 5. Acteurs

| Acteur | Rôle |
|---|---|
| **Admin IT** | Édite `business_policy.json` et le distribue (MDM/GPO/script). Ne touche jamais à l'app. |
| **Host** | Utilise l'app normalement, voit les restrictions sans pouvoir les contourner via l'UI. |
| **Déposant** | Reçoit éventuellement un 403 si IP hors whitelist, avec message non technique côté navigateur. |

## 6. Cas d'Usage Principal

1. Un IT distribue à 50 postes le fichier
   `~/Library/Application Support/LocalTransfer/business_policy.json`
   contenant :
   ```json
   {
     "plan": "business",
     "security": { "requireHttps": true },
     "network": {
       "allowP2P": false,
       "allowedCidrs": ["10.0.0.0/8", "192.168.1.0/24"]
     },
     "retention": { "historyDays": 90, "receivedFilesDays": 30 }
   }
   ```
2. Au prochain démarrage des apps :
   - HTTP redirige tout vers HTTPS,
   - Les routes P2P refusent toute requête,
   - Seules les IPs internes sont acceptées,
   - L'historique > 90 jours est purgé,
   - Les fichiers reçus > 30 jours sont supprimés.
3. L'IT consulte le journal d'audit via l'endpoint
   `/api/host-history/export` pour montrer la conformité.

## 7. Cas Alternatifs / Limites

| Cas | Comportement attendu |
|---|---|
| `business_policy.json` corrompu / illisible | Log warning, fallback PersonalFree (aucune restriction) |
| Policy changée à chaud (édition fichier pendant runtime) | Pas hot-reload Phase 3 — redémarrage de l'app requis (à clarifier en Phase 4 Enterprise) |
| HTTPS impossible (port occupé) + requireHttps=true | App refuse de démarrer avec message clair |
| Whitelist exclut localhost | Tout casse, dont le dashboard host. Le code ajoute automatiquement `127.0.0.1` et `::1` pour éviter le brick |
| Personal Free avec policy custom | Personal Free peut avoir une policy mais aucune fonction Business ne s'active (cf. Phase 2) |

## 8. Contraintes

- **Pas de dépendance externe nouvelle.**
- **Pas de hot-reload** : changement de policy = relancer l'app.
- **`127.0.0.1` et `::1` toujours autorisés** quoi qu'il arrive
  (sinon l'app se brick elle-même côté dashboard).
- **Aucune fuite côté déposant** : messages génériques (« Service
  indisponible. Contactez votre administrateur. »).
- **Header `X-Forwarded-For` ignoré** pour la whitelist : on prend
  l'IP socket directe.

## 9. Critères d'Acceptation

- [ ] `business_policy.json` avec `requireHttps:true` → toute
  requête HTTP renvoie 301 vers HTTPS.
- [ ] `allowP2P:false` → `/api/p2p/*` retourne 403 ; section P2P
  web masquée ; SSE web-peers inactif.
- [ ] `allowedCidrs:["10.0.0.0/8"]` → IP `192.168.1.5` reçoit 403.
- [ ] `allowedCidrs:["10.0.0.0/8"]` → IP `127.0.0.1` est TOUJOURS
  autorisée (override hard-coded).
- [ ] `historyDays:30` → au démarrage, les entrées
  `TransferHistory` plus vieilles que 30 jours sont supprimées.
- [ ] `receivedFilesDays:30` → au démarrage, les fichiers dans
  `downloadDir` plus vieux que 30 jours sont supprimés.
- [ ] `receivedFilesDays:0` (défaut) → aucune purge.
- [ ] `ErrorCategory::PolicyDenied` ajouté et utilisé par les
  routes Phase 3.
- [ ] Tests unitaires couvrent : parsing CIDR, matching IP,
  whitelist exclusive, override localhost, comportement avec
  policy vide.
