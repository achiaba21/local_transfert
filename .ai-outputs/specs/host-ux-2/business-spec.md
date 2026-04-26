# Spec métier — Sprint UX-2 Zone Transferts robuste

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur
**Scope :** app desktop SFML, zone basse TRANSFERTS

---

## 1. Contexte

La zone bas « TRANSFERTS » affiche aujourd'hui des cards inline 320 px sur
une hauteur fixe de 104 px. Elle :
- tronque silencieusement au-delà de 3-4 cards visibles
- n'offre **aucune action** en cours de transfert (seul `Proposed` a un
  bouton Annuler)
- n'offre **aucune action** post-transfert (pas de « Ouvrir le dossier »)
- accumule les cards Done/Failed à l'infini jusqu'à redémarrage app
- affiche une barre de progression 4 px fine + label dense `"67 % · 15 Mo/s · 12s"`

---

## 2. Objectif

Rendre la zone **scalable** (scroll), **actionnable** (annuler + ouvrir) et
**auto-nettoyée** (cards Done/Failed disparaissent toutes seules).

---

## 3. Décisions produit validées

### Q1 — Scroll horizontal (A)
- Cards inline 320 px, hauteur zone inchangée (104 px).
- Flèches L/R cliquables quand débordement.
- Scroll via molette horizontale OU molette verticale si le curseur est
  dans la zone (raccourci UX pour les souris sans molette horizontale).

### Q2 — Annuler InProgress (A)
- Côté envoi natif TCP : `TransferServer::abort(sessionId)` ferme la socket
  → émet `TransferFailedEvent{sessionId, "cancelled"}` côté host.
- Côté réception : le pair détecte la fermeture → nettoie le partial et
  émet `TransferFailedEvent{sessionId, "cancelled"}` de son côté aussi.
- Côté web streaming-zip : le provider héberge déjà le flag `errored_` →
  ajouter un `cancel()` public qui met `errored_ = true; errorMsg = "cancelled"`
  → le prochain `provide` retourne false → cpp-httplib ferme → le visiteur
  voit zip tronqué.

### Q3 — Actions post-transfert (A)
- **Côté récepteur uniquement** (Direction = Incoming + Status = Done)
- **Une seule action** : bouton « Ouvrir le dossier » qui ouvre le
  `downloadDir` via la commande système native :
  - macOS → `open "<path>"`
  - Windows → `explorer "<path>"`
  - Linux → `xdg-open "<path>"`
- Exécution via `std::system` OU `posix_spawn`/`ShellExecuteW`. Le path
  est shell-escaped pour éviter tout risque de command injection.

### Q4 — Notifications OS (B)
- **Repoussé en sprint UX-5 Confort.** Pas de touch aux Obj-C/WinRT dans
  ce sprint.

### Q5 — Auto-clean des cards terminées (A)
- **Done** → retrait automatique après **10 s**.
- **Failed / Cancelled** → retrait automatique après **30 s** (plus long
  pour laisser à l'utilisateur le temps de lire l'erreur).
- **Expired** → reste affiché (l'utilisateur doit voir qu'un visiteur web
  n'a rien téléchargé).
- **Proposed** → reste jusqu'à expiration du ticket (15 min TTL).
- Implémentation : chaque `UiTransfer` gagne un champ `terminalAt` posé
  au passage à Done/Failed/Cancelled. `AppController::onTick` (appelé
  depuis update dt) purge quand `now - terminalAt > seuil`.

---

## 4. Livrables checklist

- [ ] Scroll horizontal + flèches L/R + indicateur de position (dots ou
      fade sur les bords ? → à trancher en UI/UX)
- [ ] Molette horizontale + shift+molette verticale = scroll
- [ ] Barre de progression **8 px** épaisseur (+ hauteur de card
      éventuellement ajustée)
- [ ] Label status hiérarchisé : `%` en grand (FontSize::h2 bold),
      `speed / eta` en petit en-dessous (FontSize::small)
- [ ] Bouton Annuler pour cards en status `InProgress`
- [ ] Bouton « Ouvrir le dossier » pour cards Incoming + Done
- [ ] Auto-clean : Done 10 s, Failed/Cancelled 30 s
- [ ] `TransferServer::abort(sessionId)` thread-safe
- [ ] `StreamingZipSource::cancel()` thread-safe
- [ ] `AppController::cancelInProgress(sessionId)` route vers le bon
      backend selon PeerKind de la session
- [ ] PROGRESS.md mis à jour (UX-2 ✅ + item "Direction par icône" coché
      en héritage UX-1)

---

## 5. Critères d'acceptation

- Envoyer un gros fichier (≥ 2 Go) natif → bouton Annuler apparaît →
  clic → le transfert s'arrête des 2 côtés, status « Annulé » affiché.
- Envoyer un fichier vers le web → le visiteur démarre le download →
  host clique Annuler → browser voit le download échouer proprement.
- Recevoir un fichier → Done → card reste 10 s avec bouton « Ouvrir le
  dossier » → click → Finder/Explorer s'ouvre sur `downloadDir` → après
  10 s sans interaction, card disparaît.
- Lancer 10 transferts simultanés → flèches L/R apparaissent → scroll
  fonctionne → toutes les cards sont atteignables.
- 8 tests existants passent sans régression.

---

## 6. Contraintes techniques

- C++17 strict
- Aucune nouvelle dépendance (on utilise `std::system` pour Finder/Explorer)
- Path shell-escaped (risque command injection si `downloadDir` contient
  guillemets ou backticks → fonction utilitaire `shellEscape`)
- Thread-safety : `TransferServer::abort` appelé depuis le thread UI
  pendant que le thread de transfert pousse des bytes — besoin d'un flag
  atomique vérifié à chaque chunk
- Backward compat : API publique des widgets inchangée
- UI_REQUIRED: true (nouveaux boutons + scroll = nouveaux mockups)
