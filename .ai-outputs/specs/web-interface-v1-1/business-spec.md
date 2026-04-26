# 📋 Spécification Métier — web-interface V1.1

> **Feature** : web-interface-v1-1
> **Statut** : ✅ Validée par l'utilisateur
> **Date** : 2026-04-22
> **Mode** : Feature Complète (full)
> **Réfère à** : `.ai-outputs/specs/web-interface/business-spec.md` (V1, contexte)

---

## 1. Contexte

La V1 de l'interface web embarquée a été livrée et testée sur mobile (iOS Safari + Android). **9 bugs fonctionnels** sont remontés dont plusieurs rendent l'usage impossible :

1. PIN invisible à la saisie
2. Auto-submit surprise avant le clic « Se connecter »
3. Formulaire de connexion sur la même page que l'interface principale (pas de séparation visuelle)
4. Pair web qui disparaît côté desktop alors que l'onglet est encore ouvert
5. Transferts desktop → web bloqués à 0 % indéfiniment
6. Téléchargement web renvoie un JSON au lieu du fichier
7. Duplication du pair web dans la liste desktop
8. Fichiers ajoutés côté desktop ne sont pas vidés après envoi
9. Upload mobile (iOS + Android) ne fonctionne pas (file picker ne s'ouvre pas)

L'utilisateur demande de **« bien repenser le mécanisme »** — ce n'est pas du replâtrage superficiel, il faut revoir le cycle de vie de la session web et la relation host ↔ visiteur.

## 2. Objectif

Livrer une **V1.1 stable** où :
- Le flow de connexion est propre, lisible, sans auto-submit surprise
- Le pair web reste visible côté desktop **tant que l'onglet du visiteur est ouvert**
- L'envoi desktop → web affiche un **statut pendant l'attente** (pas juste 0 % bloqué)
- Le téléchargement côté web renvoie **le vrai fichier**, pas un JSON d'erreur
- L'upload depuis mobile (iOS + Android) **fonctionne**
- La liste de fichiers à envoyer côté desktop propose une **sélection par case à cocher**

**Hors scope** : aucune nouvelle fonctionnalité, uniquement corrections et repensée du mécanisme.

## 3. Acteurs (inchangé V1)

- **Host** : utilisateur de l'app desktop LocalTransfer
- **Visiteur web** : n'importe qui sur le LAN avec un navigateur (mobile ou desktop)
- **Pair natif** : utilisateur avec app native installée (flow inchangé)

## 4. Règles Métier (V1.1)

### R1 — Session web = durée de vie de l'onglet
Tant que le visiteur garde l'onglet ouvert (même en arrière-plan), il reste connecté. Fermeture de l'onglet ou reboot du téléphone → re-saisie du PIN à la prochaine ouverture.

### R2 — Device stable côté desktop (pas de duplication)
Le navigateur mémorise un `device_id` (UUID) dans son stockage local (`localStorage`) au premier contact. Ce `device_id` est stable pour toutes les sessions suivantes sur ce même navigateur. Côté desktop, un iPhone qui se reconnecte 3 fois apparaît comme **une seule ligne** dans la sidebar, même si les sessions sont distinctes dans le temps.

**Détail de conception** :
- `device_id` = UUID stable (localStorage, survit aux fermetures d'onglet)
- `session_token` = éphémère (cookie, meurt à la fermeture d'onglet via flag Session)
- Le serveur utilise `device_id` pour dédupliquer dans la liste desktop, et `session_token` pour l'authentification courante

### R3 — Écran de connexion dédié
`/login` est une **page HTML séparée** avec uniquement le formulaire PIN. Après PIN validé, redirection HTTP vers `/` (page principale). L'URL visible dans la barre d'adresse distingue les deux états.

### R4 — PIN : pas d'auto-submit
La soumission du PIN se fait **uniquement** via clic sur le bouton « Se connecter » (ou touche Entrée dans le formulaire). Le remplissage automatique des 6 cases **ne déclenche rien tout seul**.

### R5 — PIN : affichage visible
Les chiffres saisis **doivent être visibles** dans les cases (pas de masquage). Taille de police raisonnable pour mobile. L'utilisateur peut relire et vérifier avant de cliquer.

### R6 — Statut « Proposé » côté desktop lors d'un envoi web
Quand le host envoie des fichiers à un visiteur web, chaque transfert passe par 3 états visibles :

| État | Description | Déclencheur |
|---|---|---|
| **Proposé** | Tickets créés, SSE envoyé au navigateur, en attente du clic visiteur. Bouton « Annuler » côté desktop. | Émission de `pushFiles` |
| **En cours** | Stream actif, barre de progression en temps réel | Premier byte streamé au client |
| **Terminé** | Stream complet | EOF atteint |
| **Expiré** | 15 minutes sans clic du visiteur | Timer d'expiration ticket |
| **Annulé** | Host clique « Annuler » | Action utilisateur |

**Expiration du ticket** : 15 min (au lieu de 5 actuellement).

### R7 — Téléchargement web : fichier ou erreur explicite
Cliquer « Télécharger » sur la page web **doit** donner le fichier. Si la session est invalide :
- ❌ **Ne pas répondre avec JSON** dans le stream de téléchargement
- ✅ Gérer la ré-authentification côté client (JS détecte le 401 et redirige vers `/login`)
- ✅ Prévoir une **grâce période** serveur-side pour tolérer les micro-délais pendant la lecture

### R8 — Upload mobile fonctionne
Le visiteur doit pouvoir sélectionner et envoyer des fichiers depuis **iOS Safari** et **Android Chrome** :
- La zone « Déposer ici » doit ouvrir le sélecteur natif au **tap** (pas seulement au drag)
- Un **bouton explicite « Choisir des fichiers »** est ajouté en complément de la drop zone pour clarté
- L'upload utilise des techniques compatibles mobile (form submit ou fetch avec FormData, pas de dépendance à XHR uniquement)

### R9 — Liste fichiers desktop avec cases à cocher
Chaque `FileRow` dans la zone « Fichiers à envoyer » porte une **case à cocher** :
- Cochée par défaut
- L'utilisateur peut décocher ceux qu'il ne veut pas envoyer
- À l'envoi, **seuls les cochés** partent
- Après envoi réussi, les fichiers envoyés **disparaissent automatiquement**
- Les décochés **restent** pour un prochain envoi

### R10 — Bouton « Se déconnecter » côté web
Visible dans le header de la page principale. Clic → :
1. Session serveur invalidée (token supprimé du store)
2. Cookie nettoyé côté navigateur
3. Redirection vers `/login`

**La fermeture d'onglet reste aussi un moyen valide de se déconnecter** (R1 — session = onglet).

---

## 5. Cas d'Usage Principaux

### UC1 — Visiteur se connecte et envoie un fichier (scénario mobile)
1. Visiteur ouvre `http://192.168.1.x:45456` → page `/login`
2. Saisie des 6 chiffres PIN (**chaque chiffre s'affiche visuellement**)
3. Clic « Se connecter » (**rien ne se passe avant le clic**)
4. Si PIN OK → redirection HTTP vers `/` + apparition dans la sidebar desktop
5. Tape sur zone « Envoyer » ou bouton « Choisir des fichiers » → ouverture du sélecteur natif
6. Choix d'un fichier → upload avec progress visible
7. Côté desktop : le transfert passe de « En cours » à « Terminé »

### UC2 — Host envoie un fichier à un visiteur web (scénario pending)
1. Host voit l'iPhone dans sa sidebar (pair web, pill « Web »)
2. Sélectionne 2 fichiers via « Parcourir »
3. **Décoche la case** du 2e fichier
4. Clic « ENVOYER » → transfert créé avec statut **« Proposé — en attente du visiteur »**
5. Visiteur reçoit notification sur la page web (SSE) → 1 fichier dans « Reçus du host »
6. Visiteur clique « Télécharger » → statut desktop passe à **« En cours »** avec progress
7. Stream terminé → statut **« Terminé »** desktop + fichier sauvegardé côté navigateur
8. Côté desktop : le fichier coché + envoyé **disparaît** de la liste ; le décoché **reste**

### UC3 — Visiteur en arrière-plan (tab backgrounded)
1. Visiteur connecté à `/`, passe l'onglet en arrière-plan ou verrouille son téléphone
2. Le keepalive continue silencieusement → le pair **reste visible** côté desktop
3. Visiteur revient sur l'onglet : session toujours active, pas de re-PIN

### UC4 — Visiteur ferme l'onglet
1. L'onglet est fermé → SSE coupé côté serveur → touches session s'arrêtent
2. Après **court délai de grâce**, session expirée → `PeerLost` → pair **disparaît** côté desktop
3. Si transferts « Proposé » en cours pour ce pair → passent en « Expiré »

### UC5 — Visiteur se déconnecte explicitement
1. Clic sur « Se déconnecter » dans le header de `/`
2. Session invalidée côté serveur + cookie nettoyé + redirection vers `/login`

---

## 6. Cas Alternatifs / Limites

- **Host redémarre l'app** → nouveau PIN → tous les visiteurs doivent re-saisir (comportement attendu)
- **Ticket expiré** (15 min sans clic) → 410 visible proprement dans la page web + card desktop passe à « Expiré »
- **Perte Wi-Fi temporaire** du visiteur → SSE se reconnecte automatiquement (EventSource natif). Si session evictée → redirection vers `/login`
- **Browser onglet privé** → `localStorage` non persistant → chaque ouverture = nouveau `device_id`, cas extrême accepté V1.1

---

## 7. Contraintes

- Aucun ajout de lib (V1 libs suffisent : cpp-httplib, qrcodegen, miniz)
- Aucune régression sur le flow TCP LTR1 natif
- Les 6 tests unitaires existants doivent continuer à passer
- L'UI SFML desktop modifiée a minima (principalement : cases à cocher dans FileRow, statuts étendus dans la barre TRANSFERTS)
- Mobile-first : la page web doit être utilisable sur iPhone SE (écran étroit) sans bug

---

## 8. Critères d'Acceptation

- [ ] Les chiffres du PIN s'affichent visiblement quand on les tape (iOS + Android + desktop)
- [ ] La soumission du PIN se fait uniquement au clic « Se connecter » (pas d'auto-submit)
- [ ] `/login` est une page distincte de `/`, avec URL visible dans la barre
- [ ] Un visiteur qui reste sur sa page (onglet actif ou en arrière-plan) **ne disparaît pas** de la sidebar desktop
- [ ] Un même navigateur mobile qui se reconnecte plusieurs fois apparaît comme **1 seul appareil** côté desktop
- [ ] Un envoi desktop → web sans clic du visiteur reste visible **pendant 15 min** avec statut « Proposé » ; passé ce délai, statut « Expiré »
- [ ] Cliquer « Télécharger » côté web renvoie **le fichier réel** (pas de JSON)
- [ ] L'upload fonctionne depuis iOS Safari ET Android Chrome (file picker s'ouvre, fichier envoyé, progress affiché)
- [ ] Côté desktop, chaque fichier dans « Fichiers à envoyer » a une case à cocher ; seuls les cochés sont envoyés ; les envoyés disparaissent automatiquement après succès
- [ ] Un bouton « Se déconnecter » visible dans le header de la page web redirige vers `/login`
- [ ] Aucune régression sur le flow natif TCP LTR1
- [ ] Les 6 tests unitaires existants passent toujours
