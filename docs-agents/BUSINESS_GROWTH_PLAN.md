# BUSINESS_GROWTH_PLAN.md — Roadmap business + marketing

Objectif: transformer LocalTransfer en produit gratuit grand public et en
offre payante entreprise, sans casser la promesse de base: transfert local,
simple, sans cloud et sans compte.

Ce document complète `docs-agents/BUSINESS.md`. Il sert de plan produit,
marketing et go-to-market pour prioriser les fonctionnalités monétisables.

## 1) Positionnement central

LocalTransfer doit être positionné comme:

> Transfert local sécurisé sans cloud, contrôlable par l'IT, avec audit et
> diagnostic réseau clair.

La promesse n'est pas seulement "envoyer un fichier". La valeur payante est:

- garder les fichiers sur le réseau local autant que possible;
- offrir un chemin simple aux utilisateurs externes sans compte;
- donner à l'IT des politiques, des quotas et de la visibilité;
- produire une preuve exploitable de transfert ou de dépôt;
- diagnostiquer les blocages réseau au lieu de laisser l'utilisateur deviner.

Message principal:

> Envoyez de gros fichiers localement, sans cloud, avec preuve et contrôle.

## 2) État réel du produit à exploiter

LocalTransfer possède déjà des briques différenciantes:

- application desktop C++17 / SFML macOS et Windows;
- découverte LAN UDP + transfert TCP natif;
- interface web embarquée accessible par URL/QR;
- HTTP 45456 et HTTPS 45457 avec certificat auto-signé;
- PIN d'appairage et sessions web persistantes optionnelles en HTTPS;
- transferts host ↔ navigateur via HTTP(S);
- transferts navigateur ↔ navigateur via WebRTC DataChannel;
- tickets de download, streaming ZIP, Range requests et retries partiels;
- historique host persistant dans `cfgDir/transfer_history.json`;
- route web `/api/host-history` exposant l'historique host;
- historique de pairs dans `cfgDir/peers_history.json`;
- diagnostics partiels côté P2P web: timeouts, absence de route LAN, Wi-Fi
  perdu, récepteur muet, taille invalide;
- TOFU et empreintes pour HTTPS, TCP et P2P selon les flux.

Limites importantes à ne pas survendre:

- pas de chiffrement bout en bout des chunks TCP natifs;
- pas de vraie reprise bout en bout uniforme;
- pas encore de page historique desktop complète;
- pas encore d'export audit CSV/JSON packagé côté produit;
- pas encore de quotas, policies entreprise ou licence;
- les transferts P2P web ↔ web ne sont pas loggés dans `transfer_history.json`
  car le host ne voit pas les données.

## 3) Packaging commercial

### Personal Free

Public: particuliers, indépendants, usage ponctuel.

Inclut:

- transferts LAN natifs;
- interface web locale par URL/QR;
- host ↔ web et web ↔ web;
- aucun compte, aucun quota commercial;
- pas de SLA ni d'admin centralisée.

Rôle stratégique: adoption, bouche-à-oreille, preuve que le produit remplace
AirDrop dans un environnement cross-platform.

### Business

Public: petites organisations avec besoin de contrôle simple.

Inclut:

- licence par host;
- quota inclus `500 Go / mois / host`;
- historique host lisible;
- export audit CSV/JSON;
- policies simples: HTTPS obligatoire, P2P autorisé ou désactivé, réseaux
  autorisés, rétention historique, nettoyage automatique;
- messages quota et erreurs réseau compréhensibles;
- support standard.

Rôle stratégique: première offre vendable, peu complexe à déployer.

### Business Plus

Public: équipes qui reçoivent régulièrement des fichiers externes.

Inclut Business, plus:

- portail de dépôt local sécurisé;
- lien/QR de dépôt;
- branding léger de l'organisation;
- consentement et nom du client/déposant;
- expiration de session;
- reçu de dépôt;
- historique par dépôt;
- quota et nettoyage adaptés aux dépôts entrants.

Rôle stratégique: monétiser un problème métier concret, plus fort que le
simple transfert poste à poste.

### Enterprise

Public: organisations avec IT, conformité et déploiement contrôlé.

Inclut Business Plus, plus:

- licence signée offline avec tolérance de 30 jours;
- déploiement silencieux MSI/PKG et configuration centralisée;
- policies verrouillées: HTTPS obligatoire, P2P désactivable, réseaux
  autorisés, rétention, nettoyage, ports;
- logs SIEM ou webhooks;
- support prioritaire et version stable;
- option future de relay P2P via host pour rendre certains flux P2P auditables
  et comptables;
- SSO plus tard, après validation commerciale.

## 4) Problèmes clients ciblés

### Shadow IT

Les utilisateurs contournent l'IT avec WeTransfer, Drive, Dropbox, clés USB ou
messageries personnelles.

Réponse produit: transfert local, pas de cloud imposé, policies IT et audit.

### Gros fichiers

Les pièces jointes échouent, les dossiers sont lourds, les solutions MFT sont
surdimensionnées.

Réponse produit: LAN direct, web local, dossiers, ZIP streamé, progression et
reprise partielle.

### Clients externes sans compte

Un cabinet, une imprimerie ou une école doit recevoir des fichiers de personnes
non équipées.

Réponse produit: portail de dépôt local par QR/lien, PIN/session, nom du
déposant, reçu.

### Besoin d'audit

L'organisation doit prouver qu'un fichier a été reçu ou envoyé, avec date,
statut, pair et taille.

Réponse produit: `transfer_history.json`, page historique, export CSV/JSON,
preuve de dépôt.

### Réseaux bloqués

Wi-Fi invité, firewall, proxies, ports fermés et contraintes browser rendent les
transferts LAN difficiles à comprendre.

Réponse produit: diagnostic réseau lisible, distinction policy/quota/réseau,
fallbacks host ↔ web.

### Conformité

Certaines équipes doivent limiter les services cloud et maîtriser la rétention.

Réponse produit: pas de cloud, HTTPS local, historique, rétention et nettoyage
configurables, policies.

## 5) Segments prioritaires

### Cabinets médicaux et paramédicaux

Promesse: recevoir des documents localement sans envoyer les patients vers une
plateforme cloud.

À vendre:

- portail de dépôt en salle ou à l'accueil;
- preuve de dépôt;
- nettoyage automatique;
- message centré confidentialité.

### Avocats, notaires, experts comptables

Promesse: recevoir et transmettre des pièces avec preuve, sans complexité MFT.

À vendre:

- historique exportable;
- reçu de dépôt;
- expiration de session;
- nom du client et consentement.

### Studios photo, vidéo, agences créatives

Promesse: déplacer de gros dossiers localement plus vite qu'un upload cloud.

À vendre:

- gros fichiers et dossiers;
- web sans installation pour clients et freelances;
- quota Business lisible;
- branding léger.

### Imprimeries et reprographie

Promesse: dépôt rapide en boutique ou en atelier, sans clé USB.

À vendre:

- QR code sur comptoir;
- portail de dépôt;
- statut clair;
- nettoyage automatique après délai.

### Écoles et organismes de formation

Promesse: collecter des travaux ou distribuer des fichiers sur un réseau local,
sans compte élève.

À vendre:

- QR/lien de dépôt;
- noms des déposants;
- historique;
- quota par host.

### PME avec IT léger

Promesse: alternative simple aux solutions MFT lourdes.

À vendre:

- déploiement facile;
- policies simples;
- logs exportables;
- support.

## 6) Produit phare à construire

Le produit phare payant doit être:

> Portail de dépôt local sécurisé.

Ce portail transforme LocalTransfer d'un utilitaire de transfert en outil métier
identifiable. Il doit permettre à un visiteur externe, sur le même LAN ou Wi-Fi
invité si les règles réseau le permettent, de déposer des fichiers vers un host
autorisé.

Fonctions clés:

- lien et QR de dépôt;
- page web de dépôt avec nom du déposant;
- consentement configurable;
- branding léger: nom, couleur, logo local;
- expiration de session;
- acceptation/refus côté host selon policy;
- historique du dépôt;
- reçu avec date, taille, nombre de fichiers et statut;
- quota Business compté sur le host;
- nettoyage automatique selon rétention.

Critère de succès: un cabinet ou une imprimerie doit pouvoir dire "scannez ce
QR et déposez vos fichiers" sans expliquer P2P, WebRTC ou ports.

## 7) Roadmap d'amélioration

### Phase 1 — Offre Business vendable

But: rendre l'offre Business facturable avec peu de surface nouvelle.

À construire:

- compteur mensuel host `500 Go / mois / host`;
- comptage TCP + HTTP host;
- reset mensuel UTC;
- mode `hard-block` par défaut si quota dépassé;
- messages quota dans UI desktop et web;
- page historique host exploitable;
- export audit CSV/JSON depuis `transfer_history.json`;
- policies simples dans un fichier de configuration host;
- rétention et nettoyage automatique basiques;
- documentation commerciale "Business".

Données à utiliser:

- `cfgDir/transfer_history.json`;
- `TransferHistory::Kind`: `tcp-out`, `tcp-in`, `http-up`, `http-down`;
- `TransferHistory::Status`: `pending`, `ok`, `failed`, `cancelled`;
- `/api/host-history` comme base web;
- `peers_history.json` pour enrichir l'identification des pairs.

À ne pas inclure en Phase 1:

- SSO;
- relay P2P;
- licensing complexe;
- chiffrement bout en bout TCP.

### Phase 2 — Portail client externe

But: construire le cas d'usage premium le plus vendable.

À construire:

- mode "dépôt client" distinct du partage web général;
- QR/lien de dépôt avec expiration;
- formulaire nom client, société/email optionnels, consentement;
- branding léger;
- reçu de dépôt téléchargeable ou affichable;
- historique filtrable par dépôt;
- limites par session: taille max, nombre de fichiers, expiration;
- choix du dossier de réception côté host;
- libellés simples pour client non technique.

Point d'attention: garder le vocabulaire utilisateur autour de "dépôt",
"reçu", "expiration" et "sécurité locale", pas autour de WebRTC ou P2P.

### Phase 3 — Contrôle IT

But: rendre Business Plus acceptable pour un responsable IT.

À construire:

- fichier policies centralisé dans `cfgDir`;
- HTTPS obligatoire;
- HTTP désactivable ou redirection vers HTTPS;
- P2P autorisé/désactivé par policy;
- réseaux autorisés;
- ports configurables si déjà compatible avec l'architecture;
- rétention historique configurable;
- nettoyage automatique des fichiers reçus selon règle;
- verrouillage d'options UI si policy active;
- diagnostic "bloqué par policy" distinct d'une erreur réseau.

Clés de config à prévoir:

- `plan`;
- `licenseKey`;
- `quota.monthlyBytes`;
- `quota.resetMode`;
- `quota.enforcement`;
- `security.requireHttps`;
- `network.allowP2P`;
- `network.allowedCidrs`;
- `retention.historyDays`;
- `retention.receivedFilesDays`;
- `branding.organizationName`;
- `branding.logoPath`.

### Phase 4 — Enterprise

But: vendre aux organisations qui demandent déploiement, audit avancé et
fonctionnement offline.

À construire:

- clé de licence signée vérifiée localement au démarrage;
- licence par host avec tolérance offline de 30 jours;
- mode dégradé si licence invalide ou expirée;
- installation silencieuse;
- import de configuration centralisée;
- export logs SIEM/webhooks;
- journalisation des changements de policies;
- support de flotte;
- option relay P2P audit-able si besoin commercial confirmé.

SSO est à garder pour plus tard. Il ne doit pas bloquer la première offre
Enterprise si le produit se vend déjà sur le dépôt local, les policies et les
logs.

### Phase 5 — Growth grand public

But: augmenter l'adoption sans diluer l'offre payante.

À faire:

- garder Personal Free sans quota;
- améliorer onboarding QR;
- clarifier "AirDrop cross-platform";
- pages marketing simples;
- tutoriels courts par usage;
- friction minimale: pas de compte, pas de paiement, pas de mur artificiel sur
  les transferts core.

## 8) Règles de quota Business

Unité: quota appliqué au host, car le host est le point de contrôle naturel.

Quota recommandé: `500 Go / mois / host`.

Flux comptés:

- TCP natif host ↔ peer;
- HTTP(S) host ↔ navigateur;
- uploads web vers host;
- downloads host vers web.

Flux non comptés par défaut:

- P2P pur navigateur ↔ navigateur, tant que les données ne transitent pas par
  le host.

Reset:

- mensuel calendaire en UTC;
- plus simple à expliquer qu'un rolling 30 jours.

Comportement:

- `hard-block` par défaut en Business;
- `soft-throttle` réservé Enterprise;
- messages explicites avec quota utilisé, quota total et date de reset.

Libellés recommandés:

- "Quota mensuel atteint (500 Go). Reset le 1er du mois (UTC)."
- "Transfert bloqué par la politique entreprise."
- "Le quota Business compte les transferts qui passent par ce poste."

## 9) Audit et preuve

Base actuelle:

- `TransferHistory` stocke session, pair, type, nombre de fichiers, taille,
  statut, début, fin et erreur;
- le fichier persistant est `cfgDir/transfer_history.json`;
- la route `/api/host-history` expose ces données aux sessions web
  authentifiées.

À ajouter pour Business:

- export CSV;
- export JSON stable;
- filtres par date, statut, pair et sens;
- résumé mensuel pour quota;
- reçu de dépôt;
- indication claire si un flux P2P pur n'est pas audité côté host;
- conservation de l'erreur finale pour distinguer refus, annulation, réseau,
  quota et policy.

Champs minimum pour l'export:

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

## 10) Licensing

Recommandation:

- licence par host pour Business et Business Plus;
- licence signée locale pour Enterprise;
- vérification au démarrage;
- stockage dans `cfgDir`;
- tolérance offline de 30 jours;
- mode dégradé si licence invalide.

Mode dégradé proposé:

- Personal Free reste utilisable;
- fonctionnalités Business désactivées ou en lecture seule;
- export audit et policies verrouillées désactivés;
- message clair à l'admin.

À journaliser:

- activation;
- expiration;
- changement de plan;
- changement de policy;
- dépassement quota;
- retour en mode dégradé.

## 11) Angles marketing

### Message principal

Envoyez de gros fichiers localement, sans cloud, avec preuve et contrôle.

### Santé

Recevez des documents patients sur votre réseau local, sans compte externe et
avec une trace de dépôt.

### Juridique

Collectez les pièces clients avec date, statut et reçu, sans imposer une
plateforme lourde.

### Créatifs

Déplacez de gros dossiers sur le LAN, avec une page web simple pour les clients
et freelances.

### Imprimeries

Remplacez la clé USB au comptoir par un QR de dépôt local.

### Écoles

Collectez les travaux d'une salle ou distribuez des fichiers sans créer de
comptes.

### PME

Ajoutez du contrôle IT et de l'audit sans déployer une solution MFT complexe.

## 12) Pages marketing à prévoir

Pages prioritaires:

- page produit: transfert local sans cloud;
- page Business: quotas, audit, policies, support;
- page Portail de dépôt: QR, dépôt client, reçu, expiration;
- pages métiers: santé, juridique, créatifs, imprimerie, école, PME;
- FAQ sécurité;
- comparaison "AirDrop cross-platform";
- page déploiement IT.

Preuves à afficher:

- aucun cloud requis;
- transfert LAN;
- HTTPS local;
- historique host;
- audit exportable;
- quota maîtrisé;
- diagnostic réseau clair;
- P2P web ↔ web disponible, avec explication honnête de l'audit.

Copywriting à éviter en premier niveau:

- "P2P";
- "WebRTC";
- "DataChannel";
- "TOFU";
- "certificat auto-signé".

Copywriting recommandé:

- dépôt sécurisé;
- transfert local;
- gros fichiers;
- sans cloud;
- contrôle IT;
- preuve de transfert;
- quota mensuel;
- diagnostic réseau.

## 13) Décisions produit à garder stables

- Personal Free reste sans quota commercial.
- Le quota Business est par host.
- Business compte TCP + HTTP(S) qui passent par le host.
- Le P2P pur n'est pas compté tant qu'il ne transite pas par le host.
- `hard-block` est le comportement Business par défaut.
- `soft-throttle` et relay P2P sont Enterprise.
- Les pages marketing ne doivent pas promettre un chiffrement bout en bout TCP
  tant qu'il n'est pas livré.
- Les fonctions compliance doivent être décrites comme audit, rétention,
  contrôle et absence de cloud, pas comme certification réglementaire.

## 14) Critères de validation des futurs travaux

Avant de livrer une feature Business:

- vérifier que les flux TCP et HTTP(S) sont correctement comptés;
- vérifier que le quota ne compte pas le P2P pur;
- vérifier que le reset mensuel UTC est testable;
- vérifier que les erreurs quota/policy/réseau sont distinguées;
- vérifier que l'export audit reprend les champs de `transfer_history.json`;
- vérifier que les docs ne promettent pas une capacité encore hors périmètre;
- vérifier que Personal Free n'est pas dégradé artificiellement.

Avant de livrer le portail de dépôt:

- tester dépôt mobile par QR;
- tester expiration de session;
- tester nom du déposant et consentement;
- tester reçu;
- tester acceptation/refus côté host;
- tester nettoyage automatique;
- vérifier que le langage ne demande aucune connaissance technique au client.

## 15) Prochaine étape recommandée

La meilleure prochaine étape produit est Phase 1:

1. ajouter un service de quota host mensuel;
2. brancher le comptage sur les événements TCP et HTTP(S);
3. exposer une page historique host exploitable;
4. ajouter export CSV/JSON;
5. documenter l'offre Business;
6. seulement ensuite construire le portail de dépôt Business Plus.

Cette séquence transforme les briques déjà présentes en offre vendable avant de
lancer un chantier plus visible mais plus large.
