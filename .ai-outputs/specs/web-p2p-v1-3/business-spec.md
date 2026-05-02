# Spec métier — Sprint Web P2P V1.3 (Robustesse + Liste)

**Date :** 2026-05-02
**Statut :** ✅ Validée

---

## 1. Contexte

V1.2 livrée fonctionnelle pour cas nominaux. Tests utilisateur ont
exposé des silent stalls (DataChannel ouvert mais 0 byte transmis), des
fichiers tronqués téléchargés sans avertissement, et un manque de
visibilité par-fichier en multi-fichier.

## 2. Objectif

Éliminer les blocages silencieux, donner une visibilité granulaire par
fichier (sender + receiver), durcir les cas limites (disconnect
transitoire, intégrité, timeout drain), permettre A↔B simultané.

## 3. Décisions produit validées

| # | Décision | Choix |
|---|---|---|
| 1 | Persistance liste | localStorage : métadonnées + statuts seulement (Blobs ne survivent pas) |
| 2 | Retry échec | Manuel uniquement, bouton « Réessayer » sur ligne ✗ |
| 3 | Emplacement liste | Tabs « Host / P2P » dans la zone footer transfers-bar |
| 4 | Notification complétion | Toast + son + vibration mobile (navigator.vibrate(200)) |
| 5 | Re-téléchargement reçu | Pas de cache — l'utilisateur doit demander un nouvel envoi |

## 4. Cas d'usage durcis

### Émetteur
- Click peer → file picker → confirm → envoi multi-fichier
- Pour chaque fichier : statut visible
  « ⏱ pending » → « ↻ sending 67 % » → « ✓ sent » ou « ✗ failed (raison) »
- Si fichier 3/5 échoue → continue 4/5, pas de cleanup global
- Bouton « Réessayer » sur ligne ✗ (relance uniquement le fichier
  échoué via la même connexion si encore ouverte, sinon nouvelle)
- Vraie progression affichée = `bytesAckedByReceiver` (feedback réel),
  pas `bytesSent` local
- Si pas d'ack pendant 10 s alors qu'on push → cleanup
  « ✗ Récepteur muet »

### Récepteur
- Modale Accepter/Refuser inchangée
- Watchdog : DataChannel ouvert + 0 byte reçu pendant 10 s → cleanup
  « ✗ Pas de données »
- À chaque `file-end`, vérifier `cur.received === cur.size`. Si KO →
  marquer ✗ « Fichier tronqué », ne PAS télécharger
- Envoie ack toutes les ~500 ms : `{kind:'ack', bytes: <total cumul>}`
- À la complétion : toast + son + `navigator.vibrate(200)`

### Connection
- `disconnected` transitoire : pause des sends côté émetteur,
  reprise à `connected`
- Si `disconnected` dure > 15 s → cleanup avec message clair
- Timeout drain final 30 s côté émetteur

### Bidirectionnel
- Clé composite `${deviceId}:${role}` permet A→B + B→A simultanés
- Card peer affiche les deux états si concurrents

## 5. Liste « Transferts P2P »

UI tabs dans la footer existante :
```
TRANSFERTS · 5
[ Host (1) ] [ P2P (4) ]

✓ photo1.jpg · 2.4 Mo · → 🦊 Pingouin Bleu       13:42
↻ video.mp4  · 67 %    · → 🦊 Pingouin Bleu       13:42
✗ doc.pdf    · échec réseau  [Réessayer]          13:42
⏱ rapport.docx · en attente  · → …                13:42
✓ slides.pdf · 4.1 Mo  · ← 🐧 Lapin Vif           13:38
```

- Tri récence (plus récent en haut)
- Auto-collapse si > 10 entrées (bouton « Voir tout »)
- localStorage `ltr-p2p-history` = JSON array (Blobs jamais persistés)
- Couleurs : success (✓), accent (↻), error (✗), textSecondary (⏱)
- Mobile-first : compact, scrollable verticalement

## 6. Critères d'acceptation

- [ ] Multi-fichier : un échec sur N ne tue plus les autres
- [ ] DataChannel ouvert + 0 byte → cleanup en 10 s avec message clair
- [ ] Fichier tronqué (size mismatch) jamais téléchargé
- [ ] Drain final 30 s max
- [ ] Émetteur voit la progression réelle du receveur (ack)
- [ ] Disconnect transitoire (cut Wi-Fi 5 s puis revenir) → transfert
      reprend
- [ ] A→B et B→A simultanés fonctionnent
- [ ] Liste P2P persiste à travers F5 (métadonnées seulement)
- [ ] Bouton Réessayer relance uniquement les ✗
- [ ] Toast + son + vibration à chaque complétion
- [ ] Tabs Host/P2P séparent les deux flux dans la footer
- [ ] V1.2 nominal flow inchangé (pas de régression)
- [ ] 14/14 tests existants passent
