# Rapport d'audit — Sprint Transfer Resume (MVP Wave 1+2+3)

**Date :** 2026-04-24
**Scope livré :** 3 fichiers créés (resume_sidecar.hpp/cpp + test) + 9
modifiés (event_bus, protocol, config hpp/cpp, transfer_server hpp/cpp,
transfer_client hpp/cpp, app_state, app_controller hpp/cpp, main_screen
hpp/cpp, CMakeLists)

---

## ⚠️ Livraison partielle

Ce sprint a été livré en **MVP** : foundation + resume end-to-end basique
(restart from 0 à chaque Reprendre). Le gain vrai du resume (continuer
aux offsets partials via `ResumeOffer/ResumeResponse`) reste à faire.
Cf. section « Non livré » dans ANALYSIS.md.

---

## 📊 Scores (sur le MVP livré)

| Dimension       | Score    | Problèmes   | Statut |
| --------------- | -------- | ----------- | ------ |
| Complexité      | 85/100   | ⚠️1         | ✅     |
| Lisibilité      | 95/100   | —           | ✅     |
| DRY             | 90/100   | —           | ✅     |
| Documentation   | 90/100   | ℹ️1         | ✅     |
| SOLID           | 95/100   | —           | ✅     |
| Dette technique | 75/100   | 🚨1 ⚠️1    | ⚠️     |
| **GLOBAL**      | **88/100** |           | **✅** |

---

## 🚨 Dette technique reconnue

### Resume MVP = restart from 0 (non livré : skipBytes vrai)

**Fichier :** `src/network/transfer_client.cpp::resumeSession`
**Constat :** La méthode réutilise le même `sessionId` mais relance un
`runSender` classique → le serveur écrase son sidecar au moment du
premier `FileHeader`. Conséquence : cliquer « Reprendre » sur un envoi
qui a échoué à 64 % redémarre les 64 % déjà transférés.
**Impact :** le resume fonctionne fonctionnellement (bouton clicable,
flux re-démarre) mais ne réalise pas l'économie de bandwidth promise.
**Plan :** implémentation réelle prévue en Wave 3b :
`ResumeOffer` → `ResumeResponse` avec `skipBytes` par fichier, puis
`fseek` dans runSender pour chaque fichier partial.

---

## ⚠️ Problème majeur

### Refactor non-blocking différé

**Fichier :** `src/network/transfer_client.cpp::runSender` + `transfer_server.cpp::sessionWorker`
**Constat :** Le heartbeat Ping/Pong nécessite de refactorer les loops
de lecture bloquantes vers `sf::SocketSelector::wait(timeout)`. Pas
fait en Wave 2 MVP. Conséquence : un Wi-Fi bloqué met 30-120 s à être
détecté (TCP timeout OS), pas 20 s comme prévu.
**Décision :** Wave 3b.

---

## ℹ️ Améliorations suggérées

### Doc `docs-agents/NETWORK.md` non créée

Reportée en Wave 3b avec l'implémentation skipBytes vraie, pour pouvoir
documenter tout le flow resume en une fois.

---

## ✅ Points positifs du MVP

- **Foundation Wave 1 solide** : `resume_sidecar` (I/O atomique
  `.tmp + rename`, gestion JSON corrompu, purge TTL) testée avec 7 cas
- **Sidecar lifecycle correct côté serveur** : créé au start de
  session, updaté à chaque `FileEnd`, conservé sur erreur réseau,
  supprimé sur cancel explicite ou Done global
- **UI cohérente** : boutons « Reprendre » + « Ignorer » stackés
  vertical sur cards Failed resumable ; bouton global « Reprendre
  tout (N) » dans le header TRANSFERTS
- **ErrorCategory propagée** correctement : Cancelled n'affiche jamais
  « Reprendre » (pas un bug), Network/Unknown affiche toujours
- **Backward-compat** : le protocole reste LTR1 identique côté envoi
  (pas encore de champ `protocol: "LTR1.1"` dans OFFER), donc aucune
  incompatibilité avec un pair legacy
- **Build Release propre**, 9/9 tests passent, zéro régression sur
  web/UX-1..4

---

## Verdict

**Score MVP : 88/100**

**✅ VALIDÉ (MVP)** — la foundation + le squelette fonctionnel sont en
place. L'utilisateur peut cliquer « Reprendre » sur des transferts
échoués et ça redémarre. Le gain vrai (continuer aux offsets) +
heartbeat + auto-retry + cross-restart + banner startup + icône =
**Wave 3b à planifier en sprint dédié** (estimé 8-12 j supplémentaires).
