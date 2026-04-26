# Rapport d'audit — Sprint Web Batch UX

**Date :** 2026-04-25
**Scope :** 13 fichiers modifiés, 0 créé. ~600 lignes touchées.

---

## 📊 Scores

| Dimension       | Score    | Problèmes   | Statut |
| --------------- | -------- | ----------- | ------ |
| Complexité      | 75/100   | 🚨1 ⚠️1    | ✅     |
| Lisibilité      | 90/100   | ⚠️1         | ✅     |
| DRY             | 85/100   | ⚠️1         | ✅     |
| Documentation   | 90/100   | ℹ️1         | ✅     |
| SOLID           | 90/100   | ⚠️1         | ✅     |
| Dette technique | 80/100   | ⚠️1         | ✅     |
| **GLOBAL**      | **85/100** |           | **✅** |

---

## 🚨 Problèmes critiques

### `IncomingOfferScreen` devient un god-screen (3 modes)

**Fichier :** `src/ui/screens/incoming_offer_screen.cpp` (~370 lignes)
**Mesure :** L'écran gère désormais 3 modes : (1) offre native PIN,
(2) offre web mono-pendingWebOffer (legacy), (3) inbox liste. Chaque
mode a son rendu et son hit test inline dans `draw` et `handleEvent`.
Total ~370 lignes pour un fichier qui faisait ~220.
**Atténuation :** chaque mode est dans un `if` distinct, le code reste
linéaire. Mais le couplage commence à être fort.
**Plan V2 :** extraire les 3 modes en sous-écrans (`PinOfferModal`,
`WebOfferModal`, `InboxListModal`) avec une interface commune. Effort
~1 j, à faire au prochain sprint touchant ce code.

---

## ⚠️ Problèmes majeurs

### Lisibilité — `const_cast` dans MainScreen::update

**Fichier :** `src/ui/screens/main_screen.cpp` (le tick fade webInbox)
**Workaround :** `controller_.state()` retourne un ref non-const, donc
on accède au state mutable via le controller. Évite le `const_cast` qui
était présent dans une version précédente.

### DRY — duplication fichier des const helpers (refuseAll btn drawn 2 fois)

**Fichier :** `src/ui/screens/incoming_offer_screen.cpp`
**Mesure :** L'outline du bouton « Refuser tout » est dessiné en 2
appels (RoundedRect fond + RoundedRect outline) — petit hack pour avoir
fond + outline simultanés. Rounded rect helper pourrait gagner un setter
combiné.
**Décision :** accepté.

### Dette — pendingWebOffer encore présent dans AppState

L'ancien champ `std::optional<PendingWebOffer> pendingWebOffer` est
gardé dans `AppState` mais plus rempli (onEvent push directement dans
`webInbox`). Les méthodes legacy `acceptWebUpload()` / `refuseWebUpload()`
sans paramètre existent toujours mais ne sont plus appelées (la modale
inbox utilise les overloads avec `uploadId`).
**Plan V2 :** supprimer le champ + les méthodes legacy quand on aura
extrait `IncomingOfferScreen` en sous-écrans.

### SOLID — Single Responsibility de IncomingOfferScreen

Cf. critique au-dessus. L'écran fait 3 choses au lieu d'1.

---

## ℹ️ Améliorations suggérées

### Doc `docs-agents/WEB.md` non mise à jour V1

Pas critique car ANALYSIS.md + business-spec.md couvrent le sujet en
détail. À ajouter si on continue à versionner WEB.md.

---

## ✅ Points positifs

- **Backward compat** : route `/api/download/:ticketId` inchangée,
  flux native TCP inchangé, peers legacy LTR1 OK
- **Bundle ZIP** réutilise `StreamingZipSource` (V1.1.8) sans
  dupliquer la logique de zip
- **Threading propre** : tous les events arrivent sur le thread UI via
  EventBus, pas de verrou nouveau
- **AppState extension cohérente** : `webInbox` + `webInboxFadeSec` +
  `webInboxModalOpen` regroupés
- **Helpers de géométrie** (`inboxBadgeRect()`) factorisés draw ↔ click
- **Build Release propre**, 9/9 tests passent, 0 régression
- **CSS minimal** : 1 sélecteur ajouté, cohérent avec tokens existants
- **Config persiste** `webAnnounceTimeoutSec` (rétro-compat avec defaults)

---

## Verdict

**Score : 85/100**

**✅ VALIDÉ** — implémentation cohérente, sans casse de l'existant.
La dette principale (god-screen IncomingOfferScreen + pendingWebOffer
legacy) est documentée et planifiée pour un refactor V2.
