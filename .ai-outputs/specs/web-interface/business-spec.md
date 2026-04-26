# 📋 Spécification Métier — Interface Web LocalTransfer

> **Feature** : web-interface
> **Statut** : ✅ Validée par l'utilisateur
> **Date** : 2026-04-22
> **Mode** : Feature Complète (full)

---

## 1. Contexte

LocalTransfer permet aujourd'hui le transfert de fichiers entre apps natives installées (Mac ↔ Windows). Les pare-feux OS bloquent fréquemment les connexions entrantes, empêchant la découverte bidirectionnelle et bloquant les transferts. De plus, l'installation de l'app native freine l'adoption : un utilisateur qui veut ponctuellement recevoir un fichier ne va pas installer une app pour cela.

## 2. Objectif

Permettre à **n'importe quel appareil du réseau local** (phone, tablette, laptop, peu importe l'OS) d'envoyer ET recevoir des fichiers avec un host LocalTransfer, **sans installation**, simplement en ouvrant une URL dans son navigateur. Les visiteurs web apparaissent dans l'app desktop comme des pairs normaux, dans la **même liste unifiée** que les pairs natifs.

## 3. Acteurs

- **Host** : utilisateur qui fait tourner LocalTransfer desktop. Expose une interface web à son réseau local.
- **Visiteur web** : personne sur le LAN, quel que soit son OS/device, qui ouvre l'URL du host dans son navigateur. Aucun logiciel à installer.
- **Pair natif** : autre utilisateur ayant LocalTransfer desktop installé (chemin existant, inchangé).

## 4. Règles Métier

- **R1. Bidirectionnalité** : un visiteur web peut envoyer des fichiers au host ET télécharger des fichiers que le host met à disposition. L'expérience web reproduit les fonctions principales de l'interface desktop native.
- **R2. Serveur web toujours actif** : tant que l'app desktop tourne, l'URL d'accès web est joignable. Pas d'interrupteur dédié en V1.
- **R3. Authentification par PIN — premier contact uniquement** : au premier accès d'un navigateur à l'URL, le visiteur doit saisir le PIN 6 chiffres. Une fois validé, le device est mémorisé (via cookie/identifiant local) et n'a plus à saisir le PIN tant que la mémoire n'est pas effacée. Le PIN est le **même** que celui utilisé côté app native (source de vérité unique).
- **R4. Liste de pairs unifiée côté host** : l'app desktop affiche dans sa liste de pairs à la fois les pairs natifs (découverte LAN) et les sessions web actives. Le host peut envoyer des fichiers à n'importe lequel, de façon transparente. La représentation visuelle indique subtilement le type (ex. icône différenciée natif/web).
- **R5. Coexistence native ↔ web** : lorsque deux utilisateurs ont tous les deux l'app native et sont visibles sur le LAN, ils peuvent choisir le chemin natif (plus rapide) ou le chemin web. Les deux restent disponibles.
- **R6. Auto-propagation de l'app native** : la page web détecte l'OS du visiteur. Si le visiteur est sur le **même OS** que le host, un bandeau/bouton propose le téléchargement de l'app native (le host sert sa propre version). Si le visiteur est sur un OS **différent**, un lien pointe vers les releases GitHub. L'utilisateur peut fermer/ignorer le bandeau et continuer avec le flow web.
- **R7. Confiance LAN** : V1 part du principe que le LAN est de confiance (HTTP plain, pas de chiffrement bout-en-bout). Documenté clairement.

## 5. Cas d'Usage Principaux

### UC1 — Visiteur envoie un fichier au host (cas typique)
1. Le visiteur ouvre l'URL `http://<ip-host>:<port>` dans son navigateur (lue au QR code affiché par le host, ou tapée à la main).
2. Premier contact → saisie PIN 6 chiffres. Validation → device mémorisé.
3. Le visiteur dépose un ou plusieurs fichiers dans la page.
4. Le transfert démarre, une barre de progression s'affiche.
5. Côté host, le fichier apparaît dans le dossier de réception ; le visiteur apparaît dans la liste des pairs comme actif.

### UC2 — Host envoie un fichier à un visiteur web
1. Un visiteur a ouvert la page web et validé son PIN → il apparaît dans la liste de pairs côté desktop.
2. Le host le sélectionne dans la liste, choisit un/plusieurs fichiers, clique "Envoyer".
3. Les fichiers apparaissent dans la page web du visiteur, prêts à être téléchargés.
4. Le visiteur clique "Télécharger" → transfert vers son navigateur.

### UC3 — Visiteur installe l'app native (bonus adoption)
1. Le visiteur ouvre la page web. Sa plateforme est détectée.
2. Si même OS que le host, un bandeau propose : "Installer l'app native pour plus de vitesse et la découverte auto".
3. Le visiteur clique, télécharge le binaire servi par le host, installe, rejoint la mesh native.
4. Si OS différent, le bandeau pointe vers GitHub Releases.
5. Le visiteur peut ignorer le bandeau et continuer avec le flow web sans jamais installer.

### UC4 — Host partage son URL
1. Dans l'UI desktop, un QR code et l'URL textuelle sont visibles.
2. Les invités scannent ou copient l'URL pour se connecter.

## 6. Cas Alternatifs / Limites

- **Mauvais PIN** : message d'erreur clair, le visiteur peut réessayer. Pas de lockout en V1 (à confirmer lors d'itérations futures).
- **Visiteur ferme/oublie son onglet** : sa session reste active côté host pendant un délai court (ex. quelques minutes) puis il est retiré de la liste des pairs. Un retour rouvre la page sans re-saisir le PIN (device mémorisé).
- **Fichier trop gros pour le navigateur** (mobile notamment) : message d'avertissement dans la page, suggestion d'installer l'app native.
- **Multi-navigateurs sur un même device** : chaque navigateur est considéré comme une session/device distinct (cookies indépendants).

## 7. Contraintes

- **Aucune régression** sur le flow natif ↔ natif existant (protocole TCP, découverte UDP, tests unitaires).
- **Stack minimaliste** : pas de framework lourd côté page web (vanilla JS + CSS), dépendances C++ header-only uniquement.
- **Documentation obligatoire** : un fichier central `WEB.md` dans `docs-agents/`, ET un `README.md` par nouvelle couche du code source, pour guider les futurs agents et sessions Claude.
- **Exclusions V1** assumées : pas de HTTPS, pas de découverte réseau de l'URL (mDNS), pas d'historique persistant, pas de signature des binaires distribués.
- **Limite de confiance** : le host expose son interface à tout le LAN. La sécurité repose sur le PIN et l'hypothèse LAN de confiance.

## 8. Critères d'Acceptation

- [ ] Un visiteur sur n'importe quel OS peut ouvrir l'URL du host et envoyer un fichier sans installation.
- [ ] Un visiteur peut télécharger des fichiers mis à disposition par le host depuis son navigateur.
- [ ] Le PIN est demandé une seule fois par device ; la session est mémorisée ensuite.
- [ ] Les sessions web apparaissent dans la liste de pairs unifiée côté desktop (différenciées visuellement des pairs natifs).
- [ ] Le host peut envoyer un fichier à un visiteur web depuis l'UI desktop, comme il le ferait avec un pair natif.
- [ ] La page web détecte l'OS du visiteur et propose le téléchargement de l'app native si même OS que le host.
- [ ] Pour un OS différent, un lien vers GitHub Releases est proposé.
- [ ] Le visiteur peut ignorer la proposition d'installation et utiliser le flow web sans friction.
- [ ] Un QR code et l'URL textuelle sont visibles dans l'UI desktop.
- [ ] Le flow natif ↔ natif (TCP LTR1) reste fonctionnel et prioritaire entre deux apps natives.
- [ ] Un fichier `WEB.md` dans `docs-agents/` documente la nouvelle couche web globalement.
- [ ] Un `README.md` par couche technique ajoutée guide les futurs agents.
- [ ] Les tests existants passent toujours après l'ajout.
