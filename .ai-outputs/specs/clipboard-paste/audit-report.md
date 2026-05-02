# Rapport d'audit — Sprint Clipboard Paste (V1.4)

**Date :** 2026-05-02
**Scope :** 5 fichiers nouveaux (clipboard_paste_*, test) + 6 modifs.
~440 LOC ajoutées.

---

## 📊 Scores

| Dimension       | Score    | Problèmes | Statut |
| --------------- | -------- | --------- | ------ |
| Complexité      | 80/100   | ⚠️1       | ✅     |
| Lisibilité      | 90/100   | —         | ✅     |
| DRY             | 88/100   | ℹ️1       | ✅     |
| Documentation   | 92/100   | —         | ✅     |
| SOLID           | 92/100   | —         | ✅     |
| Dette technique | 82/100   | ℹ️1       | ✅     |
| **GLOBAL**      | **87/100** |        | **✅** |

---

## ⚠️ Problèmes Majeurs

### Complexité — Encodeur PNG ~80 LOC inline dans `clipboard_paste_win.cpp`

**Fichier :** `src/ui/clipboard_paste_win.cpp:39-118`
**Mesure :** Fonction `encodePng` + `dibToPng` ~110 lignes au total
dans le fichier (qui en fait 233). PNG IHDR + IDAT zlib + IEND
implémenté manuellement avec miniz pour la compression deflate.

**Décision :** accepté V1.4 — l'encodeur est isolé, lambdas locales
bien nommées (put32, putBytes, putChunk). À extraire dans
`include/ltr/core/png_encoder.hpp` si on a besoin d'encoder PNG
ailleurs (V2). Aucune autre dépendance ajoutée.

---

## ℹ️ Améliorations Suggérées

### DRY — `timestampSuffix` côté web / `nowTimestamp` côté C++

**Fichiers :**
- `src/app/app_controller.cpp:nowTimestamp`
- `assets/web/js/upload.js:timestampSuffix`
**Mesure :** Le format `YYYYMMDD-HHmmss` est dupliqué entre les deux.
Mineur (langages différents, code minimal). À harmoniser si on étend
le format.

### Dette — Pas de mocks pour tester `readClipboard` natifs

Les implémentations Mac/Win ne peuvent pas être testées en CI sans
manipuler le presse-papier système. Le test stub suffit pour les
invariants. Manuel pour les cas réels.

---

## ✅ Points positifs

### Architecture
- Pattern compile-time identique à `drag_drop_*` éprouvé
- 1 header public (`clipboard_paste.hpp`, 38 LOC) → API minimale
- 3 implémentations natives clairement séparées
- Aucune nouvelle dépendance externe (NSPasteboard / Win32 / miniz
  déjà liés)

### macOS
- Priorité Files > Image > Text bien implémentée
- TIFF → PNG fallback automatique via NSBitmapImageRep
- ARC + Cocoa propre, modeled sur drag_drop_mac.mm

### Windows
- Retry 3× × 10 ms sur OpenClipboard (clipboard system mutex)
- Encodeur PNG natif (IHDR + IDAT zlib via miniz + IEND)
- DIB 24/32 bits BGR/BGRA → RGBA top-down conversion correcte
- UTF-16 → UTF-8 via WideCharToMultiByte

### AppController
- Boot : `ensureClipboardTempDir` + `purgeOldClipboardTemp` (>24 h)
- Auto-clean post-TransferDone étendu : suppression physique si path
  sous tempDir → pas de pollution /tmp
- Méthode publique `clipboardTempDir()` testable

### MainScreen
- Cmd+V (Mac) / Ctrl+V (Win) via `key.system` / `key.control`
- Skip si `ipInput_.hasFocus()` → paste local au champ texte préservé
- 3e entrée menu avec hint clavier dans le label

### Web
- Détection `navigator.clipboard.read` → bouton hidden si non supporté
- Réutilise `uploadFiles` existant via `new File([blob], name)`
- Toast erreur clair sur permission refusée

### Backward compat
- 15/15 tests (14 anciens + clipboard_stub)
- Pas de régression sur drag-drop, pickers, V1.3
- ipInput_ paste local préservé

---

## Verdict

**Score : 87/100 ✅ VALIDÉ**

Sprint petit-moyen livré proprement avec :
- Multi-plateforme (Mac native + Win native + stub Linux)
- Web inclus (navigator.clipboard.read)
- Raccourci clavier global avec gestion focus champ
- Cleanup automatique des fichiers temp
- Encodeur PNG Windows fait maison (sans nouvelle dep)

À surveiller V2 : extraction `png_encoder.hpp` si besoin ailleurs ;
harmoniser timestampSuffix JS/C++.
