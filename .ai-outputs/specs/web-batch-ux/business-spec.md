# Spec métier — Sprint Web Batch UX

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur
**Analyse de référence :** `.ai-outputs/specs/web-batch-ux/ANALYSIS.md`

---

## 1. Contexte

Deux frictions UX parallèles :
1. **Côté web** : quand le host envoie N fichiers, le visiteur doit
   cliquer N fois pour tout télécharger.
2. **Côté host** : quand le visiteur envoie plusieurs fichiers en
   actions séparées (pas en un seul batch), chaque announce ouvre une
   modale bloquante → les modales s'empilent, le host est harcelé.

---

## 2. Objectif

- Ajouter un bouton « Télécharger tout » côté web avec stratégie hybride
  (ZIP serveur unique pour < 4 Go, fallback séquentiel JS pour ≥ 4 Go)
- Remplacer la pile de modales par une **inbox non-invasive** côté host :
  badge compact dans le header + modale liste complète ouverte à la
  demande, avec actions individuelles et globales.

---

## 3. Décisions produit validées

### Q1=A — « Accepter tout » = fichiers à plat
3 announces en attente : A (photo.jpg), B (dossier Photos/ 5 images),
C (notes.txt) → clic [Accepter tout] + 1 folder picker = destination
unique. Tous les fichiers atterrissent **à plat** dans ce dossier :
```
destination/
├── photo.jpg              ← de A
├── Photos/                ← de B (l'arbo interne préservée)
│   ├── IMG_001.jpg
│   └── ...
└── notes.txt              ← de C
```
Les dossiers gardent leur structure interne via
`FilesystemService::uniqueTargetPath`. Les fichiers simples atterrissent
à plat. Pas de sous-dossier horodaté créé.

### Q2=B — Badge disparaît après fade-out
Quand la dernière demande est traitée (acceptée ou refusée), le badge
reste visible ~3 s avec opacité dégressive pour confirmer « fini »,
puis disparaît. Si une nouvelle demande arrive pendant le fade, reprise
de l'opacité pleine.

### Q3=C — Timeout announce configurable
Nouveau champ `infra::Config::webAnnounceTimeoutSec` (default **300 s**,
soit 5 min). Relève le default historique de 120 s pour laisser à
l'utilisateur le temps d'interagir avec l'inbox sans pression. Le champ
est éditable dans `config.json` en V1 (pas d'UI Settings).

---

## 4. Livrables

### Backend
- Route `GET /api/download/bundle/:sessionId` qui streame un ZIP
  unique de tous les tickets de la session (File + StreamingZip
  développés en ZipEntries fusionnés)
- `DownloadTicketStore::listBySession(sessionId)` helper
- `AppState::webInbox` (remplace la modale immédiate)
- `AppController` :
  - onEvent WebIncomingOfferEvent → push dans webInbox (plus de
    modale auto-ouverte)
  - `acceptWebUpload(uploadId)` par id + `refuseWebUpload(uploadId)`
  - `acceptAllWebUploads()` + `refuseAllWebUploads()`
  - Pour « accepter tout » : UN folder picker → tous les
    `resolveAccept(id, dir)` utilisent le même dir
- `Config::webAnnounceTimeoutSec` (default 300) + load/save JSON
- `WebUploadAnnounceStore::waitForDecision` utilise cette valeur

### UI desktop (SFML)
- Badge compact dans le header (pill accent « 📥 N ») — masqué si 0
  demande
- Fade-out 3 s après passage à 0 demande (opacité dégressive)
- Modale `IncomingOfferScreen` étendue : nouveau mode « inbox list »
  affichant toutes les entrées avec boutons [Accepter] [Refuser]
  individuels + header [Accepter tout] [Refuser tout] + [Fermer sans
  agir]
- Chaque row liste : icône direction ↓ + nom peer + nom fichier (ou
  « dossier N fichiers ») + taille + 2 boutons
- Clic badge → ouvre la modale
- Flow natif TCP (PIN) reste dans sa modale actuelle, non touché

### Web (JS / CSS)
- Bouton « Télécharger tout (N · taille) » affiché au-dessus de la liste
  des reçus si ≥ 2 fichiers
- Logique hybride :
  - Total < 4 Go → `fetch('/api/download/bundle/<sid>')` + blob
    download
  - Total ≥ 4 Go → boucle séquentielle `downloadWithProgress` sur les
    tickets individuels
- Styles CSS `.dl-bundle`

### Tests
- `test_download_ticket_store` étendu pour `listBySession`
- Smoke manuels :
  - Host → web 3 fichiers + 1 dossier = 4 tickets → clic « Télécharger
    tout » → navigateur reçoit 1 ZIP qui s'ouvre et contient tout
  - Visiteur → host 3 announces successifs → badge passe à 3 → clic
    badge → modale liste 3 entrées → clic [Accepter tout] → 1 folder
    picker → 3 uploads convergent vers le même dossier
  - Badge fade-out après traitement

### Doc
- `docs-agents/WEB.md` : sections « Bundle download » + « Inbox host »

---

## 5. Critères d'acceptation

- Visiteur web reçoit 5 fichiers → bouton « Télécharger tout (5 · 42
  Mo) » visible → clic → 1 zip téléchargé contenant les 5
- Fichiers > 4 Go → bouton « Télécharger tout » → comportement
  séquentiel (pas de zip à cause de la limite ZIP32)
- Visiteur envoie 3 announces rapides en 10 s → host voit 1 badge « 3 »
  dans le header, PAS 3 modales empilées
- Clic badge → modale liste 3 rows → clic [Accepter tout] → 1 folder
  picker → les 3 envois vont au même dossier choisi
- Clic individuel [Accepter] sur 1 row → 1 folder picker pour CE row
  seul → l'envoi arrive → badge passe à 2
- [Refuser tout] → toutes les entrées retirées, visiteur voit « refusé »
- Fermer sans agir → modale se ferme, badge reste avec count
- Timeout visiteur à 5 min (default 300 s) → entrée auto-retirée,
  visiteur voit « timeout »
- Flow natif TCP LTR1 : modale PIN classique inchangée
- 9 tests existants passent, 0 régression
- Build Release propre

---

## 6. Contraintes

- C++17 strict
- Aucune nouvelle dépendance
- RoundedRect pour nouveaux éléments UI
- Tokens Theme respectés
- Thread-safety : WebIncomingOfferEvent arrive sur worker HTTP, push
  dans bus puis drain UI thread → OK
- Backward compat : route `/download/self` inchangée, `/api/download/:ticketId`
  inchangée. Nouveau endpoint `/api/download/bundle/:sid` additif.
- UI_REQUIRED: true (mockup badge + modale inbox)

---

## 7. Hors scope V1

- Notifications OS natives à l'arrivée d'un announce (reporté UX-5)
- Preview miniature des fichiers entrants
- Historique des demandes refusées / acceptées
- Édition de `webAnnounceTimeoutSec` via UI Settings
- Support ZIP64 pour fichiers > 4 Go dans le bundle (fallback séquentiel
  suffit en V1)
