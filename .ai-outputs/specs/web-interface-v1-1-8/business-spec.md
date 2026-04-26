# Spec métier — V1.1.8 : ZIP streamé desktop → web

**Date :** 2026-04-23
**Statut :** ✅ validé utilisateur

---

## 1. Contexte

Depuis V1.1.7, quand le desktop envoie un dossier vers un visiteur web,
`WebService::zipAndAnnounce` (thread détaché) zippe le dossier dans un fichier
temporaire `/tmp/ltr-zip-XXX.zip` avant d'émettre le SSE `files-offer`.

Problèmes observés sur dossiers lourds (plusieurs Go) :

- UI desktop avec un léger freeze perceptible au clic « Envoyer » (le walk
  préliminaire pour calculer les tailles tourne sur le thread UI).
- Le navigateur ne reçoit pas d'octet tant que le zip disque n'est pas
  terminé → perception « app figée ».
- I/O disque doublée : écriture temp × N octets, puis relecture par
  `download_routes` au moment du GET.

---

## 2. Objectif

Supprimer l'étape « zip temp sur disque » et streamer le zip **directement
dans la réponse HTTP** du GET `/api/download/:ticketId`. Le navigateur voit
un `.zip` normal avec un `Content-Length` pré-calculé (barre de progression
alimentée nativement), et les octets arrivent immédiatement après le clic.

---

## 3. Acteurs

- **Host desktop** (C++) : propriétaire des fichiers à envoyer
- **Visiteur web** (navigateur LAN) : destinataire du zip
- **Serveur HTTP embarqué** (cpp-httplib) : assemble le zip à la volée

---

## 4. Règles métier validées

### Q1 — Annulation visiteur (onglet fermé, cancel en cours)

→ **A** : émettre `TransferFailedEvent{sessionId, "cancelled"}` côté
desktop. Le visiteur voit sa zone TRANSFERTS passer à « Annulé ».

### Q2 — Erreur source en cours de stream (fichier disparu, permission)

→ **A** : couper la connexion HTTP (return du provider avec `false`). Le
navigateur reçoit un `.zip` tronqué et le visiteur peut retenter
(voir Q3 : ticket toujours valide pendant 15 min).

### Q3 — Ticket rejouable pendant 15 min

→ **B** : autoriser plusieurs GET sur le même `ticketId` tant que le TTL
n'est pas expiré. Utile si le visiteur annule par erreur puis reclique, ou
si une rupture réseau a coupé le 1er téléchargement. Plus d'auto-suppression
du ticket au 1er GET.

### Q4 — TTL du ticket

→ **15 min** (inchangé).

### Q5 — Fichier unique (pas un dossier)

→ **A** : un ticket pour 1 fichier simple = stream direct du fichier
source, **pas de zip**. Le zip n'intervient que pour les dossiers.

### Bonus — `folder_zipper.cpp::zipDirectoryToFile`

→ **Supprimer** le mode fichier-temp (plus de caller après la migration).

---

## 5. Cas d'usage principal

1. Host clique « Envoyer » sur un dossier de 5 Go vers un visiteur web
2. `AppController::requestSend` → `WebService::pushFiles` (thread UI,
   non-bloquant — pas de walk récursif ici)
3. Un thread worker :
   - Walk récursif du dossier pour construire la liste des fichiers
   - Calcul du Content-Length zip (STORE mode) = Σ(fileSize) +
     overhead constant par entrée + EOCD
   - Issue 1 ticket `DownloadTicket` (mode « streaming-zip »)
   - Émet SSE `files-offer` vers le navigateur
4. Navigateur affiche « dossier.zip (5 Go) » → clic Télécharger
5. GET `/api/download/:ticketId`
   - Résolve le ticket → récupère la liste des fichiers
   - `set_content_provider(zipSize, "application/zip", provider)`
   - Provider émet à la volée : local file header + CRC32 + data + next
     entry... + central directory + EOCD
6. Navigateur streame directement vers le disque, barre de progression
   en temps réel, aucun fichier temp côté serveur
7. Desktop reçoit des `TransferProgressEvent` à cadence raisonnable
   (throttle 100 ms / 1 MB pour ne pas flooder l'EventBus)
8. `TransferDoneEvent` à la fin

---

## 6. Cas alternatifs / limites

- **Fichier source disparu en cours de stream** (Q2) : le provider renvoie
  `false` → cpp-httplib coupe la connexion. Desktop émet
  `TransferFailedEvent{sessionId, "source_error"}`.
- **Visiteur ferme l'onglet** (Q1) : le provider voit une erreur d'écriture
  sur `DataSink` → renvoie `false`. Desktop émet
  `TransferFailedEvent{sessionId, "cancelled"}`.
- **Ticket expiré** : GET répond 404 (comportement actuel).
- **Ticket rejoué pendant le TTL** (Q3) : chaque GET relit le dossier à la
  volée. Si le dossier a changé entre 2 GET, la liste snapshot au moment
  de l'issue fait foi (les ajouts ne sont pas inclus).
- **Session expirée pendant le stream** : le stream continue (la session
  n'est vérifiée qu'au moment du GET initial, pas à chaque chunk).
- **Gros dossier avec >10k fichiers** : le calcul de Content-Length reste
  linéaire en N fichiers (une lecture du file_size par entrée). Acceptable.

---

## 7. Contraintes techniques

- C++17 strict
- Aucune nouvelle dépendance (miniz + cpp-httplib suffisent — ou writer
  zip STORE custom si plus simple)
- RAII, EventBus pour thread → UI
- Thread-safety : le provider est appelé sur un worker cpp-httplib —
  aucun accès à `AppState`, uniquement `EventBus::post()`
- Aucune régression TCP LTR1
- Tests existants (7/7) doivent continuer à passer
- Ajout d'un test unitaire sur le calcul de Content-Length (sinon gros
  risque de mismatch octet)

---

## 8. Critères d'acceptation

- [ ] Aucun fichier temp créé dans `/tmp/ltr-zip-*` après un envoi de dossier
- [ ] Premier octet reçu côté navigateur < 200 ms après le clic Télécharger
  (mesuré au `curl -v`)
- [ ] Le `.zip` téléchargé est lisible par `unzip -l` et décompressable
- [ ] Le Content-Length annoncé == taille réelle du zip (sinon 200 OK
  tronqué côté navigateur)
- [ ] UI desktop ne freeze pas pendant le walk d'un dossier de 5 Go
- [ ] Annulation navigateur → TransferFailedEvent{cancelled} côté desktop
- [ ] Ticket peut être rejoué plusieurs fois pendant 15 min
- [ ] Fichier unique (non-dossier) = stream direct du fichier, pas de zip
- [ ] `folder_zipper.cpp::zipDirectoryToFile` supprimé (le header aussi)
- [ ] Tests 7/7 passent + test Content-Length zip
- [ ] `docs-agents/WEB.md` V1.1.8 documenté
- [ ] `.ai-outputs/docs/web-interface.html` changelog V1.1.8
