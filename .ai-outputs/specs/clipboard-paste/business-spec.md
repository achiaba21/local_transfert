# Spec métier — Sprint Clipboard Paste (V1.4)

**Date :** 2026-05-02
**Statut :** ✅ Validée

---

## 1. Contexte

Ajout d'une fonctionnalité « Coller » pour permettre l'envoi rapide
du contenu du presse-papier sans passer par un picker ou drag-drop.

## 2. Objectif

- Bouton « Coller » dans le menu existant + raccourci Cmd+V / Ctrl+V
  global
- Détecte automatiquement le type (texte, image, fichiers) et agit
- macOS + Windows natif, web via navigator.clipboard
- Réutilise tout le pipeline existant (addFiles → enumerate → send)

## 3. Décisions produit validées

| # | Décision | Choix |
|---|---|---|
| 1 | Nom fichier texte | Générique : `clipboard-YYYYMMDD-HHmmss.txt` |
| 2 | Multi-fichiers Finder | Tous ajoutés sans confirmation |
| 3 | Linux | Stub minimal : texte seulement via sf::Clipboard |
| 4 | Cmd+V dans champ focus | Local gagne (paste standard) |
| 5 | Web permission refusée | Toast d'erreur + lien aide |

## 4. Cas d'usage

### Desktop
1. User copie contenu (texte / image / fichier(s)) dans une autre app
2. Click « Ajouter ▾ → Coller » OU Cmd+V dans la fenêtre
3. App détecte le type avec priorité **Files > Image > Text > None**
4. Selon le type :
   - Files/Folders : `addFiles(paths)` direct
   - Image PNG : écrit dans `/tmp/ltr-clipboard/` puis `addFiles`
   - Text : écrit `clipboard-YYYYMMDD-HHmmss.txt` UTF-8 puis `addFiles`
   - None : toast « Presse-papier vide »
5. Toast feedback récap

### Web
1. Bouton « Coller » à côté de « Choisir des fichiers »
2. Click → `navigator.clipboard.read()`
3. Si autorisé : lit text/plain ou image/png, crée Blob
4. Si refusé : toast erreur

### Cleanup
- Sous-dossier `/tmp/ltr-clipboard/` ou `%TEMP%\ltr-clipboard\`
- Création idempotente au boot, purge > 24 h au boot
- Purge totale au shutdown propre
- Auto-clean post-TransferDone : suppression physique si path sous
  dossier clipboard

## 5. Cas alternatifs

- Cmd+V dans IpInput → paste local au champ
- Presse-papier vide → toast
- Permission web refusée → toast + retry
- Image non-PNG (TIFF/BMP/DIB) → tentative conversion, sinon toast
- Multi types simultanés → priorité Files > Image > Text
- Windows clipboard mutex → retry 3× avec 10 ms

## 6. Critères d'acceptation

- [ ] Cmd+V hors champ texte → ajoute presse-papier
- [ ] Click menu « Coller » : même comportement
- [ ] Texte → `clipboard-*.txt` ajouté
- [ ] Screenshot → `clipboard-*.png` ajouté
- [ ] Cmd+C 3 fichiers dans Finder + Cmd+V → 3 entrées
- [ ] Cmd+C 1 dossier + Cmd+V → folder préservé (fix précédent)
- [ ] Vide → toast « Presse-papier vide »
- [ ] Champ « 192.168.1.x » + Cmd+V → paste local au champ
- [ ] Bouton web visible si dispo, masqué sinon
- [ ] Web texte/image OK, fichiers non supportés
- [ ] Auto-clean post-send supprime physiquement temp clipboard
- [ ] 14/14 tests + 1 nouveau (stub Kind::None)
- [ ] Build Mac et Windows propre
