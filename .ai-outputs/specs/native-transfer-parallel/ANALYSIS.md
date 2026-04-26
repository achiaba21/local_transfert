# Native Transfer — Parallel chunks + performance overhaul

> Document de suivi / roadmap pour l'amélioration du flow TCP LTR1
> app-to-app. À lire en début de session pour reprendre le contexte.

**Créé :** 2026-04-24
**Scope :** couche `ltr::network` (TransferClient, TransferServer, protocol)
**État :** analyse validée, en attente de décisions préalables avant
implémentation.

---

## 📊 État des phases

| Phase | Titre | Statut | Gain attendu | Effort |
|-------|-------|--------|--------------|--------|
| 0 | Benchmark baseline (avant/après) | ⏳ à faire | mesure | 1-2 j |
| 1 | Parallélisme multi-fichiers (N conns) | ⏳ à venir | 3-4× batches | 10-15 j |
| 2 | Parallélisme intra-fichier (ranges + pwrite) | ⏳ à venir | 2-3× gros Wi-Fi | 10-15 j |
| 3 | Multi-destinataires avec fanout | ⏳ à venir | I/O × 1 au lieu de × N | 5-7 j |
| 4 | Optimisations OS (sendfile / TransmitFile) | ⏳ à venir | 10-20% | 3-5 j |

---

## 1. Contexte et baseline

### Flow actuel TCP LTR1

- Port TCP 45455, protocole `LTR1` (magic + type + len BE + payload)
- Messages : `Offer` (JSON tous fichiers) → `Accept/Reject` → pour chaque
  fichier `FileHeader + FileChunk × N + FileEnd` → `Done`
- Chunks **256 KB** (`core::kChunkSize`)
- Hash SHA256 final (picosha2)
- **1 seule connexion TCP par session**, fichiers envoyés
  **séquentiellement**
- Pas de resume, pas de retry, pas de parallélisme multi-cible
- `FilesystemService::enumerate(inputs)` walk récursif complet AVANT
  d'ouvrir la socket

### Points d'amélioration retenus par l'utilisateur

- **B** Latence de démarrage (pipeliner scan + offer + streaming)
- **C** Multi-destinataires en parallèle (lecture unique + fanout)
- **D** Resume après coupure
- **E** Progression par fichier + totale
- **G** Retry auto
- **I** Offer streaming (gros catalogues)
- **Ajout utilisateur :** envoyer 1 fichier en N morceaux simultanés,
  reconstruits côté récepteur

---

## 2. Est-ce que le parallélisme aide sur LAN gigabit ?

Gain réel selon scénario :

| Scénario | Gain | Raison |
|----------|------|--------|
| Ethernet gigabit filaire, SSD↔SSD | **0-15 %** | 1 flux TCP sature déjà ~110 Mo/s |
| Wi-Fi 5/6 congestionné, SSD↔SSD | **2-3×** | Wi-Fi cap le per-flow throughput |
| Wi-Fi mesh / répéteurs | **2-4×** | Latence variable masquée par pipeline |
| HDD↔HDD | **Négatif** | Seek thrashing : 4 lecteurs = disque qui cherche |
| Batch 1000 × 10 Mo | **3-4×** | Latence round-trip par fichier absorbée |
| 1 fichier énorme, Wi-Fi | **2-3×** | Pipe non saturé |

**Implication :** le parallélisme intra-fichier (ranges + pwrite) n'est
utile que pour **Wi-Fi + gros fichier**. Le parallélisme multi-fichiers
(Phase 1) est le meilleur ratio effort/gain pour le cas commun.

---

## 3. Design du parallélisme intra-fichier (Phase 2)

### 3.1 Principe

- Fichier F de taille S divisé en K **ranges** de ~M Mo (ex M=4 → 1 Go = 250 ranges)
- N workers sender (ex N=4) tirent des ranges depuis une file partagée
- Chaque worker = sa propre connexion TCP
- Chaque range : `{fileId, offset, length, data, crc32}`
- Receiver écrit via **`pwrite`** (atomique thread-safe OS-level) à
  l'offset exact
- Fichier préalloué via `ftruncate(S)` côté receiver
- Bitmap des ranges reçues pour détecter complétion

### 3.2 Nouveau protocole LTR1.1 (backward-compat)

Négociation dans `OFFER` :
```json
{
  "protocol": "LTR1.1",
  "features": ["parallel", "resume", "retry"],
  "suggestedConnections": 4,
  "suggestedRangeMB": 4,
  "files": [...]
}
```

Réponse `ACCEPT` liste les features acceptées. Absence de `"protocol"` →
fallback LTR1 legacy (1 connexion, séquentiel).

### 3.3 Nouveaux messages

| Type | Sens | Payload |
|------|------|---------|
| `SessionParams` | C→S | params finaux négociés |
| `ConnectionHello` | C→S (conns 2..N) | `{sessionId, connectionId}` |
| `RangeStart` | C→S | `{fileId, offset, length}` |
| `RangeData` | C→S | `length` octets bruts |
| `RangeEnd` | C→S | `crc32` |
| `RangeAck` | S→C | `{fileId, offset, status}` |
| `RangeRequest` (resume) | S→C | `{fileId, missingRanges}` |

### 3.4 Concurrency

**Sender :**
```
SessionSender {
  FileQueue files;
  RangeQueue ranges;
  N workers (thread + socket + 4 MB buf)
  ProgressAggregator
}

Worker:
  while running:
    Range r = ranges.steal();
    if none: block on FileQueue.nextFile()
    pread(fd, buf, r.size, r.offset);
    crc32(buf);
    send RangeStart + RangeData + RangeEnd;
    wait RangeAck;
    if bad_crc: retry 3x;
    commit progress;
```

**Receiver :**
```
SessionReceiver {
  file_fd (pwrite OK concurrent)
  RangeBitmap bitmap;
  mutex m;  // bitmap + progress
  N acceptor threads
}

Acceptor:
  read RangeStart / RangeData / RangeEnd;
  verify crc32;
  pwrite(file_fd, data, length, offset);  // OS-atomic
  lock; bitmap.set(offset, length); unlock;
  send RangeAck;
  if bitmap.complete(fileId):
    fsync; SHA256 verify optional; emit TransferDoneEvent per file;
```

### 3.5 Range size tradeoff

| Taille | Overhead réseau | Imbalance workers | RAM/worker |
|--------|-----------------|-------------------|-----------|
| 256 KB | 🔴 élevé | 🟢 très fin | 🟢 minimal |
| 4 MB | 🟢 faible | 🟡 OK si >100 MB | 🟡 4 MB × N |
| 16 MB | 🟢 très faible | 🔴 dernier worker fait toute la fin | 🔴 16 MB × N |

**Recommandation :** 4 MB par range, adaptatif selon taille fichier.
Fichier < 4 MB → range unique (1 worker).

### 3.6 Thread-safety `pwrite`

- **POSIX** (macOS/Linux) : `pwrite(fd, buf, n, offset)` atomique par
  appel, pas de lock userspace
- **Windows** : `WriteFile` + `OVERLAPPED{offset}` équivalent
- Wrapper RAII : `ltr::infra::RandomAccessFile::pwriteAt(buf, n, off)`

### 3.7 Préallocation

Avant premier range : `ftruncate(fd, totalSize)` (POSIX) /
`SetFilePointer+SetEndOfFile` (Windows). Empêche fragmentation +
réserve l'espace disque.

Bonus Linux : `posix_fallocate` pour préallocation **physique**
(zéro risque `ENOSPC` en cours).

---

## 4. Intégration des autres axes

### Axe B — Latence démarrage (pipeliner scan + offer + streaming)

**Aujourd'hui :** `FilesystemService::enumerate(inputs)` walk TOUT avant
la socket. 10 000 fichiers = 3-5 s freeze avant 1er octet.

**Propo :** scanner par **chunks de 100 fichiers**, streamer un
`FileBatchOffer` dès le 1er chunk, continuer au fil du walk. Récepteur
peut commencer à accepter des ranges dès le 1er fichier.

Nouveau flow OFFER :
- `OfferHeader` (metadata globale)
- N × `FileBatchOffer` (100 files/chunk)
- `OfferComplete`

UX : récepteur peut afficher « Préparation… fichier 500/? » pendant
phase de scan sender.

### Axe C — Multi-destinataires (fanout lecture unique)

**Aujourd'hui :** envoi vers 3 Macs = 3 `TransferClient::send()` qui
relisent chacun le fichier → I/O × 3.

**Propo :** thread lecteur unique + N queues destinataires. Chaque
destinataire a son pool de connexions parallèles (axe intra-fichier).

**Challenge :** destinataires n'avancent pas au même rythme → buffer
circulaire par destinataire (ex 64 MB), le reader bloque seulement
quand **tous** les buffers sont full.

### Axe D — Resume

**État receiver :** `.ltr-resume-<sessionId>.json` à côté du partial,
contenant bitmap des ranges reçues + hash des N premiers octets du
source (détection modif source).

**Reconnexion :**
1. Sender retente (backoff 1s/2s/4s/max 30s)
2. Après ACCEPT, receiver envoie `RangeRequest{missingRanges}`
3. Sender skip ce qui est déjà chez le receiver

Nécessite `sessionId` **déterministe** (pas random) : hash(source_paths
+ receiver_id).

### Axe E — Progression par fichier + totale

**UiTransfer étendu :**
```cpp
struct UiTransfer {
  // existant
  std::uint64_t totalBytes;
  std::uint64_t bytesTransferred;

  // V1.1.9
  int filesTotal;
  int filesCompleted;
  std::string currentFileName;
  std::uint64_t currentFileTransferred;
  std::uint64_t currentFileTotal;
};
```

Card transfert : 2 barres empilées (globale + fichier courant) ou
texte `"Fichier 3/12 · photo.jpg 45 %"` + barre globale.

### Axe G — Retry auto

- **Granularité :** par range, pas par fichier
- **Stratégie :** 3 tentatives avec backoff (100 ms / 500 ms / 2 s) sur
  même conn. Si échec persistant sur conn X → re-schedule sur worker Y
  (work-stealing naturel)
- **Hash mismatch :** retry silencieux. 3 échecs → `TransferFailedEvent
  {fileId, "crc_mismatch"}` + fichier failed mais les autres continuent

### Axe I — Offer streaming (couplé B)

Déjà couvert par le redécoupage OFFER en OfferHeader + N ×
FileBatchOffer + OfferComplete.

---

## 5. Compatibilité ascendante

| Pair reçu OFFER | Comportement |
|-----------------|--------------|
| Sans `"protocol"` (legacy) | Envoi séquentiel classique, 1 conn |
| `"protocol": "LTR1.1"` | Négocie features communes |
| Inconnu | Fallback LTR1 |

Sender détecte via ACCEPT payload (feature list).

---

## 6. Décisions préalables à prendre AVANT de coder

1. **Commence par Phase 1 seule ou pipeline complet ?**
   → Recommandation : **Phase 0 (benchmark) + Phase 1** puis valider
   les gains avec metrics avant d'attaquer Phase 2/3.

2. **Valeurs par défaut** :
   - N connexions = **4** ?
   - Range size = **4 MB** ?
   - Seuil intra-fichier = **100 MB** (sinon range unique) ?

3. **Format `.ltr-resume`** : JSON (debuggable) ou binaire (rapide) ?
   → Recommandation : JSON (partie metadata) + bitmap binaire séparé
   pour éviter de parser N KB de bitmap en JSON.

4. **UI mode parallèle** :
   - Auto-détection (Wi-Fi → N=4, Ethernet → N=1 ou 2) ?
   - Toggle settings ?
   - Toujours parallèle avec N adaptatif ?
   → Recommandation : auto avec N adaptatif + log de décision.

5. **Phase 3 multi-destinataires** souhaité ou suffisant avec N×conns
   par cible ?

6. **Benchmark protocol** : scénarios test ?
   - 1 Go Ethernet SSD↔SSD
   - 1 Go Wi-Fi SSD↔SSD
   - 1000 × 10 Mo batch
   - 1 Go HDD↔HDD (cas défavorable — désactiver intra-fichier ?)
   - Resume après kill -9 à 50 %

7. **Détection HDD vs SSD** : comment ?
   - macOS : `IOKit` / `diskutil info` (lourd)
   - Linux : `/sys/block/X/queue/rotational`
   - Windows : `DeviceIoControl + STORAGE_DEVICE_DESCRIPTOR`
   → Alternative : **benchmark I/O rapide au démarrage** (10 reads
   random → si latence > 5 ms = HDD)

---

## 7. Risques & tradeoffs

| Risque | Probabilité | Impact | Atténuation |
|--------|-------------|--------|-------------|
| Zéro gain LAN filaire gigabit | Haute | Faible | Tester avant livraison, documenter gain Wi-Fi |
| Complexité protocole × 3 | Haute | Moyen | Séparer LTR1 legacy / LTR1.1 dans code |
| Bugs pwrite/bitmap (fichiers corrompus) | Moyenne | Très haut | Tests unitaires intensifs + SHA256 final |
| Disk HDD → pire qu'avant | Basse | Moyen | Détecter fs, désactiver intra-fichier sur HDD |
| Résume casse si source changé | Basse | Haut | Hash N premiers octets dans `.ltr-resume` → invalidate |
| N conns × firewall soupçon | Basse | Faible | Garder port unique 45455 |

---

## 8. Plan d'implémentation proposé

### Phase 0 — Benchmark baseline (avant tout)
- Script test qui mesure throughput sur :
  - Ethernet 1 Go
  - Wi-Fi 1 Go
  - Batch 1000 × 10 Mo
- Sortie : CSV avec débit Mo/s + temps total
- Objectif : valider la théorie § 2

### Phase 1 — Parallélisme multi-fichiers (10-15 j)
- N connexions TCP persistantes par session (N=4 default)
- Work queue de fichiers (1 file par worker à la fois)
- Pas de pwrite, pas de bitmap
- Axes B (offer streaming), E (progression détaillée), I (offer chunked)
- Tests unitaires + smoke perf tests

### Phase 2 — Parallélisme intra-fichier (10-15 j)
- Ranges + pwrite + bitmap
- Axe D (resume) via bitmap persisté
- Axe G (retry auto par range)
- Seuil 100 MB (sinon range unique)
- Détection HDD → range unique aussi

### Phase 3 — Multi-destinataires fanout (5-7 j)
- Reader unique + N queues destinataires
- Backpressure coordonné
- Utile si envoi à ≥ 2 cibles

### Phase 4 — Optimisations OS (3-5 j)
- `sendfile(2)` / `TransmitFile` zero-copy
- `posix_fallocate` préallocation physique
- Tests benchmark avant/après

---

## 9. Questions ouvertes

Voir § 6 pour la liste des 7 décisions préalables à trancher avant de
lancer `/feature full`.

---

## Journal

### 2026-04-24
- Analyse initiale livrée
- Axes sélectionnés par l'utilisateur : B, C, D, E, G, I + parallélisme
  intra-fichier
- Document créé pour reprendre plus tard après le sprint UX-5 Confort
