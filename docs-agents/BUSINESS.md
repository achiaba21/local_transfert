# BUSINESS.md — Stratégie de monétisation (Entreprise)

Objectif: conserver une application simple et gratuite pour le grand public,
et monétiser la valeur "IT + conformité + contrôle + support" côté entreprise.

Ce document décrit:
- les offres commerciales,
- ce que l'entreprise achète concrètement,
- une proposition de quota `500 Go / mois / host`,
- des choix d'implémentation et de UX associés.

## 1) Offres (packaging)

### Personal (Free)
- Usage grand public: transferts LAN/Web/P2P "core".
- Zéro paiement, zéro création de compte.
- Pas de promesse SLA.

But: maximiser l'adoption et la recommandation.

### Business (Paid)
Monétisation principale: licence entreprise.

Inclut:
- fonctionnalités IT/admin (policies, contrôle, logs),
- traçabilité/audit,
- mode déploiement,
- support,
- quota inclus `500 Go / mois / host` (cf. section 3).

### Enterprise (Paid+)
Pour les organisations plus exigeantes:
- SSO (SAML/OIDC), contrôle avancé,
- audit avancé (export, SIEM), politiques fines,
- SLA/support renforcé,
- options de volume licensing (par site, par flotte, etc).

## 2) Ce que l'entreprise paie (valeur)

Les entreprises paient rarement pour "envoyer un fichier". Elles paient pour:

### Controle IT / politiques
- Forcer HTTPS/TLS et les contraintes de sécurité.
- Paramétrer ports, réseaux autorisés, modes autorisés (ex: désactiver P2P).
- Verrouiller certaines options utilisateur.
- Mode kiosque / poste partagé (politiques d'effacement).

### Traçabilité et conformité
- Historique clair: qui a transféré quoi, quand, taille, statut.
- Audit log exportable (JSON/CSV) et rétention configurable.
- Nettoyage automatique (politiques) + indicateur d'espace utilisé.

### Déploiement et exploitation
- Installation silencieuse / packaging (MSI/PKG), configuration centralisée.
- Version LTS, notes de version, compatibilité stable.
- Monitoring basique: santé service web, diagnostics réseau.

### Support
- Canal support entreprise (SLA), priorisation bugfix.

## 3) Quota `500 Go / mois` (recommandation)

### Unité de quota
Recommandation: quota appliqué au `host` (poste desktop) car c'est:
- le point de contrôle naturel,
- facile a appliquer sans "compte" utilisateur.

Définition: total d'octets effectivement envoyés/recus par le host.

Note: les transferts `browser ↔ browser` en P2P ne transitent pas par le host,
donc ne sont pas comptables au niveau host dans l'architecture actuelle.

Choix produit recommandé:
- Le quota Business compte `TCP + HTTP` (host ↔ peer, host ↔ web).
- Le P2P pur (web ↔ web) n'est pas compté par défaut (cohérent avec "par host").

Option future (Enterprise): "P2P relay via host" (les donnees passent par le host)
pour rendre le P2P comptable + audit-able, mais c'est un chantier.

### Calendrier de reset
Deux options:
- Mensuel calendaire (recommandé pour entreprise): reset au 1er du mois (UTC).
- Rolling 30 jours: plus complexe a expliquer, plus "grand public".

Recommandation Business: mensuel calendaire en UTC.

### Comportement au dépassement
Prévoir un comportement configurable par policy:
- `hard-block`: refuser les nouveaux transferts (message clair).
- `soft-throttle`: autoriser mais limiter le débit et/ou la taille de file.

Recommandation par défaut: `hard-block` pour éviter les surprises de coûts/reseau,
avec option `soft-throttle` en Enterprise.

### UX (messages)
Quand quota atteint:
- afficher le quota et la date de reset,
- distinguer "bloqué par policy" vs "erreur reseau".

Exemples de libellés:
- "Quota mensuel atteint (500 Go). Reset le 1er du mois (UTC)."
- "Transfert bloque par la politique entreprise."

## 4) Licensing (simple et robuste)

### Métrique de facturation
Recommandation de démarrage: licence `par host` (poste).

Alternative: licence `par site` (plus simple pour grandes boites, plus cher).

### Activation
Objectifs:
- fonctionner offline (en entreprise, pas toujours d'acces internet),
- être stable et supportable.

Recommandation:
- cle de licence signée (verification locale),
- tolerance offline (ex: 30 jours) avant mode degrade.

### Politiques et preuve
- Stocker l'etat licence + policies dans le `cfgDir` du host.
- Journaliser les changements de licence/policy dans l'audit log.

## 5) Fonctionnalites Business/Enterprise (liste)

### Business (priorite)
- Policies: TLS/HTTPS force, ports, desactivation P2P, reseaux autorises.
- Historique/audit exportable (CSV/JSON).
- Retention + auto-nettoyage configurable.
- Alias par device persistés côté host (utile pour identification).
- Diagnostics reseau: expliquer pourquoi P2P/connexion echoue.
- Support + version stable.

### Enterprise (plus)
- SSO (SAML/OIDC).
- Role-based access (admin vs utilisateur).
- Export SIEM, webhook events.
- Mode "relay" (option) pour audit/quotas sur P2P.
- Deploiement/MDM/packaging avance.

## 6) Positionnement (message)

Message principal entreprise:
- "Transfert local, pas de cloud."
- "Controle IT + traçabilité."
- "Conformite et support."

Ne pas casser le gratuit grand public:
- pas de quota sur Personal,
- pas de friction compte,
- garder l'app utilisable et fiable sans paiement.

