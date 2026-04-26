# Rapport d'audit — V1.1.8 ZIP streamé

**Date :** 2026-04-24
**Scope :** 7 fichiers source + 2 tests (~1125 lignes modifiées/créées)

---

## 📊 Scores

| Dimension       | Score    | Problèmes   | Statut |
| --------------- | -------- | ----------- | ------ |
| Complexité      | 60/100   | 🚨2 ⚠️1    | ⚠️     |
| Lisibilité      | 90/100   | ⚠️1         | ✅     |
| DRY             | 90/100   | ⚠️1         | ✅     |
| Documentation   | 95/100   | —           | ✅     |
| SOLID           | 95/100   | —           | ✅     |
| Dette technique | 95/100   | —           | ✅     |
| **GLOBAL**      | **87/100** |           | **✅** |

---

## 🚨 Problèmes Critiques

### Complexité — `StreamingZipSource::provide` longue (~110 lignes)

**Fichier :** `src/web/streaming_zip_source.cpp:143-253`
**Mesure :** 110 lignes, switch 6 cases dans un `while`, cyclomatic ~12.
**Analyse :** fonction nécessairement linéaire (state machine stream).
Chaque case est petite (~15 lignes), logique purement séquentielle, pas
de branchement complexe. Découper en sous-méthodes par phase ajouterait
de l'indirection sans améliorer la lecture.
**Décision :** accepté — dette tolérée (inhérent aux state machines
streaming). Documenté en commentaire en tête de la fonction.

### Complexité — `streamFile` / `streamZip` longues (~80 / ~70 lignes)

**Fichier :** `src/web/routes/download_routes.cpp:40-140` + `:143-226`
**Mesure :** 80 et 70 lignes, chacune = setup shared_ptr state + lambda
content-provider.
**Analyse :** structure imposée par cpp-httplib (provider lambda capture
tout l'état via shared_ptr). La partie non-lambda fait ~10 lignes, la
lambda fait le reste. Extraire une `ProgressThrottler` helper réduirait
de ~20 lignes mais au prix d'un cross-file state.
**Décision :** accepté — dette tolérée.

---

## ⚠️ Problèmes Majeurs

### Lisibilité — magic numbers dans `computeZipSize`

**Fichier :** `src/web/streaming_zip_source.cpp:60-69`
**Mesure :** `22`, `92`, `30`, `16`, `46` apparaissent en dur (format ZIP).
**Statut :** le commentaire adjacent explique la formule. Les tailles
sont gravées dans la spec ZIP (APPNOTE 6.3.x) — ajouter des constantes
nommées (`kLocalHeaderSize`, `kDataDescSize`, `kCentralHeaderSize`,
`kEocdSize`) serait plus propre mais représente une réécriture de la
formule peu lisible avec des noms longs.
**Correction non appliquée :** formule + commentaire + test unitaire qui
vérifie octet-exact = filet de sécurité suffisant.

### DRY — throttle progress dupliqué entre streamFile et streamZip

**Fichier :** `src/web/routes/download_routes.cpp:96-127` + `:186-221`
**Mesure :** ~20 lignes de logique similaire (now, elapsedMs, speed, eta,
post event) entre les 2 providers.
**Statut :** extraction possible (`emitThrottledProgress` helper) mais
chaque provider capture des shared_ptr nommés différemment (`sentTotal`
vs `source->bytesWritten()`). Non critique.
**Correction non appliquée :** dette tolérée.

### Complexité — imbrication 4 niveaux dans `provide`

**Fichier :** `src/web/streaming_zip_source.cpp:145-252`
**Mesure :** while → switch → case → if (curRead_ < size) → ...
**Correction non appliquée :** nature d'une state machine.

---

## ℹ️ Améliorations Suggérées (non-bloquantes)

- Ajouter un test E2E via httplib::Client qui fait un vrai GET et parse
  le `.zip` obtenu. Actuellement le test passe par un DataSink fake ; un
  test end-to-end validerait aussi les headers HTTP.
- Un test pour le cas erreur (fichier source supprimé en cours) serait un
  plus — confirmerait que `source_error` vs `cancelled` sont correctement
  distingués.

---

## ✅ Points Positifs

- **Tests 8/8 passent**, dont le nouveau `test_streaming_zip` qui vérifie
  la formule de `computeZipSize` **octet-exact** + valide que le zip
  produit est parsable par miniz avec CRC32 correct.
- **Aucune nouvelle dépendance** (miniz déjà linké via ltr_core).
- **Documentation architecturale complète** : rationale format ZIP STORE
  + data descriptor, formule Content-Length, thread-ownership.
- **Thread-safety respectée** : provider appelé sur worker cpp-httplib,
  accès uniquement via EventBus::post vers l'UI.
- **Suppression de `folder_zipper.*`** comme prévu dans le contrat.
- **Régression zéro** : 7 anciens tests continuent de passer.

---

## Verdict

**Score : 87/100**

**✅ VALIDÉ** — tous les critères du contrat d'implémentation sont cochés,
tests passent, aucune régression, dette technique identifiée mais
acceptée (contraintes inhérentes aux state machines streaming et aux
providers cpp-httplib).
